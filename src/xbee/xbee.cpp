#include "xbee/xbee.hpp"

#include "core/byte_codec.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <utility>

struct ActiveTraceCallback
{
    const XBee *owner;
    const ActiveTraceCallback *previous;
};

thread_local const ActiveTraceCallback *activeTraceCallback = nullptr;

bool isTraceCallbackActiveFor(const XBee *xbee)
{
    const ActiveTraceCallback *current = activeTraceCallback;
    while (current != nullptr)
    {
        if (current->owner == xbee)
        {
            return true;
        }
        current = current->previous;
    }
    return false;
}
enum class ParseStatus : uint8_t
{
    none,
    frameReady,
    checksumError,
    frameTooLarge
};

struct FrameView
{
    const uint8_t *data{};
    std::size_t size{};
};

uint8_t calculateChecksum(const uint8_t *frameData, const std::size_t size)
{
    uint16_t sum = 0;
    for (std::size_t i = 0; i < size; ++i)
    {
        sum += frameData[i];
    }
    return static_cast<uint8_t>(0xFF - (sum & 0xFF));
}

bool buildFrame(std::vector<uint8_t> &output, const std::vector<uint8_t> &frameData)
{
    output.clear();
    if (frameData.size() > 0xFFFF)
    {
        return false;
    }

    output.reserve(frameData.size() + 4);
    output.push_back(startDelimiter);
    output.push_back(static_cast<uint8_t>(frameData.size() >> 8));
    output.push_back(static_cast<uint8_t>(frameData.size()));
    output.insert(output.end(), frameData.begin(), frameData.end());
    output.push_back(calculateChecksum(frameData.data(), frameData.size()));
    return true;
}

template <std::size_t FrameCapacity>
class FrameParser
{
public:
    ParseStatus process(const uint8_t byte, FrameView &frame)
    {
        frame = {};

        switch (_state)
        {
        case State::waitStart:
            if (byte == startDelimiter)
            {
                _length = 0;
                _size = 0;
                _state = State::readLengthMsb;
            }
            return ParseStatus::none;

        case State::readLengthMsb:
            _length = static_cast<uint16_t>(byte) << 8;
            _state = State::readLengthLsb;
            return ParseStatus::none;

        case State::readLengthLsb:
            _length |= byte;
            _size = 0;
            if (_length > FrameCapacity)
            {
                reset();
                return ParseStatus::frameTooLarge;
            }
            _state = _length == 0 ? State::readChecksum : State::readFrameData;
            return ParseStatus::none;

        case State::readFrameData:
            _buffer[_size++] = byte;
            if (_size >= _length)
            {
                _state = State::readChecksum;
            }
            return ParseStatus::none;

        case State::readChecksum:
            if (calculateChecksum(_buffer, _length) != byte)
            {
                reset();
                return ParseStatus::checksumError;
            }

            frame.data = _buffer;
            frame.size = _length;
            reset();
            return ParseStatus::frameReady;
        }

        reset();
        return ParseStatus::none;
    }

    void reset()
    {
        _state = State::waitStart;
        _length = 0;
        _size = 0;
    }

private:
    enum class State : uint8_t
    {
        waitStart,
        readLengthMsb,
        readLengthLsb,
        readFrameData,
        readChecksum
    };

    uint8_t _buffer[FrameCapacity]{};
    uint16_t _length{};
    std::size_t _size{};
    State _state{State::waitStart};
};

XBee::~XBee()
{
    close();
}

uint8_t XBee::open(const std::string &portName, const uint32_t baudRate, const ApiMode apiMode)
{
    close();
    if (_parserThread.joinable())
    {
        return 1;
    }

    if (_serialPort.open(portName, baudRate) != 0)
    {
        return 1;
    }

    {
        std::lock_guard<std::mutex> outputLock(_outputPayloadMutex);
        _outputPayload.clear();

        std::lock_guard<std::mutex> payloadLock(_parsedPayloadMutex);
        _parsedPayloads.clear();
        _parsedPayloadWaitInterrupted = false;
    }

    {
        std::lock_guard<std::mutex> responseLock(_atResponseMutex);
        _pendingAtResponse = false;
        _pendingAtResponseCompleted = false;
        _pendingAtFrameId = 0;
        _pendingAtCommand = 0;
        _pendingAtStatus = 0xFF;
        _pendingAtData.clear();
    }

    _clearFrameData();
    _apiMode = apiMode;
    _running = true;
    _parserThread = std::thread(&XBee::_parserLoop, this);
    return 0;
}

bool XBee::isOpen() const
{
    return _serialPort.isOpen();
}

void XBee::close()
{
    _running = false;
    interruptParsedInputPayloadWait();
    _atResponseCondition.notify_all();
    _serialPort.close();

    if (_parserThread.joinable() && std::this_thread::get_id() != _parserThread.get_id())
    {
        _parserThread.join();
    }
}

uint8_t XBee::openJoinWindow(const uint8_t seconds)
{
    const uint8_t nodeJoinResult = atCommandRequest(nj, seconds);
    if (nodeJoinResult != 0)
    {
        return nodeJoinResult;
    }

    return atCommandRequest(ac);
}

uint8_t XBee::closeJoinWindow()
{
    const uint8_t nodeJoinResult = atCommandRequest(nj, static_cast<uint8_t>(0));
    if (nodeJoinResult != 0)
    {
        return nodeJoinResult;
    }

    return atCommandRequest(ac);
}

bool XBee::readAtCommandData(const uint16_t atCommand, std::vector<uint8_t> &data, const uint32_t timeoutMs)
{
    uint8_t frameId = 0;
    {
        std::lock_guard<std::mutex> responseLock(_atResponseMutex);
        if (_pendingAtResponse)
        {
            return false;
        }

        frameId = _nextFrameIdForRequest();
        _pendingAtResponse = true;
        _pendingAtResponseCompleted = false;
        _pendingAtFrameId = frameId;
        _pendingAtCommand = atCommand;
        _pendingAtStatus = 0xFF;
        _pendingAtData.clear();
    }

    uint8_t sendResult = 0;
    std::vector<uint8_t> tracedOutput;
    {
        std::lock_guard<std::mutex> commandLock(_commandMutex);
        _clearFrameData();
        ::write8(_frameData, ::atCommandRequest);
        ::write8(_frameData, frameId);
        write16BigEndian(_frameData, atCommand);
        sendResult = _sendFrameData(tracedOutput);
    }
    _trace(TraceDirection::tx, tracedOutput.data(), tracedOutput.size());

    if (sendResult != 0)
    {
        std::lock_guard<std::mutex> responseLock(_atResponseMutex);
        _pendingAtResponse = false;
        _pendingAtResponseCompleted = false;
        _pendingAtFrameId = 0;
        _pendingAtCommand = 0;
        _pendingAtStatus = 0xFF;
        _pendingAtData.clear();
        return false;
    }

    std::unique_lock<std::mutex> responseLock(_atResponseMutex);
    const bool success = _atResponseCondition.wait_for(responseLock, std::chrono::milliseconds(timeoutMs), [this]
                                                       { return _pendingAtResponseCompleted || !_running; }) &&
                         _pendingAtResponseCompleted &&
                         _pendingAtStatus == 0;

    if (success)
    {
        data = _pendingAtData;
    }

    _pendingAtResponse = false;
    _pendingAtResponseCompleted = false;
    _pendingAtData.clear();
    return success;
}

bool XBee::readAtCommand8(const uint16_t atCommand, uint8_t &value, const uint32_t timeoutMs)
{
    std::vector<uint8_t> data;
    if (!readAtCommandData(atCommand, data, timeoutMs) || data.size() != 1)
    {
        return false;
    }

    value = read8(data);
    return true;
}

bool XBee::readAtCommand16(const uint16_t atCommand, uint16_t &value, const uint32_t timeoutMs)
{
    std::vector<uint8_t> data;
    if (!readAtCommandData(atCommand, data, timeoutMs) || data.size() != 2)
    {
        return false;
    }

    value = read16BigEndian(data);
    return true;
}

bool XBee::readAtCommand32(const uint16_t atCommand, uint32_t &value, const uint32_t timeoutMs)
{
    std::vector<uint8_t> data;
    if (!readAtCommandData(atCommand, data, timeoutMs) || data.size() != 4)
    {
        return false;
    }

    value = read32BigEndian(data);
    return true;
}

bool XBee::readAtCommand64(const uint16_t atCommand, uint64_t &value, const uint32_t timeoutMs)
{
    std::vector<uint8_t> data;
    if (!readAtCommandData(atCommand, data, timeoutMs) || data.size() != 8)
    {
        return false;
    }

    value = read64BigEndian(data);
    return true;
}

uint8_t XBee::atCommandRequest(const uint16_t atCommand)
{
    std::vector<uint8_t> tracedOutput;
    uint8_t result = 0;
    {
        std::lock_guard<std::mutex> commandLock(_commandMutex);
        _clearFrameData();
        ::write8(_frameData, ::atCommandRequest);
        ::write8(_frameData, defaultFrameId);
        write16BigEndian(_frameData, atCommand);
        result = _sendFrameData(tracedOutput);
    }
    _trace(TraceDirection::tx, tracedOutput.data(), tracedOutput.size());
    return result;
}

uint8_t XBee::atCommandRequest(const uint16_t atCommand, const uint8_t value)
{
    std::vector<uint8_t> tracedOutput;
    uint8_t result = 0;
    {
        std::lock_guard<std::mutex> commandLock(_commandMutex);
        _clearFrameData();
        ::write8(_frameData, ::atCommandRequest);
        ::write8(_frameData, defaultFrameId);
        write16BigEndian(_frameData, atCommand);
        ::write8(_frameData, value);
        result = _sendFrameData(tracedOutput);
    }
    _trace(TraceDirection::tx, tracedOutput.data(), tracedOutput.size());
    return result;
}

uint8_t XBee::atCommandRequest(const uint16_t atCommand, const uint16_t value)
{
    std::vector<uint8_t> tracedOutput;
    uint8_t result = 0;
    {
        std::lock_guard<std::mutex> commandLock(_commandMutex);
        _clearFrameData();
        ::write8(_frameData, ::atCommandRequest);
        ::write8(_frameData, defaultFrameId);
        write16BigEndian(_frameData, atCommand);
        write16BigEndian(_frameData, value);
        result = _sendFrameData(tracedOutput);
    }
    _trace(TraceDirection::tx, tracedOutput.data(), tracedOutput.size());
    return result;
}

uint8_t XBee::atCommandRequest(const uint16_t atCommand, const uint32_t value)
{
    std::vector<uint8_t> tracedOutput;
    uint8_t result = 0;
    {
        std::lock_guard<std::mutex> commandLock(_commandMutex);
        _clearFrameData();
        ::write8(_frameData, ::atCommandRequest);
        ::write8(_frameData, defaultFrameId);
        write16BigEndian(_frameData, atCommand);
        write32BigEndian(_frameData, value);
        result = _sendFrameData(tracedOutput);
    }
    _trace(TraceDirection::tx, tracedOutput.data(), tracedOutput.size());
    return result;
}

uint8_t XBee::atCommandRequest(const uint16_t atCommand, const uint64_t value)
{
    std::vector<uint8_t> tracedOutput;
    uint8_t result = 0;
    {
        std::lock_guard<std::mutex> commandLock(_commandMutex);
        _clearFrameData();
        ::write8(_frameData, ::atCommandRequest);
        ::write8(_frameData, defaultFrameId);
        write16BigEndian(_frameData, atCommand);
        write64BigEndian(_frameData, value);
        result = _sendFrameData(tracedOutput);
    }
    _trace(TraceDirection::tx, tracedOutput.data(), tracedOutput.size());
    return result;
}

uint8_t XBee::remoteAtCommandRequest(
    const uint64_t destinationXbee64Id,
    const uint16_t destinationXbee16Id,
    const uint16_t atCommand)
{
    std::vector<uint8_t> tracedOutput;
    uint8_t result = 0;
    {
        std::lock_guard<std::mutex> lock(_commandMutex);
        _clearFrameData();
        ::write8(_frameData, ::remoteAtCommandRequest);
        ::write8(_frameData, defaultFrameId);
        write64BigEndian(_frameData, destinationXbee64Id);
        write16BigEndian(_frameData, destinationXbee16Id);
        ::write8(_frameData, remoteCommandOptionsApplyChanges);
        write16BigEndian(_frameData, atCommand);
        result = _sendFrameData(tracedOutput);
    }
    _trace(TraceDirection::tx, tracedOutput.data(), tracedOutput.size());
    return result;
}

uint8_t XBee::remoteAtCommandRequest(
    const uint64_t destinationXbee64Id,
    const uint16_t destinationXbee16Id,
    const uint16_t atCommand,
    const uint8_t value)
{
    std::vector<uint8_t> tracedOutput;
    uint8_t result = 0;
    {
        std::lock_guard<std::mutex> lock(_commandMutex);
        _clearFrameData();
        ::write8(_frameData, ::remoteAtCommandRequest);
        ::write8(_frameData, defaultFrameId);
        write64BigEndian(_frameData, destinationXbee64Id);
        write16BigEndian(_frameData, destinationXbee16Id);
        ::write8(_frameData, remoteCommandOptionsApplyChanges);
        write16BigEndian(_frameData, atCommand);
        ::write8(_frameData, value);
        result = _sendFrameData(tracedOutput);
    }
    _trace(TraceDirection::tx, tracedOutput.data(), tracedOutput.size());
    return result;
}

uint8_t XBee::remoteAtCommandRequest(
    const uint64_t destinationXbee64Id,
    const uint16_t destinationXbee16Id,
    const uint16_t atCommand,
    const uint16_t value)
{
    std::vector<uint8_t> tracedOutput;
    uint8_t result = 0;
    {
        std::lock_guard<std::mutex> lock(_commandMutex);
        _clearFrameData();
        ::write8(_frameData, ::remoteAtCommandRequest);
        ::write8(_frameData, defaultFrameId);
        write64BigEndian(_frameData, destinationXbee64Id);
        write16BigEndian(_frameData, destinationXbee16Id);
        ::write8(_frameData, remoteCommandOptionsApplyChanges);
        write16BigEndian(_frameData, atCommand);
        write16BigEndian(_frameData, value);
        result = _sendFrameData(tracedOutput);
    }
    _trace(TraceDirection::tx, tracedOutput.data(), tracedOutput.size());
    return result;
}

uint8_t XBee::remoteAtCommandRequest(
    const uint64_t destinationXbee64Id,
    const uint16_t destinationXbee16Id,
    const uint16_t atCommand,
    const uint32_t value)
{
    std::vector<uint8_t> tracedOutput;
    uint8_t result = 0;
    {
        std::lock_guard<std::mutex> lock(_commandMutex);
        _clearFrameData();
        ::write8(_frameData, ::remoteAtCommandRequest);
        ::write8(_frameData, defaultFrameId);
        write64BigEndian(_frameData, destinationXbee64Id);
        write16BigEndian(_frameData, destinationXbee16Id);
        ::write8(_frameData, remoteCommandOptionsApplyChanges);
        write16BigEndian(_frameData, atCommand);
        write32BigEndian(_frameData, value);
        result = _sendFrameData(tracedOutput);
    }
    _trace(TraceDirection::tx, tracedOutput.data(), tracedOutput.size());
    return result;
}

uint8_t XBee::remoteAtCommandRequest(
    const uint64_t destinationXbee64Id,
    const uint16_t destinationXbee16Id,
    const uint16_t atCommand,
    const uint64_t value)
{
    std::vector<uint8_t> tracedOutput;
    uint8_t result = 0;
    {
        std::lock_guard<std::mutex> lock(_commandMutex);
        _clearFrameData();
        ::write8(_frameData, ::remoteAtCommandRequest);
        ::write8(_frameData, defaultFrameId);
        write64BigEndian(_frameData, destinationXbee64Id);
        write16BigEndian(_frameData, destinationXbee16Id);
        ::write8(_frameData, remoteCommandOptionsApplyChanges);
        write16BigEndian(_frameData, atCommand);
        write64BigEndian(_frameData, value);
        result = _sendFrameData(tracedOutput);
    }
    _trace(TraceDirection::tx, tracedOutput.data(), tracedOutput.size());
    return result;
}

uint8_t XBee::transmitRequest(
    const uint64_t destinationXbee64Id,
    const uint16_t destinationXbee16Id,
    const uint8_t broadcastRadius,
    const uint8_t options)
{
    std::vector<uint8_t> payload;
    {
        std::lock_guard<std::mutex> lock(_outputPayloadMutex);
        payload.swap(_outputPayload);
    }

    return transmitRequest(destinationXbee64Id, destinationXbee16Id, payload, broadcastRadius, options);
}

uint8_t XBee::transmitRequest(
    const uint64_t destinationXbee64Id,
    const uint16_t destinationXbee16Id,
    const std::vector<uint8_t> &payload,
    const uint8_t broadcastRadius,
    const uint8_t options)
{
    std::vector<uint8_t> tracedOutput;
    uint8_t result = 0;
    {
        std::lock_guard<std::mutex> lock(_commandMutex);
        _clearFrameData();
        ::write8(_frameData, ::transmitRequest);
        ::write8(_frameData, defaultFrameId);
        write64BigEndian(_frameData, destinationXbee64Id);
        write16BigEndian(_frameData, destinationXbee16Id);
        ::write8(_frameData, broadcastRadius);
        ::write8(_frameData, options);
        _frameData.insert(_frameData.end(), payload.begin(), payload.end());
        result = _sendFrameData(tracedOutput);
    }
    _trace(TraceDirection::tx, tracedOutput.data(), tracedOutput.size());
    return result;
}

void XBee::clearOutputPayload()
{
    std::lock_guard<std::mutex> lock(_outputPayloadMutex);
    _outputPayload.clear();
}

void XBee::write8(const uint8_t input)
{
    std::lock_guard<std::mutex> lock(_outputPayloadMutex);
    ::write8(_outputPayload, input);
}

void XBee::write16(const uint16_t input)
{
    std::lock_guard<std::mutex> lock(_outputPayloadMutex);
    write16BigEndian(_outputPayload, input);
}

void XBee::write32(const uint32_t input)
{
    std::lock_guard<std::mutex> lock(_outputPayloadMutex);
    write32BigEndian(_outputPayload, input);
}

void XBee::write64(const uint64_t input)
{
    std::lock_guard<std::mutex> lock(_outputPayloadMutex);
    write64BigEndian(_outputPayload, input);
}

std::vector<ReceivedXBeeFrame> XBee::getParsedInputPayload()
{
    std::lock_guard<std::mutex> lock(_parsedPayloadMutex);
    std::vector<ReceivedXBeeFrame> payloads;
    payloads.swap(_parsedPayloads);
    return payloads;
}

std::vector<ReceivedXBeeFrame> XBee::waitAndTakeParsedInputPayload(const uint32_t timeoutMs)
{
    std::unique_lock<std::mutex> lock(_parsedPayloadMutex);
    const auto canStopWaiting = [this]
    {
        return !_parsedPayloads.empty() || !_running || _parsedPayloadWaitInterrupted;
    };

    if (timeoutMs == 0)
    {
        _parsedPayloadCondition.wait(lock, canStopWaiting);
    }
    else
    {
        _parsedPayloadCondition.wait_for(lock, std::chrono::milliseconds(timeoutMs), canStopWaiting);
    }

    std::vector<ReceivedXBeeFrame> payloads;
    payloads.swap(_parsedPayloads);
    _parsedPayloadWaitInterrupted = false;
    return payloads;
}

void XBee::interruptParsedInputPayloadWait()
{
    {
        std::lock_guard<std::mutex> lock(_parsedPayloadMutex);
        _parsedPayloadWaitInterrupted = true;
    }
    _parsedPayloadCondition.notify_all();
}

void XBee::setTraceCallback(const XBeeTraceCallback callback, void *userData)
{
    std::unique_lock<std::mutex> lock(_traceMutex);
    _traceCallback = callback;
    _traceUserData = userData;

    if (callback == nullptr)
    {
        if (isTraceCallbackActiveFor(this))
        {
            return;
        }

        _traceCondition.wait(lock, [this]
                             { return _activeTraceCallbacks == 0; });
    }
}

void XBee::_clearFrameData()
{
    _frameData.clear();
}

uint8_t XBee::_nextFrameIdForRequest()
{
    if (_nextFrameId == 0 || _nextFrameId == 0xFF)
    {
        _nextFrameId = 1;
    }

    const uint8_t frameId = _nextFrameId++;
    if (_nextFrameId == 0xFF)
    {
        _nextFrameId = 1;
    }
    return frameId;
}

uint8_t XBee::_sendFrameData(std::vector<uint8_t> &tracedOutput)
{
    tracedOutput.clear();

    std::vector<uint8_t> frame;
    if (!buildFrame(frame, _frameData))
    {
        return 1;
    }

    std::vector<uint8_t> output;
    output.reserve(frame.size() * 2);
    output.push_back(startDelimiter);
    for (std::size_t i = 1; i < frame.size(); ++i)
    {
        _appendEscapedByte(output, frame[i]);
    }

    _serialPort.clearOutputBuffer();
    for (const uint8_t byte : output)
    {
        _serialPort.write8(byte);
    }

    const uint32_t writtenSize = _serialPort.send();
    tracedOutput.swap(output);
    return writtenSize == tracedOutput.size() ? 0 : 1;
}

void XBee::_appendEscapedByte(std::vector<uint8_t> &output, const uint8_t byte) const
{
    if (_apiMode == ApiMode::api2 &&
        (byte == startDelimiter || byte == escape || byte == xon || byte == xoff))
    {
        output.push_back(escape);
        output.push_back(static_cast<uint8_t>(byte ^ 0x20));
        return;
    }

    output.push_back(byte);
}

void XBee::_trace(const TraceDirection direction, const uint8_t *bytes, const std::size_t size)
{
    XBeeTraceCallback callback = nullptr;
    void *userData = nullptr;
    {
        std::unique_lock<std::mutex> lock(_traceMutex);
        callback = _traceCallback;
        userData = _traceUserData;

        if (callback == nullptr || bytes == nullptr || size == 0)
        {
            return;
        }

        ++_activeTraceCallbacks;
    }

    const ActiveTraceCallback currentTraceCallback{this, activeTraceCallback};
    activeTraceCallback = &currentTraceCallback;

    callback(direction, bytes, size, userData);

    activeTraceCallback = currentTraceCallback.previous;

    {
        std::unique_lock<std::mutex> lock(_traceMutex);
        --_activeTraceCallbacks;
        _traceCondition.notify_all();
    }
}

void XBee::_parserLoop()
{
    FrameParser<1024> parser;
    bool insideFrame = false;
    bool escapeNext = false;

    const auto resetParserState = [&parser, &insideFrame, &escapeNext]
    {
        parser.reset();
        insideFrame = false;
        escapeNext = false;
    };

    while (_running)
    {
        if (!_serialPort.isOpen())
        {
            resetParserState();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (!_serialPort.waitForInputSize(1, 100))
        {
            if (!_serialPort.isOpen())
            {
                resetParserState();
            }
            continue;
        }

        while (_running && _serialPort.bytesToRead() > 0)
        {
            uint8_t byte = _serialPort.read8();

            if (!insideFrame)
            {
                if (byte != startDelimiter)
                {
                    continue;
                }

                parser.reset();
                insideFrame = true;
                escapeNext = false;
            }
            else if (_apiMode == ApiMode::api2 && byte == startDelimiter)
            {
                parser.reset();
                insideFrame = true;
                escapeNext = false;
            }
            else if (_apiMode == ApiMode::api2 && insideFrame)
            {
                if (escapeNext)
                {
                    byte = static_cast<uint8_t>(byte ^ 0x20);
                    escapeNext = false;
                }
                else if (byte == escape)
                {
                    escapeNext = true;
                    continue;
                }
            }

            FrameView frame;
            const ParseStatus status = parser.process(byte, frame);
            if (status == ParseStatus::checksumError || status == ParseStatus::frameTooLarge)
            {
                insideFrame = false;
                escapeNext = false;
                continue;
            }

            if (status != ParseStatus::frameReady || frame.size == 0)
            {
                continue;
            }

            insideFrame = false;
            escapeNext = false;

            const uint8_t frameType = frame.data[0];
            const std::vector<uint8_t> frameData(frame.data, frame.data + frame.size);
            std::vector<uint8_t> rawFrame;
            if (buildFrame(rawFrame, frameData))
            {
                _trace(TraceDirection::rx, rawFrame.data(), rawFrame.size());
            }
            else
            {
                _trace(TraceDirection::rx, frame.data, frame.size);
            }

            if (frameType == atCommandResponse && frame.size >= 5)
            {
                AtCommandResponse response;
                response.frameId = frame.data[1];
                response.atCommand = read16BigEndian(frameData, 2);
                response.status = frame.data[4];
                response.value.assign(frame.data + 5, frame.data + frame.size);

                {
                    std::lock_guard<std::mutex> responseLock(_atResponseMutex);
                    if (_pendingAtResponse &&
                        _pendingAtFrameId == response.frameId &&
                        _pendingAtCommand == response.atCommand)
                    {
                        _pendingAtStatus = response.status;
                        _pendingAtData = std::move(response.value);
                        _pendingAtResponseCompleted = true;
                        _atResponseCondition.notify_all();
                    }
                }

                continue;
            }

            if (frameType == receivePacket && frame.size >= 12)
            {
                ReceivedXBeeFrame payload;
                payload.xbee64Id = read64BigEndian(frameData, 1);
                payload.xbee16Id = read16BigEndian(frameData, 9);
                payload.receiveOptions = frame.data[11];
                payload.payload.assign(frame.data + 12, frame.data + frame.size);
                {
                    std::lock_guard<std::mutex> payloadLock(_parsedPayloadMutex);
                    _parsedPayloads.push_back(std::move(payload));
                }
                _parsedPayloadCondition.notify_one();
                continue;
            }

            if (frameType == explicitReceiveIndicator && frame.size >= 18)
            {
                ReceivedXBeeFrame payload;
                payload.xbee64Id = read64BigEndian(frameData, 1);
                payload.xbee16Id = read16BigEndian(frameData, 9);
                payload.receiveOptions = frame.data[17];
                payload.payload.assign(frame.data + 18, frame.data + frame.size);
                {
                    std::lock_guard<std::mutex> payloadLock(_parsedPayloadMutex);
                    _parsedPayloads.push_back(std::move(payload));
                }
                _parsedPayloadCondition.notify_one();
                continue;
            }
        }
    }
}
