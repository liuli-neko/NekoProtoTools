if(TARGET NekoProto::PugiXml)
    set(NEKO_PROTO_PUGIXML_FOUND TRUE)
    return()
endif()

find_package(pugixml CONFIG QUIET)
if(NOT TARGET pugixml::pugixml AND NEKO_PROTO_FETCH_MISSING_DEPS)
    include(FetchContent)
    set(_neko_pugixml_fetch_options)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
        list(APPEND _neko_pugixml_fetch_options DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    endif()
    FetchContent_Declare(neko_pugixml
        URL https://github.com/zeux/pugixml/archive/refs/tags/v1.16.tar.gz
        URL_HASH SHA256=357bcab8877dc9943f355d3a72daba1b053238ba955f50fa81586afb65090219
        ${_neko_pugixml_fetch_options})
    FetchContent_MakeAvailable(neko_pugixml)
endif()

if(TARGET pugixml::pugixml)
    add_library(NekoProto::PugiXml INTERFACE IMPORTED GLOBAL)
    set_target_properties(NekoProto::PugiXml PROPERTIES
        INTERFACE_LINK_LIBRARIES pugixml::pugixml)
    if(WIN32 AND TARGET pugixml::shared)
        get_target_property(_neko_pugixml_dll pugixml::shared IMPORTED_LOCATION_RELEASE)
        if(NOT _neko_pugixml_dll)
            get_target_property(_neko_pugixml_dll pugixml::shared IMPORTED_LOCATION)
        endif()
        if(_neko_pugixml_dll)
            get_filename_component(_neko_pugixml_runtime_dir "${_neko_pugixml_dll}" DIRECTORY)
            set_property(TARGET NekoProto::PugiXml PROPERTY
                NEKO_PROTO_RUNTIME_DIR "${_neko_pugixml_runtime_dir}")
        endif()
    endif()
    set(NEKO_PROTO_PUGIXML_FOUND TRUE)
else()
    set(NEKO_PROTO_PUGIXML_FOUND FALSE)
    set(NEKO_PROTO_PUGIXML_DIAGNOSTIC
        "NEKO_PROTO_ENABLE_PUGIXML=ON requires pugixml::pugixml. Install pugixml and set CMAKE_PREFIX_PATH, or set NEKO_PROTO_FETCH_MISSING_DEPS=ON.")
endif()
