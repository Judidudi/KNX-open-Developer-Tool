add_library(knxodt_usb STATIC
    UsbKnxInterface.h
    UsbKnxInterface.cpp
    ConnectInterfaceFactory.h
    ConnectInterfaceFactory.cpp
)

target_include_directories(knxodt_usb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(knxodt_usb PUBLIC Qt6::Core knxodt_knxip)

find_package(Qt6 QUIET COMPONENTS SerialPort)
if(Qt6SerialPort_FOUND)
    target_link_libraries(knxodt_usb PUBLIC Qt6::SerialPort)
else()
    message(STATUS "Qt6SerialPort not found – USB CDC interface disabled")
    target_compile_definitions(knxodt_usb PRIVATE KNXODT_NO_SERIAL)
endif()
