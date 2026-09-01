cmake_minimum_required(VERSION 3.20)

project(device_transport VERSION 0.1.0 LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(DEVICE_TRANSPORT_INSTALL "Install device-transport package" ON)

if(NOT WIN32)
    message(FATAL_ERROR "device_transport currently supports Windows only")
endif()

add_library(device_transport_core INTERFACE)

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

add_library(device_transport_serial STATIC
    src/serial_port/serial_port.cpp
)

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

add_library(device_transport_desktop STATIC
    src/xbee/xbee.cpp
)

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

target_link_libraries(device_transport
    INTERFACE
        device_transport_desktop
)

set_target_properties(device_transport
    PROPERTIES
        EXPORT_NAME device_transport
)

if(DEVICE_TRANSPORT_INSTALL)
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
endif()
