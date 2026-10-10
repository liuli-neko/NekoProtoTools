cmake_minimum_required(VERSION 3.23)
project(NekoProtoConsumer LANGUAGES CXX)
enable_testing()

if(NEKO_USE_FETCHCONTENT)
    include(FetchContent)
    FetchContent_Declare(NekoProtoTools SOURCE_DIR "${NEKO_SOURCE_DIR}")
    FetchContent_MakeAvailable(NekoProtoTools)
else()
    find_package(NekoProtoTools CONFIG REQUIRED)
endif()

add_executable(neko_consumer main.cpp)
add_test(NAME neko_consumer_smoke COMMAND neko_consumer)
target_link_libraries(neko_consumer PRIVATE NekoProto::ArgParser)

if(TARGET NekoProto::Protocol)
    target_link_libraries(neko_consumer PRIVATE NekoProto::Protocol)
    target_compile_definitions(neko_consumer PRIVATE NEKO_CONSUMER_HAS_PROTOCOL)
endif()

if(TARGET NekoProto::Communication)
    target_link_libraries(neko_consumer PRIVATE NekoProto::Communication)
    target_compile_definitions(neko_consumer PRIVATE NEKO_CONSUMER_HAS_COMMUNICATION)
endif()

if(TARGET NekoProto::JsonRpc)
    target_link_libraries(neko_consumer PRIVATE NekoProto::JsonRpc)
    target_compile_definitions(neko_consumer PRIVATE NEKO_CONSUMER_HAS_JSONRPC)
endif()

if(TARGET NekoProto::PugiXml)
    target_compile_definitions(neko_consumer PRIVATE NEKO_CONSUMER_HAS_PUGIXML)
    if(WIN32)
        get_target_property(_neko_pugixml_runtime_dir NekoProto::PugiXml NEKO_PROTO_RUNTIME_DIR)
        if(_neko_pugixml_runtime_dir)
            set_property(TEST neko_consumer_smoke APPEND PROPERTY ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:${_neko_pugixml_runtime_dir}")
        endif()
    endif()
endif()

if(TARGET NekoProto::YamlCpp AND WIN32)
    get_target_property(_neko_yamlcpp_runtime_dir NekoProto::YamlCpp NEKO_PROTO_RUNTIME_DIR)
    if(_neko_yamlcpp_runtime_dir)
        set_property(TEST neko_consumer_smoke APPEND PROPERTY ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${_neko_yamlcpp_runtime_dir}")
    endif()
endif()
