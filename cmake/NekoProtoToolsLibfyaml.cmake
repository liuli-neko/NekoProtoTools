if(TARGET NekoProto::Libfyaml)
    set(NEKO_PROTO_LIBFYAML_FOUND TRUE)
    return()
endif()

find_package(libfyaml CONFIG QUIET)
if(NOT TARGET libfyaml::fyaml AND NOT TARGET libfyaml::libfyaml
   AND NEKO_PROTO_FETCH_MISSING_DEPS AND NOT WIN32)
    include(FetchContent)
    set(_neko_libfyaml_fetch_options)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
        list(APPEND _neko_libfyaml_fetch_options DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    endif()
    FetchContent_Declare(neko_libfyaml
        URL https://github.com/pantoniou/libfyaml/archive/refs/tags/v0.9.5.tar.gz
        URL_HASH SHA256=bbb0ce413003e665b8bce1c167640f069ad10491a0c05937b41aa7a2c3f1d139
        ${_neko_libfyaml_fetch_options})
    # Upstream defaults BUILD_SHARED_LIBS to ON even when embedded. Match the
    # consumer's established setting so later targets keep their library type.
    if(NOT DEFINED BUILD_SHARED_LIBS)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries")
    endif()
    # libfyaml uses the global BUILD_TESTING option for its own test suite.
    # Keep the consumer's setting after adding the dependency.
    if(DEFINED BUILD_TESTING)
        set(_neko_libfyaml_had_build_testing TRUE)
        set(_neko_libfyaml_build_testing "${BUILD_TESTING}")
    else()
        set(_neko_libfyaml_had_build_testing FALSE)
    endif()
    set(BUILD_TESTING OFF)
    FetchContent_MakeAvailable(neko_libfyaml)
    if(_neko_libfyaml_had_build_testing)
        set(BUILD_TESTING "${_neko_libfyaml_build_testing}")
    else()
        unset(BUILD_TESTING)
    endif()
endif()

if(TARGET libfyaml::fyaml)
    set(_neko_libfyaml_target libfyaml::fyaml)
elseif(TARGET libfyaml::libfyaml)
    set(_neko_libfyaml_target libfyaml::libfyaml)
endif()

if(_neko_libfyaml_target)
    add_library(NekoProto::Libfyaml INTERFACE IMPORTED GLOBAL)
    set_target_properties(NekoProto::Libfyaml PROPERTIES
        INTERFACE_LINK_LIBRARIES "${_neko_libfyaml_target}")
    set(NEKO_PROTO_LIBFYAML_FOUND TRUE)
else()
    set(NEKO_PROTO_LIBFYAML_FOUND FALSE)
    if(WIN32)
        set(NEKO_PROTO_LIBFYAML_DIAGNOSTIC
            "NEKO_PROTO_ENABLE_LIBFYAML=ON requires an installed libfyaml CMake package with target libfyaml::fyaml on Windows; source fetching is supported on Unix only. Set CMAKE_PREFIX_PATH or libfyaml_DIR.")
    else()
        set(NEKO_PROTO_LIBFYAML_DIAGNOSTIC
            "NEKO_PROTO_ENABLE_LIBFYAML=ON requires the libfyaml CMake package with target libfyaml::fyaml. Install libfyaml v0.9.5 and set CMAKE_PREFIX_PATH or libfyaml_DIR, or set NEKO_PROTO_FETCH_MISSING_DEPS=ON.")
    endif()
endif()
