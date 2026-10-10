if(TARGET NekoProto::SimdJSON)
    set(NEKO_PROTO_SIMDJSON_FOUND TRUE)
    return()
endif()

find_package(simdjson CONFIG QUIET)
if(NOT TARGET simdjson::simdjson AND NEKO_PROTO_FETCH_MISSING_DEPS)
    include(FetchContent)
    set(_neko_simdjson_fetch_options)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
        list(APPEND _neko_simdjson_fetch_options DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    endif()
    FetchContent_Declare(neko_simdjson
        URL https://github.com/simdjson/simdjson/archive/refs/tags/v4.6.11.tar.gz
        URL_HASH SHA256=61d948fc24f0d793829ad658058e7597d064988a89b4607ea02e401a82df98ff
        ${_neko_simdjson_fetch_options})
    FetchContent_MakeAvailable(neko_simdjson)
endif()

if(TARGET simdjson::simdjson)
    add_library(NekoProto::SimdJSON INTERFACE IMPORTED GLOBAL)
    set_target_properties(NekoProto::SimdJSON PROPERTIES
        INTERFACE_LINK_LIBRARIES simdjson::simdjson)
    set(NEKO_PROTO_SIMDJSON_FOUND TRUE)
else()
    set(NEKO_PROTO_SIMDJSON_FOUND FALSE)
    set(NEKO_PROTO_SIMDJSON_DIAGNOSTIC
        "NEKO_PROTO_ENABLE_SIMDJSON=ON requires the simdjson CMake package with target simdjson::simdjson. Install simdjson and set CMAKE_PREFIX_PATH or simdjson_DIR, or set NEKO_PROTO_FETCH_MISSING_DEPS=ON.")
endif()
