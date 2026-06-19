#pragma once

#include <cstdint>

namespace device_transport
{
    namespace api_frame
    {
        static constexpr uint8_t startDelimiter = 0x7E;
        static constexpr uint8_t escape = 0x7D;
        static constexpr uint8_t xon = 0x11;
        static constexpr uint8_t xoff = 0x13;
        static constexpr uint8_t defaultFrameId = 0x00;
        static constexpr uint8_t remoteCommandOptionsApplyChanges = 0x02;

        enum class ApiMode : uint8_t
        {
            api1 = 1,
            api2 = 2
        };

        namespace frame_type
        {
            static constexpr uint8_t atCommandRequest = 0x08;
            static constexpr uint8_t atCommandQueueParameterValue = 0x09;
            static constexpr uint8_t transmitRequest = 0x10;
            static constexpr uint8_t remoteAtCommandRequest = 0x17;
            static constexpr uint8_t atCommandResponse = 0x88;
            static constexpr uint8_t modemStatus = 0x8A;
            static constexpr uint8_t transmitStatus = 0x8B;
            static constexpr uint8_t receivePacket = 0x90;
            static constexpr uint8_t explicitReceiveIndicator = 0x91;
            static constexpr uint8_t remoteAtCommandResponse = 0x97;
        }
    }

    namespace at_command
    {
        static constexpr uint16_t id = 0x4944;
        static constexpr uint16_t sc = 0x5343;
        static constexpr uint16_t sd = 0x5344;
        static constexpr uint16_t zs = 0x5A53;
        static constexpr uint16_t nj = 0x4E4A;
        static constexpr uint16_t nw = 0x4E57;
        static constexpr uint16_t jv = 0x4A56;
        static constexpr uint16_t jn = 0x4A4E;
        static constexpr uint16_t op = 0x4F50;
        static constexpr uint16_t oi = 0x4F49;
        static constexpr uint16_t ch = 0x4348;
        static constexpr uint16_t nc = 0x4E43;
        static constexpr uint16_t sa = 0x5341;
        static constexpr uint16_t my = 0x4D59;
        static constexpr uint16_t da = 0x4441;
        static constexpr uint16_t ni = 0x4E49;
        static constexpr uint16_t nh = 0x4E48;
        static constexpr uint16_t bh = 0x4248;
        static constexpr uint16_t ar = 0x4152;
        static constexpr uint16_t dd = 0x4444;
        static constexpr uint16_t nt = 0x4E54;
        static constexpr uint16_t no = 0x4E4F;
        static constexpr uint16_t np = 0x4E50;
        static constexpr uint16_t cr = 0x4352;
        static constexpr uint16_t pl = 0x504C;
        static constexpr uint16_t pm = 0x504D;
        static constexpr uint16_t pp = 0x5050;
        static constexpr uint16_t ee = 0x4545;
        static constexpr uint16_t eo = 0x454F;
        static constexpr uint16_t ky = 0x4B59;
        static constexpr uint16_t bd = 0x4244;
        static constexpr uint16_t nb = 0x4E42;
        static constexpr uint16_t sb = 0x5342;
        static constexpr uint16_t d7 = 0x4437;
        static constexpr uint16_t d6 = 0x4436;
        static constexpr uint16_t ap = 0x4150;
        static constexpr uint16_t ao = 0x414F;
        static constexpr uint16_t sm = 0x534D;
        static constexpr uint16_t sn = 0x534E;
        static constexpr uint16_t so = 0x534F;
        static constexpr uint16_t sp = 0x5350;
        static constexpr uint16_t st = 0x5354;
        static constexpr uint16_t po = 0x504F;
        static constexpr uint16_t vr = 0x5652;
        static constexpr uint16_t hv = 0x4856;
        static constexpr uint16_t ai = 0x4149;
        static constexpr uint16_t db = 0x4442;
        static constexpr uint16_t v = 0x2556;
        static constexpr uint16_t nr = 0x4E52;
        static constexpr uint16_t wr = 0x5752;
        static constexpr uint16_t ac = 0x4143;
        static constexpr uint16_t cb = 0x4342;
    }
}
