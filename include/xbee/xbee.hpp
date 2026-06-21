#pragma once

#include "serial_port/serial_port.hpp"
#include "core/trace.hpp"
#include "xbee/constants.hpp"
#include "xbee/frames.hpp"

#include <atomic>
#include <cstddef>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace device_transport
{
    using XBeeTraceCallback = void (*)(TraceDirection direction, const uint8_t *bytes, size_t size, void *userData);

    class XBee
    {
    public:
        XBee() = default;
        ~XBee();

        TransportError open(const std::string &portName, uint32_t baudRate = 9600, api_frame::ApiMode apiMode = api_frame::ApiMode::api1);
        bool isOpen() const;
        void close();

        uint8_t openJoinWindow(uint8_t seconds = 60);
        uint8_t closeJoinWindow();

        bool readAtCommandData(uint16_t atCommand, std::vector<uint8_t> &data, uint32_t timeoutMs);
        bool readAtCommand8(uint16_t atCommand, uint8_t &value, uint32_t timeoutMs);
        bool readAtCommand16(uint16_t atCommand, uint16_t &value, uint32_t timeoutMs);
        bool readAtCommand32(uint16_t atCommand, uint32_t &value, uint32_t timeoutMs);
        bool readAtCommand64(uint16_t atCommand, uint64_t &value, uint32_t timeoutMs);

        uint8_t atCommandRequest(uint16_t atCommand);
        uint8_t atCommandRequest(uint16_t atCommand, uint8_t value);
        uint8_t atCommandRequest(uint16_t atCommand, uint16_t value);
        uint8_t atCommandRequest(uint16_t atCommand, uint32_t value);
        uint8_t atCommandRequest(uint16_t atCommand, uint64_t value);

        uint8_t remoteAtCommandRequest(uint64_t destinationXbee64Id, uint16_t destinationXbee16Id, uint16_t atCommand);
        uint8_t remoteAtCommandRequest(uint64_t destinationXbee64Id, uint16_t destinationXbee16Id, uint16_t atCommand, uint8_t value);
        uint8_t remoteAtCommandRequest(uint64_t destinationXbee64Id, uint16_t destinationXbee16Id, uint16_t atCommand, uint16_t value);
        uint8_t remoteAtCommandRequest(uint64_t destinationXbee64Id, uint16_t destinationXbee16Id, uint16_t atCommand, uint32_t value);
        uint8_t remoteAtCommandRequest(uint64_t destinationXbee64Id, uint16_t destinationXbee16Id, uint16_t atCommand, uint64_t value);

        uint8_t transmitRequest(
            uint64_t destinationXbee64Id,
            uint16_t destinationXbee16Id = xbee_address::unknownXbee16Id,
            uint8_t broadcastRadius = 0x00,
            uint8_t options = 0x00);
        uint8_t transmitRequest(
            uint64_t destinationXbee64Id,
            uint16_t destinationXbee16Id,
            const std::vector<uint8_t> &payload,
            uint8_t broadcastRadius = 0x00,
            uint8_t options = 0x00);

        void clearOutputPayload();

        void write8(uint8_t input);
        void write16(uint16_t input);
        void write32(uint32_t input);
        void write64(uint64_t input);

        std::vector<ReceivedXBeeFrame> getParsedInputPayload();
        std::vector<ReceivedXBeeFrame> waitAndTakeParsedInputPayload(uint32_t timeoutMs = 0);
        void interruptParsedInputPayloadWait();

        void setTraceCallback(XBeeTraceCallback callback, void *userData = nullptr);

    private:
        SerialPort _serialPort;

        std::atomic<bool> _running{false};
        std::thread _parserThread;

        std::vector<uint8_t> _frameData;
        std::vector<uint8_t> _outputPayload;
        std::vector<ReceivedXBeeFrame> _parsedPayloads;
        mutable std::mutex _outputPayloadMutex;
        mutable std::mutex _parsedPayloadMutex;
        std::condition_variable _parsedPayloadCondition;
        bool _parsedPayloadWaitInterrupted{};
        std::mutex _commandMutex;
        std::mutex _atResponseMutex;
        std::condition_variable _atResponseCondition;
        api_frame::ApiMode _apiMode{api_frame::ApiMode::api1};
        XBeeTraceCallback _traceCallback{};
        void *_traceUserData{};

        uint8_t _nextFrameId{1};
        bool _pendingAtResponse{};
        bool _pendingAtResponseCompleted{};
        uint8_t _pendingAtFrameId{};
        uint16_t _pendingAtCommand{};
        uint8_t _pendingAtStatus{0xFF};
        std::vector<uint8_t> _pendingAtData;

        void _clearFrameData();
        uint8_t _nextFrameIdForRequest();
        uint8_t _sendFrameData();

        void _appendEscapedByte(std::vector<uint8_t> &output, uint8_t byte) const;
        void _trace(TraceDirection direction, const uint8_t *bytes, size_t size) const;

        void _parserLoop();
    };
}
