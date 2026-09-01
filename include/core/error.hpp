#pragma once

#include <cstdint>

enum class TransportError : uint16_t
{
    ok = 0,
    openFailed = 1,
    stateReadFailed = 2,
    configureFailed = 3,
    timeoutConfigureFailed = 4,
    unsupportedBaudRate = 5,
    invalidArgument = 6
};

constexpr uint16_t errorCode(const TransportError error)
{
    return static_cast<uint16_t>(error);
}
