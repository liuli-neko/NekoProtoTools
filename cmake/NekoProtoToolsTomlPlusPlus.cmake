if(TARGET NekoProto::TomlPlusPlus)
    set(NEKO_PROTO_TOMLPLUSPLUS_FOUND TRUE)
    return()
endif()

find_package(tomlplusplus 3.4.0 CONFIG QUIET)
if(NOT TARGET tomlplusplus::tomlplusplus AND NEKO_PROTO_FETCH_MISSING_DEPS)
    include(FetchContent)
    set(_neko_tomlplusplus_fetch_options)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
        list(APPEND _neko_tomlplusplus_fetch_options DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    endif()
    FetchContent_Declare(neko_tomlplusplus
        URL https://github.com/marzer/tomlplusplus/archive/refs/tags/v3.4.0.tar.gz
        URL_HASH SHA256=8517f65938a4faae9ccf8ebb36631a38c1cadfb5efa85d9a72e15b9e97d25155
        SOURCE_SUBDIR nekoproto_header_only
        ${_neko_tomlplusplus_fetch_options})
    FetchContent_MakeAvailable(neko_tomlplusplus)
    add_library(tomlplusplus::tomlplusplus INTERFACE IMPORTED GLOBAL)
    set_target_properties(tomlplusplus::tomlplusplus PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${neko_tomlplusplus_SOURCE_DIR}/include")
endif()

if(TARGET tomlplusplus::tomlplusplus)
    add_library(NekoProto::TomlPlusPlus INTERFACE IMPORTED GLOBAL)
    set_target_properties(NekoProto::TomlPlusPlus PROPERTIES
        INTERFACE_LINK_LIBRARIES tomlplusplus::tomlplusplus)
    set(NEKO_PROTO_TOMLPLUSPLUS_FOUND TRUE)
else()
    set(NEKO_PROTO_TOMLPLUSPLUS_FOUND FALSE)
    set(NEKO_PROTO_TOMLPLUSPLUS_DIAGNOSTIC
        "NEKO_PROTO_ENABLE_TOMLPLUSPLUS=ON requires the tomlplusplus CMake package with target tomlplusplus::tomlplusplus. Install toml++ v3.4.0 and set CMAKE_PREFIX_PATH or tomlplusplus_DIR, or set NEKO_PROTO_FETCH_MISSING_DEPS=ON.")
endif()
