#pragma once

#include <cstdint>

namespace device_transport
{
    enum class TraceDirection : uint8_t
    {
        rx,
        tx
    };
}
