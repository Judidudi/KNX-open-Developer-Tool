function(add_knxodt_test TARGET SOURCE)
    qt_add_executable(${TARGET} ${SOURCE})
    target_link_libraries(${TARGET} PRIVATE Qt6::Test knxodt_core)
    add_test(NAME ${TARGET} COMMAND ${TARGET})
endfunction()

add_knxodt_test(test_groupaddress           test_groupaddress.cpp)
add_knxodt_test(test_project_model          test_project_model.cpp)
add_knxodt_test(test_xml_serializer         test_xml_serializer.cpp)
add_knxodt_test(test_parameter_visibility   test_parameter_visibility.cpp)

add_knxodt_test(test_knxprod_parser test_knxprod_parser.cpp)

# CEMI frame test also links knxodt_knxip
qt_add_executable(test_cemi_frame test_cemi_frame.cpp)
target_link_libraries(test_cemi_frame PRIVATE Qt6::Test knxodt_core knxodt_knxip)
add_test(NAME test_cemi_frame COMMAND test_cemi_frame)

# TableBuilder lives in knxodt_knxip and depends on both libs
qt_add_executable(test_table_builder test_table_builder.cpp)
target_link_libraries(test_table_builder PRIVATE Qt6::Test knxodt_core knxodt_knxip)
add_test(NAME test_table_builder COMMAND test_table_builder)

# DeviceProgrammer test (mock interface, no real KNX hardware needed)
qt_add_executable(test_programmer test_programmer.cpp)
target_link_libraries(test_programmer PRIVATE Qt6::Test knxodt_core knxodt_knxip)
add_test(NAME test_programmer COMMAND test_programmer)
