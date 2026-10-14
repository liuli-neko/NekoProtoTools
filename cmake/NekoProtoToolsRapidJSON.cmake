if(TARGET NekoProto::RapidJSON)
    set(NEKO_PROTO_RAPIDJSON_FOUND TRUE)
    return()
endif()

if(NEKO_PROTO_FETCH_MISSING_DEPS)
    # xmake's temporary RapidJSON build can register an incomplete CMake package.
    # Installed packages in explicit prefixes remain discoverable; otherwise fetch headers.
    find_package(RapidJSON CONFIG QUIET NO_CMAKE_PACKAGE_REGISTRY)
else()
    find_package(RapidJSON CONFIG QUIET)
endif()
if(NOT TARGET RapidJSON AND NEKO_PROTO_FETCH_MISSING_DEPS)
    include(FetchContent)
    set(_neko_rapidjson_fetch_options)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
        list(APPEND _neko_rapidjson_fetch_options DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    endif()
    FetchContent_Declare(neko_rapidjson
        URL https://github.com/Tencent/rapidjson/archive/24b5e7a8b27f42fa16b96fc70aade9106cf7102f.tar.gz
        URL_HASH SHA256=2d2601a82d2d3b7e143a3c8d43ef616671391034bc46891a9816b79cf2d3e7a8
        SOURCE_SUBDIR nekoproto_header_only
        ${_neko_rapidjson_fetch_options})
    FetchContent_MakeAvailable(neko_rapidjson)
    add_library(RapidJSON INTERFACE IMPORTED GLOBAL)
    set_target_properties(RapidJSON PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${neko_rapidjson_SOURCE_DIR}/include")
endif()

if(TARGET RapidJSON)
    add_library(NekoProto::RapidJSON INTERFACE IMPORTED GLOBAL)
    set_target_properties(NekoProto::RapidJSON PROPERTIES
        INTERFACE_LINK_LIBRARIES RapidJSON)
    set(NEKO_PROTO_RAPIDJSON_FOUND TRUE)
else()
    set(NEKO_PROTO_RAPIDJSON_FOUND FALSE)
    set(NEKO_PROTO_RAPIDJSON_DIAGNOSTIC
        "NEKO_PROTO_ENABLE_RAPIDJSON=ON requires the RapidJSON CMake package with target RapidJSON. Install RapidJSON and set CMAKE_PREFIX_PATH or RapidJSON_DIR, or set NEKO_PROTO_FETCH_MISSING_DEPS=ON.")
endif()
