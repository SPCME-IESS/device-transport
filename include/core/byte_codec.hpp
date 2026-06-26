#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace device_transport
{
    namespace byte_codec
    {
        inline uint8_t read8(const std::vector<uint8_t> &bytes, const size_t offset = 0)
        {
            return bytes[offset];
        }

        inline uint16_t read16BigEndian(const std::vector<uint8_t> &bytes, const size_t offset = 0)
        {
            return static_cast<uint16_t>((static_cast<uint16_t>(bytes[offset]) << 8) | bytes[offset + 1]);
        }

        inline uint32_t read32BigEndian(const std::vector<uint8_t> &bytes, const size_t offset = 0)
        {
            return (static_cast<uint32_t>(bytes[offset]) << 24) |
                   (static_cast<uint32_t>(bytes[offset + 1]) << 16) |
                   (static_cast<uint32_t>(bytes[offset + 2]) << 8) |
                   static_cast<uint32_t>(bytes[offset + 3]);
        }

        inline uint64_t read64BigEndian(const std::vector<uint8_t> &bytes, const size_t offset = 0)
        {
            return (static_cast<uint64_t>(bytes[offset]) << 56) |
                   (static_cast<uint64_t>(bytes[offset + 1]) << 48) |
                   (static_cast<uint64_t>(bytes[offset + 2]) << 40) |
                   (static_cast<uint64_t>(bytes[offset + 3]) << 32) |
                   (static_cast<uint64_t>(bytes[offset + 4]) << 24) |
                   (static_cast<uint64_t>(bytes[offset + 5]) << 16) |
                   (static_cast<uint64_t>(bytes[offset + 6]) << 8) |
                   static_cast<uint64_t>(bytes[offset + 7]);
        }

        inline uint16_t read16LittleEndian(const std::vector<uint8_t> &bytes, const size_t offset = 0)
        {
            return static_cast<uint16_t>(bytes[offset] | (static_cast<uint16_t>(bytes[offset + 1]) << 8));
        }

        inline uint32_t read32LittleEndian(const std::vector<uint8_t> &bytes, const size_t offset = 0)
        {
            return static_cast<uint32_t>(bytes[offset]) |
                   (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
                   (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
                   (static_cast<uint32_t>(bytes[offset + 3]) << 24);
        }

        inline uint64_t read64LittleEndian(const std::vector<uint8_t> &bytes, const size_t offset = 0)
        {
            return static_cast<uint64_t>(bytes[offset]) |
                   (static_cast<uint64_t>(bytes[offset + 1]) << 8) |
                   (static_cast<uint64_t>(bytes[offset + 2]) << 16) |
                   (static_cast<uint64_t>(bytes[offset + 3]) << 24) |
                   (static_cast<uint64_t>(bytes[offset + 4]) << 32) |
                   (static_cast<uint64_t>(bytes[offset + 5]) << 40) |
                   (static_cast<uint64_t>(bytes[offset + 6]) << 48) |
                   (static_cast<uint64_t>(bytes[offset + 7]) << 56);
        }

        inline void write8(std::vector<uint8_t> &buffer, const uint8_t value)
        {
            buffer.push_back(value);
        }

        inline void write16BigEndian(std::vector<uint8_t> &buffer, const uint16_t value)
        {
            buffer.push_back(static_cast<uint8_t>(value >> 8));
            buffer.push_back(static_cast<uint8_t>(value));
        }

        inline void write32BigEndian(std::vector<uint8_t> &buffer, const uint32_t value)
        {
            buffer.push_back(static_cast<uint8_t>(value >> 24));
            buffer.push_back(static_cast<uint8_t>(value >> 16));
            buffer.push_back(static_cast<uint8_t>(value >> 8));
            buffer.push_back(static_cast<uint8_t>(value));
        }

        inline void write64BigEndian(std::vector<uint8_t> &buffer, const uint64_t value)
        {
            buffer.push_back(static_cast<uint8_t>(value >> 56));
            buffer.push_back(static_cast<uint8_t>(value >> 48));
            buffer.push_back(static_cast<uint8_t>(value >> 40));
            buffer.push_back(static_cast<uint8_t>(value >> 32));
            buffer.push_back(static_cast<uint8_t>(value >> 24));
            buffer.push_back(static_cast<uint8_t>(value >> 16));
            buffer.push_back(static_cast<uint8_t>(value >> 8));
            buffer.push_back(static_cast<uint8_t>(value));
        }

        inline void write16LittleEndian(std::vector<uint8_t> &buffer, const uint16_t value)
        {
            buffer.push_back(static_cast<uint8_t>(value));
            buffer.push_back(static_cast<uint8_t>(value >> 8));
        }

        inline void write32LittleEndian(std::vector<uint8_t> &buffer, const uint32_t value)
        {
            buffer.push_back(static_cast<uint8_t>(value));
            buffer.push_back(static_cast<uint8_t>(value >> 8));
            buffer.push_back(static_cast<uint8_t>(value >> 16));
            buffer.push_back(static_cast<uint8_t>(value >> 24));
        }

        inline void write64LittleEndian(std::vector<uint8_t> &buffer, const uint64_t value)
        {
            buffer.push_back(static_cast<uint8_t>(value));
            buffer.push_back(static_cast<uint8_t>(value >> 8));
            buffer.push_back(static_cast<uint8_t>(value >> 16));
            buffer.push_back(static_cast<uint8_t>(value >> 24));
            buffer.push_back(static_cast<uint8_t>(value >> 32));
            buffer.push_back(static_cast<uint8_t>(value >> 40));
            buffer.push_back(static_cast<uint8_t>(value >> 48));
            buffer.push_back(static_cast<uint8_t>(value >> 56));
        }

        inline float bitsToFloat(const uint32_t raw)
        {
            static_assert(sizeof(float) == sizeof(uint32_t), "float must be 32 bits");
            float value{};
            std::memcpy(&value, &raw, sizeof(value));
            return value;
        }

        inline double bitsToDouble(const uint64_t raw)
        {
            static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64 bits");
            double value{};
            std::memcpy(&value, &raw, sizeof(value));
            return value;
        }

        inline uint32_t floatToBits(const float value)
        {
            static_assert(sizeof(float) == sizeof(uint32_t), "float must be 32 bits");
            uint32_t raw{};
            std::memcpy(&raw, &value, sizeof(raw));
            return raw;
        }

        inline uint64_t doubleToBits(const double value)
        {
            static_assert(sizeof(double) == sizeof(uint64_t), "double must be 64 bits");
            uint64_t raw{};
            std::memcpy(&raw, &value, sizeof(raw));
            return raw;
        }

        inline float readFloatBigEndian(const std::vector<uint8_t> &bytes, const size_t offset = 0)
        {
            return bitsToFloat(read32BigEndian(bytes, offset));
        }

        inline float readFloatLittleEndian(const std::vector<uint8_t> &bytes, const size_t offset = 0)
        {
            return bitsToFloat(read32LittleEndian(bytes, offset));
        }

        inline double readDoubleBigEndian(const std::vector<uint8_t> &bytes, const size_t offset = 0)
        {
            return bitsToDouble(read64BigEndian(bytes, offset));
        }

        inline double readDoubleLittleEndian(const std::vector<uint8_t> &bytes, const size_t offset = 0)
        {
            return bitsToDouble(read64LittleEndian(bytes, offset));
        }

        inline void writeFloatBigEndian(std::vector<uint8_t> &buffer, const float value)
        {
            write32BigEndian(buffer, floatToBits(value));
        }

        inline void writeFloatLittleEndian(std::vector<uint8_t> &buffer, const float value)
        {
            write32LittleEndian(buffer, floatToBits(value));
        }

        inline void writeDoubleBigEndian(std::vector<uint8_t> &buffer, const double value)
        {
            write64BigEndian(buffer, doubleToBits(value));
        }

        inline void writeDoubleLittleEndian(std::vector<uint8_t> &buffer, const double value)
        {
            write64LittleEndian(buffer, doubleToBits(value));
        }
    }
}
