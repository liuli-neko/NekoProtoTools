if(TARGET NekoProto::Ilias)
    set(NEKO_PROTO_ILIAS_FOUND TRUE)
    return()
endif()

set(NEKO_PROTO_ILIAS_ROOT "" CACHE PATH "Installation prefix for Ilias when no CMake package is available")

if(NOT TARGET ilias::ilias AND NOT TARGET ilias)
    find_package(ilias 0.5.1 CONFIG QUIET)
    if(NOT TARGET ilias::ilias AND NOT TARGET ilias)
        # Upstream installs IliasConfig.cmake (with a capital I).
        find_package(Ilias 0.5.1 CONFIG QUIET)
    endif()
endif()
if(TARGET ilias::ilias)
    set(_neko_ilias_target ilias::ilias)
elseif(TARGET ilias)
    set(_neko_ilias_target ilias)
elseif(NEKO_PROTO_ILIAS_ROOT)
    if(WIN32)
        set(_neko_ilias_include_dir "${NEKO_PROTO_ILIAS_ROOT}/include")
        set(_neko_ilias_library "${NEKO_PROTO_ILIAS_ROOT}/lib/ilias.lib")
        if(NOT EXISTS "${_neko_ilias_include_dir}/ilias/task.hpp"
           OR NOT EXISTS "${_neko_ilias_library}")
            unset(_neko_ilias_include_dir)
            unset(_neko_ilias_library)
        endif()
    else()
        find_path(_neko_ilias_include_dir NAMES ilias/task.hpp
            HINTS "${NEKO_PROTO_ILIAS_ROOT}/include" NO_DEFAULT_PATH)
        find_library(_neko_ilias_library NAMES ilias
            HINTS "${NEKO_PROTO_ILIAS_ROOT}/lib" NO_DEFAULT_PATH)
    endif()
    if(_neko_ilias_include_dir AND _neko_ilias_library)
        add_library(NekoProto::IliasBinary UNKNOWN IMPORTED GLOBAL)
        set_target_properties(NekoProto::IliasBinary PROPERTIES
            IMPORTED_LOCATION "${_neko_ilias_library}"
            INTERFACE_INCLUDE_DIRECTORIES "${_neko_ilias_include_dir}")
        set(_neko_ilias_target NekoProto::IliasBinary)
    endif()
endif()

if(NOT _neko_ilias_target)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(NEKO_PROTO_ILIAS QUIET IMPORTED_TARGET GLOBAL ilias>=0.5.1)
        if(TARGET PkgConfig::NEKO_PROTO_ILIAS)
            set(_neko_ilias_target PkgConfig::NEKO_PROTO_ILIAS)
        endif()
    endif()
endif()

if(NOT _neko_ilias_target AND NEKO_PROTO_FETCH_MISSING_DEPS)
    if(CMAKE_VERSION VERSION_LESS 3.25)
        set(NEKO_PROTO_ILIAS_FOUND FALSE)
        set(NEKO_PROTO_ILIAS_DIAGNOSTIC
            "Fetching ilias v0.5.1 requires CMake >= 3.25. Upgrade CMake, or provide an installed Ilias package through CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, or NEKO_PROTO_ILIAS_ROOT.")
        return()
    endif()

    # Keep upstream's C++ standard and option variables local to this call.
    function(_neko_proto_fetch_ilias)
        include(FetchContent)
        FetchContent_Declare(neko_ilias
            URL https://github.com/BusyStudent/Ilias/archive/refs/tags/v0.5.1.tar.gz
            URL_HASH SHA256=f365ad1330ce4dedc12a0d526defb96920f877442d302645848fa7b40a83240d
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        # Use the same expected backend as NekoProto::Ilias for both runtime
        # and callers; leave the generated header's definition off to avoid redefining it.
        set(ILIAS_USE_ZEUS_EXPECTED OFF)
        FetchContent_MakeAvailable(neko_ilias)
        if(TARGET ilias)
            target_compile_definitions(ilias PUBLIC ILIAS_USE_ZEUS_EXPECTED=1)
            target_link_libraries(ilias PUBLIC "$<BUILD_INTERFACE:NekoProto::Expected>")
            target_compile_features(ilias PUBLIC cxx_std_20)
            set_target_properties(ilias PROPERTIES CXX_STANDARD 20)
        endif()
    endfunction()
    message(STATUS "Ilias not found; fetching ilias v0.5.1")
    _neko_proto_fetch_ilias()
    if(TARGET ilias::ilias)
        set(_neko_ilias_target ilias::ilias)
    elseif(TARGET ilias)
        set(_neko_ilias_target ilias)
    endif()
endif()

if(_neko_ilias_target)
    # Upstream exports OpenSSL targets for static TLS builds without
    # finding them in IliasConfig.cmake. Resolve that transitive dependency.
    get_target_property(_neko_ilias_links "${_neko_ilias_target}" INTERFACE_LINK_LIBRARIES)
    if(_neko_ilias_links MATCHES "OpenSSL::" AND (NOT TARGET OpenSSL::SSL OR NOT TARGET OpenSSL::Crypto))
        find_package(OpenSSL QUIET)
        if(NOT OpenSSL_FOUND)
            set(NEKO_PROTO_ILIAS_FOUND FALSE)
            set(NEKO_PROTO_ILIAS_DIAGNOSTIC
                "The installed Ilias TLS library requires OpenSSL. Install the OpenSSL development package or set OPENSSL_ROOT_DIR.")
            return()
        endif()
    endif()
    add_library(NekoProto::Ilias INTERFACE IMPORTED GLOBAL)
    set_target_properties(NekoProto::Ilias PROPERTIES
        INTERFACE_LINK_LIBRARIES "${_neko_ilias_target};NekoProto::Expected"
        INTERFACE_COMPILE_DEFINITIONS ILIAS_USE_ZEUS_EXPECTED=1)
    if(WIN32)
        get_target_property(_neko_ilias_type "${_neko_ilias_target}" TYPE)
        if(_neko_ilias_type STREQUAL "SHARED_LIBRARY")
            set_target_properties(NekoProto::Ilias PROPERTIES
                NEKO_PROTO_RUNTIME_DIR "$<TARGET_FILE_DIR:${_neko_ilias_target}>")
        elseif(NEKO_PROTO_ILIAS_ROOT AND EXISTS "${NEKO_PROTO_ILIAS_ROOT}/bin/ilias.dll")
            set_target_properties(NekoProto::Ilias PROPERTIES
                NEKO_PROTO_RUNTIME_DIR "${NEKO_PROTO_ILIAS_ROOT}/bin")
        endif()
    endif()
    set(NEKO_PROTO_ILIAS_FOUND TRUE)
else()
    set(NEKO_PROTO_ILIAS_FOUND FALSE)
    set(NEKO_PROTO_ILIAS_DIAGNOSTIC
        "Communication and JSON-RPC require ilias >= 0.5.1. Set NEKO_PROTO_FETCH_MISSING_DEPS=ON to fetch ilias v0.5.1 (CMake >= 3.25), install a CMake package exporting ilias::ilias, provide ilias.pc through CMAKE_PREFIX_PATH or PKG_CONFIG_PATH, or set NEKO_PROTO_ILIAS_ROOT to an installation prefix containing include/ilias/task.hpp and lib/ilias.")
    if(NEKO_PROTO_ILIAS_ROOT)
        string(APPEND NEKO_PROTO_ILIAS_DIAGNOSTIC
            " Checked NEKO_PROTO_ILIAS_ROOT=${NEKO_PROTO_ILIAS_ROOT}; expected ${NEKO_PROTO_ILIAS_ROOT}/include/ilias/task.hpp and ${NEKO_PROTO_ILIAS_ROOT}/lib/ilias.lib on Windows.")
    endif()
endif()
