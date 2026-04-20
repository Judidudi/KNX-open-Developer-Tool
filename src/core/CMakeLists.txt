add_library(knxodt_core STATIC
    GroupAddress.h
    GroupAddress.cpp
    TopologyNode.h
    TopologyNode.cpp
    DeviceInstance.h
    DeviceInstance.cpp
    ComObjectLink.h
    ComObjectLink.cpp
    Manifest.h
    Manifest.cpp
    DeviceCatalog.h
    DeviceCatalog.cpp
    DptRegistry.h
    DptRegistry.cpp
    Project.h
    Project.cpp
    ProjectXmlSerializer.h
    ProjectXmlSerializer.cpp
)

target_include_directories(knxodt_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(knxodt_core PUBLIC Qt6::Core Qt6::Xml yaml-cpp::yaml-cpp)
