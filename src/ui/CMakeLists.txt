add_library(knxodt_ui STATIC
    MainWindow.h
    MainWindow.cpp
    ProjectTreeWidget.h
    ProjectTreeWidget.cpp
    ProjectTreeModel.h
    ProjectTreeModel.cpp
    CatalogWidget.h
    CatalogWidget.cpp
    CatalogModel.h
    CatalogModel.cpp
    DeviceEditorWidget.h
    DeviceEditorWidget.cpp
    BusMonitorWidget.h
    BusMonitorWidget.cpp
    BusMonitorModel.h
    BusMonitorModel.cpp
    GroupMonitorWidget.h
    GroupMonitorWidget.cpp
    UndoCommands.h
    UndoCommands.cpp
    ProgramDialog.h
    ProgramDialog.cpp
    PropertiesPanel.h
    PropertiesPanel.cpp
    dialogs/NewProjectDialog.h
    dialogs/NewProjectDialog.cpp
    dialogs/ConnectDialog.h
    dialogs/ConnectDialog.cpp
    dialogs/GroupAddressDialog.h
    dialogs/GroupAddressDialog.cpp
    dialogs/LineScanDialog.h
    dialogs/LineScanDialog.cpp
)

target_include_directories(knxodt_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(knxodt_ui PUBLIC Qt6::Core Qt6::Widgets Qt6::Network knxodt_core knxodt_knxip knxodt_usb)
