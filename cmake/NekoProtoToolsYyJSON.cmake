if(TARGET NekoProto::YyJSON)
    set(NEKO_PROTO_YYJSON_FOUND TRUE)
    return()
endif()

find_package(yyjson CONFIG QUIET)
if(NOT TARGET yyjson::yyjson AND NOT TARGET yyjson
   AND NEKO_PROTO_FETCH_MISSING_DEPS)
    include(FetchContent)
    set(_neko_yyjson_fetch_options)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
        list(APPEND _neko_yyjson_fetch_options DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    endif()
    FetchContent_Declare(neko_yyjson
        URL https://github.com/ibireme/yyjson/archive/refs/tags/0.12.0.tar.gz
        URL_HASH SHA256=b16246f617b2a136c78d73e5e2647c6f1de1313e46678062985bdcf1f40bb75d
        ${_neko_yyjson_fetch_options})
    FetchContent_MakeAvailable(neko_yyjson)
endif()

if(TARGET yyjson::yyjson OR TARGET yyjson)
    add_library(NekoProto::YyJSON INTERFACE IMPORTED GLOBAL)
    if(TARGET yyjson::yyjson)
        set_target_properties(NekoProto::YyJSON PROPERTIES
            INTERFACE_LINK_LIBRARIES yyjson::yyjson)
    else()
        set_target_properties(NekoProto::YyJSON PROPERTIES
            INTERFACE_LINK_LIBRARIES yyjson)
    endif()
    set(NEKO_PROTO_YYJSON_FOUND TRUE)
else()
    set(NEKO_PROTO_YYJSON_FOUND FALSE)
    set(NEKO_PROTO_YYJSON_DIAGNOSTIC
        "NEKO_PROTO_ENABLE_YYJSON=ON requires the yyjson CMake package with target yyjson::yyjson. Install yyjson and set CMAKE_PREFIX_PATH or yyjson_DIR, or set NEKO_PROTO_FETCH_MISSING_DEPS=ON.")
endif()
