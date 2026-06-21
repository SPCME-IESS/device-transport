#pragma once

#include "core/error.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace device_transport
{
    class SerialPort
    {
    public:
        SerialPort() = default;
        ~SerialPort();

        TransportError open(const std::string &portName, uint32_t baudRate);
        bool isOpen() const;
        void close();

        size_t bytesToRead() const;
        size_t bytesToWrite() const;
        uint32_t bytesInDriverQueue() const;
        bool waitForInputSize(size_t byteCount, uint32_t timeoutMs);

        uint8_t read8();
        uint16_t read16();
        uint32_t read32();
        uint64_t read64();

        uint32_t write8(uint8_t value);
        uint32_t write16(uint16_t value);
        uint32_t write32(uint32_t value);
        uint32_t write64(uint64_t value);

        void clearInputBuffer();
        void clearOutputBuffer();

        std::vector<uint8_t> getInputBuffer() const;
        std::vector<uint8_t> getOutputBuffer() const;

        uint32_t send();

    private:
        void *_nativeHandle = nullptr;
        std::string _portName;
        uint32_t _baudRate{};

        std::atomic<bool> _running{false};
        std::atomic<bool> _closing{false};
        mutable std::atomic<bool> _connectionLost{false};
        std::thread _readerThread;

        std::vector<uint8_t> _inputBuffer;
        std::vector<uint8_t> _outputBuffer;
        mutable std::mutex _nativeHandleMutex;
        mutable std::mutex _inputMutex;
        mutable std::mutex _outputMutex;
        mutable std::condition_variable _inputCondition;

        TransportError _openNativeHandle();
        void _closeNativeHandle();
        void _markConnectionLost() const;
        void _readerLoop();
    };
}
