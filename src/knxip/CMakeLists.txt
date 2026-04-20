add_library(knxodt_knxip STATIC
    IKnxInterface.h
    CemiFrame.h
    CemiFrame.cpp
    KnxIpDiscovery.h
    KnxIpDiscovery.cpp
    KnxIpTunnelingClient.h
    KnxIpTunnelingClient.cpp
)

target_include_directories(knxodt_knxip PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(knxodt_knxip PUBLIC Qt6::Core Qt6::Network)
