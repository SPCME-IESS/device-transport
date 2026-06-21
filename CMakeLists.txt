cmake_minimum_required(VERSION 3.20)

project(device_transport VERSION 0.1.0 LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

if(NOT WIN32)
    message(FATAL_ERROR "device_transport currently supports Windows only")
endif()

add_library(device_transport_core INTERFACE)
add_library(device_transport::core ALIAS device_transport_core)

target_compile_features(device_transport_core
    INTERFACE
        cxx_std_11
)

target_include_directories(device_transport_core
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(device_transport_core
    PROPERTIES
        EXPORT_NAME core
)

add_library(device_transport_serial
    src/serial_port/serial_port.cpp
)

add_library(device_transport::serial ALIAS device_transport_serial)

target_compile_features(device_transport_serial
    PUBLIC
        cxx_std_11
)

target_link_libraries(device_transport_serial
    PUBLIC
        device_transport_core
)

set_target_properties(device_transport_serial
    PROPERTIES
        EXPORT_NAME serial
)

add_library(device_transport_desktop
    src/xbee/xbee.cpp
)

add_library(device_transport::desktop ALIAS device_transport_desktop)

target_compile_features(device_transport_desktop
    PUBLIC
        cxx_std_11
)

target_link_libraries(device_transport_desktop
    PUBLIC
        device_transport_core
        device_transport_serial
)

set_target_properties(device_transport_desktop
    PROPERTIES
        EXPORT_NAME desktop
)

add_library(device_transport INTERFACE)
add_library(device_transport::device_transport ALIAS device_transport)

target_link_libraries(device_transport
    INTERFACE
        device_transport::desktop
)

set_target_properties(device_transport
    PROPERTIES
        EXPORT_NAME device_transport
)

install(TARGETS
    device_transport
    device_transport_core
    device_transport_serial
    device_transport_desktop
    EXPORT device_transportTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT device_transportTargets
    FILE device_transportTargets.cmake
    NAMESPACE device_transport::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/device_transport
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/device_transportConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/device_transportConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/device_transportConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/device_transport
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/device_transportConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/device_transportConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/device_transport
)

install(FILES LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/device_transport
)
