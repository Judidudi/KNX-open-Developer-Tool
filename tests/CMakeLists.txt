function(add_knxodt_test TARGET SOURCE)
    qt_add_executable(${TARGET} ${SOURCE})
    target_link_libraries(${TARGET} PRIVATE Qt6::Test knxodt_core)
    add_test(NAME ${TARGET} COMMAND ${TARGET})
endfunction()

add_knxodt_test(test_groupaddress    test_groupaddress.cpp)
add_knxodt_test(test_project_model   test_project_model.cpp)
add_knxodt_test(test_xml_serializer  test_xml_serializer.cpp)

# Manifest parser test needs catalog path
qt_add_executable(test_manifest_parser test_manifest_parser.cpp)
target_link_libraries(test_manifest_parser PRIVATE Qt6::Test knxodt_core)
target_compile_definitions(test_manifest_parser PRIVATE
    CATALOG_PATH="${CMAKE_SOURCE_DIR}/catalog/devices")
add_test(NAME test_manifest_parser COMMAND test_manifest_parser)

# CEMI frame test also links knxodt_knxip
qt_add_executable(test_cemi_frame test_cemi_frame.cpp)
target_link_libraries(test_cemi_frame PRIVATE Qt6::Test knxodt_core knxodt_knxip)
add_test(NAME test_cemi_frame COMMAND test_cemi_frame)

# TableBuilder lives in knxodt_knxip and depends on both libs
qt_add_executable(test_table_builder test_table_builder.cpp)
target_link_libraries(test_table_builder PRIVATE Qt6::Test knxodt_core knxodt_knxip)
add_test(NAME test_table_builder COMMAND test_table_builder)
