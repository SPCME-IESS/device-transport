# device-transport

Reusable low-level Windows transport library for byte encoding, serial-port I/O,
and XBee API frames.

The library intentionally does not contain application concepts such as devices,
databases, initialization workflows, telemetry storage, UI frameworks, or user
algorithms.

## Responsibilities

- big-endian byte helpers;
- transport error codes;
- trace direction and callback support;
- Windows `SerialPort` byte I/O;
- XBee API frame constants and structs;
- XBee AT and remote AT commands;
- XBee transmit requests;
- XBee receive packet parsing.

## Threading Model

`SerialPort` owns the Windows COM-port reader thread.

`XBee` owns the API-frame parser thread and exposes parsed receive packets to
consumer applications.

## Layout

```text
include/
  core/
    byte_codec.hpp
    error.hpp
    trace.hpp

  serial_port/
    serial_port.hpp

  xbee/
    constants.hpp
    frames.hpp
    xbee.hpp

src/
  serial_port/
    serial_port.cpp

  xbee/
    xbee.cpp
```

## Build

Requirements:

- CMake 3.20 or newer;
- Windows;
- C++11-compatible compiler.

```sh
cmake -S . -B build
cmake --build build
```

Or with presets:

```sh
cmake --preset default
cmake --build --preset default
```

## CMake Usage

```cmake
add_subdirectory(path/to/device-transport)
target_link_libraries(my_app PRIVATE device_transport_desktop)
```

After install:

```cmake
find_package(device_transport CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE desktop)
```

Available targets:

- `core` provides header-only byte, error, and trace helpers;
- `serial` provides Windows COM-port I/O;
- `desktop` provides XBee over `SerialPort`;
- `device_transport` links the desktop transport interface.

## C++ Usage

```cpp
#include <core/byte_codec.hpp>
#include <xbee/xbee.hpp>

#include <cstdint>
#include <vector>

XBee xbee;
if (xbee.open("\\\\.\\COM5", 9600) != 0)
{
    return 1;
}

std::vector<uint8_t> payload;
write8(payload, 0x01);
write16BigEndian(payload, 0x0800);

xbee.transmitRequest(
    0x0013A20000000000ULL,
    unknownXbee16Id,
    payload);
```

Remote AT commands use the same address-first order. Pass the unknown 16-bit
address explicitly when it is not available:

```cpp
xbee.remoteAtCommandRequest(
    0x0013A20000000000ULL,
    unknownXbee16Id,
    ni);
```

The older stateful payload API remains available for compatibility:

```cpp
xbee.clearOutputPayload();
xbee.write8(0x01);
xbee.write16(0x0800);
xbee.transmitRequest(0x0013A20000000000ULL);
```

## Join Window

```cpp
xbee.openJoinWindow(60);
xbee.closeJoinWindow();
```

## AT Reads

Raw and typed AT response helpers are available:

```cpp
std::vector<uint8_t> data;
xbee.readAtCommandData(op, data, 200);

uint16_t panId = 0;
xbee.readAtCommand16(oi, panId, 200);

uint64_t extendedPanId = 0;
xbee.readAtCommand64(op, extendedPanId, 200);
```

## License

MIT. See `LICENSE`.
