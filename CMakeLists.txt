cmake_minimum_required(VERSION 3.23)
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/version.lua" NEKO_PROTO_VERSION_LINE LIMIT_COUNT 1)
string(REGEX MATCH "[0-9]+[.][0-9]+[.][0-9]+" NEKO_PROTO_VERSION "${NEKO_PROTO_VERSION_LINE}")
if(NOT NEKO_PROTO_VERSION_LINE MATCHES "^function project_version" OR NOT NEKO_PROTO_VERSION)
    message(FATAL_ERROR "version.lua must define project_version() with major.minor.patch")
endif()
project(NekoProtoTools VERSION "${NEKO_PROTO_VERSION}" LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(NEKO_PROTO_BUILD_EXAMPLES "Build standalone examples" OFF)
option(NEKO_PROTO_BUILD_TESTS "Run example smoke tests" OFF)
option(NEKO_PROTO_ENABLE_RAPIDJSON "Enable the RapidJSON serializer" OFF)
option(NEKO_PROTO_ENABLE_PROTOCOL "Build the protocol factory" ON)

if(NEKO_PROTO_BUILD_TESTS)
    set(NEKO_PROTO_BUILD_EXAMPLES ON)
    enable_testing()
endif()

if(BUILD_SHARED_LIBS)
    set(NEKO_PROTO_USE_STATIC OFF)
else()
    set(NEKO_PROTO_USE_STATIC ON)
endif()

if(NEKO_PROTO_ENABLE_RAPIDJSON)
    find_package(RapidJSON CONFIG REQUIRED)
    if(NOT TARGET RapidJSON)
        message(FATAL_ERROR "RapidJSON must provide its RapidJSON CMake target")
    endif()
endif()

configure_file(cmake/config.h.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/nekoproto/global/config.h" @ONLY)

add_library(NekoProtoSerializer INTERFACE)
add_library(NekoProto::Serializer ALIAS NekoProtoSerializer)
set_target_properties(NekoProtoSerializer PROPERTIES EXPORT_NAME Serializer)
target_compile_features(NekoProtoSerializer INTERFACE cxx_std_23)
target_include_directories(NekoProtoSerializer INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
if(NEKO_PROTO_ENABLE_RAPIDJSON)
    target_link_libraries(NekoProtoSerializer INTERFACE RapidJSON)
endif()

add_library(NekoProtoArgParser INTERFACE)
add_library(NekoProto::ArgParser ALIAS NekoProtoArgParser)
set_target_properties(NekoProtoArgParser PROPERTIES EXPORT_NAME ArgParser)
target_link_libraries(NekoProtoArgParser INTERFACE NekoProtoSerializer)

if(NEKO_PROTO_ENABLE_PROTOCOL)
    add_library(NekoProtoProtocol src/proto_base.cpp)
    add_library(NekoProto::Protocol ALIAS NekoProtoProtocol)
    set_target_properties(NekoProtoProtocol PROPERTIES EXPORT_NAME Protocol)
    target_link_libraries(NekoProtoProtocol PUBLIC NekoProtoSerializer)
    if(BUILD_SHARED_LIBS)
        target_compile_definitions(NekoProtoProtocol PRIVATE NEKO_PROTO_LIBRARY)
    endif()
endif()

if(NEKO_PROTO_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
install(DIRECTORY include/nekoproto DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h" PATTERN "*.h.in" EXCLUDE)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/nekoproto/global/config.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/nekoproto/global")

set(_neko_targets NekoProtoSerializer NekoProtoArgParser)
if(NEKO_PROTO_ENABLE_PROTOCOL)
    list(APPEND _neko_targets NekoProtoProtocol)
endif()
install(TARGETS ${_neko_targets} EXPORT NekoProtoToolsTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(EXPORT NekoProtoToolsTargets NAMESPACE NekoProto::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/NekoProtoTools")

configure_package_config_file(cmake/NekoProtoToolsConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/NekoProtoToolsConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/NekoProtoTools")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/NekoProtoToolsConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/NekoProtoToolsConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/NekoProtoToolsConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/NekoProtoTools")
