set(TS_FILES
    resources/translations/app_de.ts
    resources/translations/app_en.ts
)

qt_add_executable(KNXOpenDeveloperTool
    main.cpp
)

qt_add_translations(KNXOpenDeveloperTool
    TS_FILES ${TS_FILES}
    QM_FILES_OUTPUT_VARIABLE QM_FILES
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
    MACOSX_BUNDLE TRUE
)
