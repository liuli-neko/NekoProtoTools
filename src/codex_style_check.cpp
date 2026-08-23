#define NEKO_PROTO_ENABLE_SIMDJSON
#define NEKO_PROTO_ENABLE_TOMLPLUSPLUS

#include "nekoproto/global/string_literal.hpp"
#include "nekoproto/global/zip.hpp"
#include "nekoproto/serialization/json/simd_json_serializer.hpp"
#include "nekoproto/serialization/json/text_json_writer.hpp"
#include "nekoproto/serialization/private/integer.hpp"
#include "nekoproto/serialization/toml_serializer.hpp"
#include "nekoproto/serialization/xml_serializer.hpp"
#include "nekoproto/serialization/yaml_serializer.hpp"
