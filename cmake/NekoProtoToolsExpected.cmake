if(TARGET NekoProto::Expected)
    set(NEKO_PROTO_EXPECTED_FOUND TRUE)
    return()
endif()

# Prefer the upstream CMake package, then an already installed header. Fetching
# is opt-in so installed consumers also work without network access.
find_package(zeus_expected CONFIG QUIET)
if(NOT TARGET zeus::expected)
    find_path(NEKO_PROTO_ZEUS_EXPECTED_INCLUDE_DIR NAMES zeus/expected.hpp
        DOC "Directory containing zeus/expected.hpp")
endif()

if(NOT TARGET zeus::expected AND NOT NEKO_PROTO_ZEUS_EXPECTED_INCLUDE_DIR
   AND NEKO_PROTO_FETCH_MISSING_DEPS)
    include(FetchContent)
    set(_neko_expected_fetch_options)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
        list(APPEND _neko_expected_fetch_options DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    endif()
    FetchContent_Declare(neko_zeus_expected
        URL https://github.com/zeus-cpp/expected/archive/refs/tags/v1.4.0.tar.gz
        URL_HASH SHA256=70f7118651be51f5c02c0aa10bfb903f5a6c0bcd13ae726dfe34bf8836aaece0
        ${_neko_expected_fetch_options})
    FetchContent_MakeAvailable(neko_zeus_expected)
endif()

if(TARGET zeus::expected)
    add_library(NekoProto::Expected INTERFACE IMPORTED GLOBAL)
    set_target_properties(NekoProto::Expected PROPERTIES
        INTERFACE_LINK_LIBRARIES zeus::expected)
    set(NEKO_PROTO_EXPECTED_FOUND TRUE)
elseif(NEKO_PROTO_ZEUS_EXPECTED_INCLUDE_DIR AND
       EXISTS "${NEKO_PROTO_ZEUS_EXPECTED_INCLUDE_DIR}/zeus/expected.hpp")
    add_library(NekoProto::Expected INTERFACE IMPORTED GLOBAL)
    set_target_properties(NekoProto::Expected PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${NEKO_PROTO_ZEUS_EXPECTED_INCLUDE_DIR}")
    set(NEKO_PROTO_EXPECTED_FOUND TRUE)
else()
    set(NEKO_PROTO_EXPECTED_FOUND FALSE)
    set(NEKO_PROTO_EXPECTED_DIAGNOSTIC
        "Missing zeus_expected (zeus/expected.hpp) for C++20. Install its CMake package and set CMAKE_PREFIX_PATH, provide NEKO_PROTO_ZEUS_EXPECTED_INCLUDE_DIR, or set NEKO_PROTO_FETCH_MISSING_DEPS=ON.")
endif()
