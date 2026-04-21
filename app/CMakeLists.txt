set(TS_FILES
    resources/translations/app_de.ts
    resources/translations/app_en.ts
)

qt_add_executable(KNXOpenDeveloperTool
    main.cpp
)

# Embed translations as Qt resources (accessible via :/translations/)
qt_add_translations(KNXOpenDeveloperTool
    TS_FILES ${TS_FILES}
    QM_FILES_OUTPUT_VARIABLE QM_FILES
)

# Embed icons and other resources
qt_add_resources(KNXOpenDeveloperTool "app_resources"
    PREFIX "/"
    FILES
        resources/icons/knxodt.svg
)

target_link_libraries(KNXOpenDeveloperTool PRIVATE
    Qt6::Core
    Qt6::Widgets
    knxodt_ui
    knxodt_core
    knxodt_knxip
    knxodt_usb
)

set_target_properties(KNXOpenDeveloperTool PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE    TRUE
    MACOSX_BUNDLE_BUNDLE_NAME       "KNX open Developer Tool"
    MACOSX_BUNDLE_BUNDLE_VERSION    "0.1.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
)

# Linux install targets
if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)

    install(TARGETS KNXOpenDeveloperTool
        DESTINATION ${CMAKE_INSTALL_BINDIR})

    install(FILES resources/knxodt.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

    install(FILES resources/icons/knxodt.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

    install(FILES resources/knxodt-mime.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages)

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/catalog
        DESTINATION ${CMAKE_INSTALL_DATADIR}/knxodt)
endif()
