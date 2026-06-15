#pragma once

#include "serial_port/xbee/constants.hpp"

#include <cstdint>
#include <vector>

namespace device_transport
{
    struct ReceivedXBeeFrame
    {
        uint64_t xbee64Id{};
        uint16_t xbee16Id{};
        uint8_t receiveOptions{};
        std::vector<uint8_t> payload;
    };

    struct AtCommandResponse
    {
        uint8_t frameId{};
        uint16_t atCommand{};
        uint8_t status{};
        std::vector<uint8_t> value;
    };

    struct RemoteAtCommandResponse
    {
        uint8_t frameId{};
        uint64_t xbee64Id{};
        uint16_t xbee16Id{};
        uint16_t atCommand{};
        uint8_t status{};
        std::vector<uint8_t> value;
    };

    struct TransmitStatus
    {
        uint8_t frameId{};
        uint16_t destinationAddress{};
        uint8_t retryCount{};
        uint8_t deliveryStatus{};
        uint8_t discoveryStatus{};
    };

    struct ModemStatus
    {
        uint8_t status{};
    };
}
