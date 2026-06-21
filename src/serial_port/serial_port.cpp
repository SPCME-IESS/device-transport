#include "serial_port/serial_port.hpp"

#include "core/byte_codec.hpp"

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <thread>

namespace device_transport
{
    SerialPort::~SerialPort()
    {
        close();
    }

    TransportError SerialPort::open(const std::string &portName, const uint32_t baudRate)
    {
        close();
        _closing = false;

        if (portName.empty() || baudRate == 0)
        {
            return TransportError::invalidArgument;
        }

        _portName = portName;
        _baudRate = baudRate;

        const TransportError openResult = _openNativeHandle();
        if (openResult != TransportError::ok)
        {
            return openResult;
        }

        {
            std::lock_guard<std::mutex> inputLock(_inputMutex);
            _inputBuffer.clear();

            std::lock_guard<std::mutex> outputLock(_outputMutex);
            _outputBuffer.clear();
        }

        _running = true;
        _readerThread = std::thread(&SerialPort::_readerLoop, this);
        return TransportError::ok;
    }

    bool SerialPort::isOpen() const
    {
        std::lock_guard<std::mutex> lock(_nativeHandleMutex);
        return _nativeHandle != nullptr && !_connectionLost;
    }

    void SerialPort::close()
    {
        _closing = true;
        _running = false;
        _inputCondition.notify_all();

        _closeNativeHandle();

        if (_readerThread.joinable())
        {
            _readerThread.join();
        }
    }

    size_t SerialPort::bytesToRead() const
    {
        std::lock_guard<std::mutex> lock(_inputMutex);
        return _inputBuffer.size();
    }

    size_t SerialPort::bytesToWrite() const
    {
        std::lock_guard<std::mutex> lock(_outputMutex);
        return _outputBuffer.size();
    }

    uint32_t SerialPort::bytesInDriverQueue() const
    {
        std::lock_guard<std::mutex> lock(_nativeHandleMutex);
        if (_nativeHandle == nullptr || _connectionLost)
        {
            return 0;
        }

        DWORD errors = 0;
        COMSTAT status{};
        if (!ClearCommError(static_cast<HANDLE>(_nativeHandle), &errors, &status))
        {
            _markConnectionLost();
            return 0;
        }

        return static_cast<uint32_t>(status.cbInQue);
    }

    bool SerialPort::waitForInputSize(const size_t byteCount, const uint32_t timeoutMs)
    {
        std::unique_lock<std::mutex> lock(_inputMutex);
        if (timeoutMs == 0)
        {
            _inputCondition.wait(lock, [this, byteCount]
                                 { return _inputBuffer.size() >= byteCount || !_running || !isOpen(); });
            return _inputBuffer.size() >= byteCount;
        }

        return _inputCondition.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this, byteCount]
                                        { return _inputBuffer.size() >= byteCount || !_running || !isOpen(); }) &&
               _inputBuffer.size() >= byteCount;
    }

    uint8_t SerialPort::read8()
    {
        std::lock_guard<std::mutex> lock(_inputMutex);
        if (_inputBuffer.empty())
        {
            return 0;
        }

        const uint8_t value = byte_codec::read8(_inputBuffer);
        _inputBuffer.erase(_inputBuffer.begin());
        return value;
    }

    uint16_t SerialPort::read16()
    {
        std::lock_guard<std::mutex> lock(_inputMutex);
        if (_inputBuffer.size() < 2)
        {
            return 0;
        }

        const uint16_t value = byte_codec::read16(_inputBuffer);
        _inputBuffer.erase(_inputBuffer.begin(), _inputBuffer.begin() + 2);
        return value;
    }

    uint32_t SerialPort::read32()
    {
        std::lock_guard<std::mutex> lock(_inputMutex);
        if (_inputBuffer.size() < 4)
        {
            return 0;
        }

        const uint32_t value = byte_codec::read32(_inputBuffer);
        _inputBuffer.erase(_inputBuffer.begin(), _inputBuffer.begin() + 4);
        return value;
    }

    uint64_t SerialPort::read64()
    {
        std::lock_guard<std::mutex> lock(_inputMutex);
        if (_inputBuffer.size() < 8)
        {
            return 0;
        }

        const uint64_t value = byte_codec::read64(_inputBuffer);
        _inputBuffer.erase(_inputBuffer.begin(), _inputBuffer.begin() + 8);
        return value;
    }

    uint32_t SerialPort::write8(const uint8_t value)
    {
        std::lock_guard<std::mutex> lock(_outputMutex);
        byte_codec::write8(_outputBuffer, value);
        return 1;
    }

    uint32_t SerialPort::write16(const uint16_t value)
    {
        std::lock_guard<std::mutex> lock(_outputMutex);
        byte_codec::write16(_outputBuffer, value);
        return 2;
    }

    uint32_t SerialPort::write32(const uint32_t value)
    {
        std::lock_guard<std::mutex> lock(_outputMutex);
        byte_codec::write32(_outputBuffer, value);
        return 4;
    }

    uint32_t SerialPort::write64(const uint64_t value)
    {
        std::lock_guard<std::mutex> lock(_outputMutex);
        byte_codec::write64(_outputBuffer, value);
        return 8;
    }

    void SerialPort::clearInputBuffer()
    {
        std::lock_guard<std::mutex> lock(_inputMutex);
        _inputBuffer.clear();
    }

    void SerialPort::clearOutputBuffer()
    {
        std::lock_guard<std::mutex> lock(_outputMutex);
        _outputBuffer.clear();
    }

    std::vector<uint8_t> SerialPort::getInputBuffer() const
    {
        std::lock_guard<std::mutex> lock(_inputMutex);
        return _inputBuffer;
    }

    std::vector<uint8_t> SerialPort::getOutputBuffer() const
    {
        std::lock_guard<std::mutex> lock(_outputMutex);
        return _outputBuffer;
    }

    uint32_t SerialPort::send()
    {
        if (!isOpen())
        {
            return 0;
        }

        std::vector<uint8_t> bytes;
        {
            std::lock_guard<std::mutex> lock(_outputMutex);
            bytes.swap(_outputBuffer);
        }

        uint32_t totalWritten = 0;
        {
            std::lock_guard<std::mutex> nativeLock(_nativeHandleMutex);
            if (_nativeHandle == nullptr || _connectionLost)
            {
                return 0;
            }

            while (totalWritten < bytes.size())
            {
                DWORD bytesWritten = 0;
                const DWORD remaining = static_cast<DWORD>(bytes.size() - totalWritten);
                if (!WriteFile(static_cast<HANDLE>(_nativeHandle), bytes.data() + totalWritten, remaining, &bytesWritten, nullptr) || bytesWritten == 0)
                {
                    _markConnectionLost();
                    break;
                }

                totalWritten += bytesWritten;
            }
        }

        return totalWritten;
    }

    TransportError SerialPort::_openNativeHandle()
    {
        if (_closing)
        {
            return TransportError::openFailed;
        }

        HANDLE handle = CreateFileA(_portName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            return TransportError::openFailed;
        }

        DCB dcb{};
        dcb.DCBlength = sizeof(dcb);
        if (!GetCommState(handle, &dcb))
        {
            CloseHandle(handle);
            return TransportError::stateReadFailed;
        }

        dcb.BaudRate = static_cast<DWORD>(_baudRate);
        dcb.ByteSize = 8;
        dcb.StopBits = ONESTOPBIT;
        dcb.Parity = NOPARITY;
        dcb.fBinary = TRUE;
        dcb.fParity = FALSE;
        dcb.fOutxCtsFlow = FALSE;
        dcb.fOutxDsrFlow = FALSE;
        dcb.fDsrSensitivity = FALSE;
        dcb.fOutX = FALSE;
        dcb.fInX = FALSE;
        dcb.fAbortOnError = FALSE;
        dcb.fDtrControl = DTR_CONTROL_DISABLE;
        dcb.fRtsControl = RTS_CONTROL_DISABLE;

        if (!SetCommState(handle, &dcb))
        {
            CloseHandle(handle);
            return TransportError::configureFailed;
        }

        COMMTIMEOUTS timeouts{};
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.ReadTotalTimeoutConstant = 0;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant = 50;
        timeouts.WriteTotalTimeoutMultiplier = 0;
        if (!SetCommTimeouts(handle, &timeouts))
        {
            CloseHandle(handle);
            return TransportError::timeoutConfigureFailed;
        }

        std::lock_guard<std::mutex> lock(_nativeHandleMutex);
        if (_closing)
        {
            CloseHandle(handle);
            return TransportError::openFailed;
        }

        if (_nativeHandle != nullptr)
        {
            PurgeComm(static_cast<HANDLE>(_nativeHandle), PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
            CloseHandle(static_cast<HANDLE>(_nativeHandle));
        }

        _nativeHandle = handle;
        _connectionLost = false;
        _inputCondition.notify_all();
        return TransportError::ok;
    }

    void SerialPort::_closeNativeHandle()
    {
        std::lock_guard<std::mutex> lock(_nativeHandleMutex);
        if (_nativeHandle == nullptr)
        {
            return;
        }

        HANDLE handle = static_cast<HANDLE>(_nativeHandle);
        PurgeComm(handle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);
        CloseHandle(handle);
        _nativeHandle = nullptr;
        _connectionLost = false;
        _inputCondition.notify_all();
    }

    void SerialPort::_markConnectionLost() const
    {
        if (!_connectionLost.exchange(true))
        {
            _inputCondition.notify_all();
        }
    }

    void SerialPort::_readerLoop()
    {
        std::vector<uint8_t> chunk(256, 0);

        while (_running)
        {
            if (!isOpen())
            {
                if (!_running || _closing)
                {
                    break;
                }

                _closeNativeHandle();
                clearInputBuffer();
                _inputCondition.notify_all();
                while (_running && !_closing && _openNativeHandle() != TransportError::ok)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                }

                if (!_running || _closing)
                {
                    break;
                }

                continue;
            }

            const uint32_t queuedBytes = bytesInDriverQueue();
            if (queuedBytes == 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            DWORD receivedSize = 0;
            const DWORD bytesToRead = static_cast<DWORD>(std::min<size_t>(chunk.size(), queuedBytes));
            {
                std::lock_guard<std::mutex> nativeLock(_nativeHandleMutex);
                if (_nativeHandle == nullptr || _connectionLost)
                {
                    continue;
                }

                if (!ReadFile(static_cast<HANDLE>(_nativeHandle), chunk.data(), bytesToRead, &receivedSize, nullptr))
                {
                    _markConnectionLost();
                    continue;
                }
            }

            if (receivedSize > 0)
            {
                {
                    std::lock_guard<std::mutex> lock(_inputMutex);
                    _inputBuffer.insert(
                        _inputBuffer.end(),
                        chunk.begin(),
                        chunk.begin() + static_cast<std::vector<uint8_t>::difference_type>(receivedSize));
                }
                _inputCondition.notify_one();
            }
        }
    }
}
