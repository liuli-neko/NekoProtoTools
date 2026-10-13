if(TARGET NekoProto::Ilias)
    set(NEKO_PROTO_ILIAS_FOUND TRUE)
    return()
endif()

set(NEKO_PROTO_ILIAS_ROOT "" CACHE PATH "Installation prefix for Ilias when no CMake package is available")

find_package(ilias 0.5.0 CONFIG QUIET)
if(TARGET ilias::ilias)
    set(_neko_ilias_target ilias::ilias)
elseif(TARGET ilias)
    set(_neko_ilias_target ilias)
elseif(NEKO_PROTO_ILIAS_ROOT)
    find_path(_neko_ilias_include_dir NAMES ilias/task.hpp
        HINTS "${NEKO_PROTO_ILIAS_ROOT}/include" NO_DEFAULT_PATH)
    find_library(_neko_ilias_library NAMES ilias
        HINTS "${NEKO_PROTO_ILIAS_ROOT}/lib" NO_DEFAULT_PATH)
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
        pkg_check_modules(NEKO_PROTO_ILIAS QUIET IMPORTED_TARGET GLOBAL ilias>=0.5.0)
        if(TARGET PkgConfig::NEKO_PROTO_ILIAS)
            set(_neko_ilias_target PkgConfig::NEKO_PROTO_ILIAS)
        endif()
    endif()
endif()

if(_neko_ilias_target)
    add_library(NekoProto::Ilias INTERFACE IMPORTED GLOBAL)
    set_target_properties(NekoProto::Ilias PROPERTIES
        INTERFACE_LINK_LIBRARIES "${_neko_ilias_target};NekoProto::Expected"
        INTERFACE_COMPILE_DEFINITIONS ILIAS_USE_ZEUS_EXPECTED=1)
    if(WIN32 AND NEKO_PROTO_ILIAS_ROOT AND EXISTS "${NEKO_PROTO_ILIAS_ROOT}/bin/ilias.dll")
        set_target_properties(NekoProto::Ilias PROPERTIES
            NEKO_PROTO_RUNTIME_DIR "${NEKO_PROTO_ILIAS_ROOT}/bin")
    endif()
    set(NEKO_PROTO_ILIAS_FOUND TRUE)
else()
    set(NEKO_PROTO_ILIAS_FOUND FALSE)
    set(NEKO_PROTO_ILIAS_DIAGNOSTIC
        "Communication and JSON-RPC require ilias >= 0.5.0. Install a CMake package exporting ilias::ilias, provide ilias.pc through CMAKE_PREFIX_PATH or PKG_CONFIG_PATH, or set NEKO_PROTO_ILIAS_ROOT to an installation prefix containing include/ilias/task.hpp and lib/ilias. Automatic source fetching is not provided for ilias.")
endif()
