#pragma once
#include "module_prelude.hpp"
#include "nekoproto/global/detail/expected_dependency.hpp"
#if defined(NEKO_PROTO_ENABLE_RAPIDJSON)
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#endif
#if defined(NEKO_PROTO_ENABLE_YYJSON)
#include <yyjson.h>
#endif
#if defined(NEKO_PROTO_ENABLE_SIMDJSON)
#include <simdjson.h>
#endif
#if defined(NEKO_PROTO_ENABLE_PUGIXML)
#include <pugixml.hpp>
#endif
#if defined(NEKO_PROTO_ENABLE_LIBFYAML)
#include <libfyaml.h>
#endif
#if defined(NEKO_PROTO_ENABLE_YAMLCPP)
#include <yaml-cpp/yaml.h>
#endif
#if defined(NEKO_PROTO_ENABLE_TOMLPLUSPLUS)
#ifndef TOML_EXCEPTIONS
#define TOML_EXCEPTIONS 0
#endif
#include <toml++/toml.h>
#endif
