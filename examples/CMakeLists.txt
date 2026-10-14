add_executable(neko_serialization serialization.cpp)
target_link_libraries(neko_serialization PRIVATE NekoProto::Serializer)

add_executable(neko_reflection reflection.cpp)
target_link_libraries(neko_reflection PRIVATE NekoProto::Serializer)

add_executable(neko_argparser argparser.cpp)
target_link_libraries(neko_argparser PRIVATE NekoProto::ArgParser)

add_executable(neko_all_in_one all_in_one.cpp)
target_link_libraries(neko_all_in_one PRIVATE NekoProto::ArgParser)

add_executable(neko_protocol_ids protocol_ids.cpp)
target_link_libraries(neko_protocol_ids PRIVATE NekoProto::Protocol)

if(NEKO_PROTO_BUILD_TESTS)
    add_test(NAME example_serialization COMMAND neko_serialization)
    add_test(NAME example_reflection COMMAND neko_reflection)
    add_test(NAME example_argparser COMMAND neko_argparser --id 7 --name Neko)
    add_test(NAME example_all_in_one COMMAND neko_all_in_one --id 7 --name Neko)
    add_test(NAME example_protocol_ids_forward COMMAND neko_protocol_ids --forward)
    add_test(NAME example_protocol_ids_reverse COMMAND neko_protocol_ids --reverse)
endif()
