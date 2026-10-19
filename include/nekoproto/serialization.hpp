#pragma once
#include "nekoproto/global/macros.hpp"

#if !defined(NEKO_BUILDING_MODULE) || NEKO_BUILDING_MODULE == NEKO_MODULE_SERIALIZER

#include "nekoproto/serializer.hpp"
#include "nekoproto/serialization/binary_serializer.hpp"
#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/xml_serializer.hpp"
#include "nekoproto/serialization/yaml_serializer.hpp"
#include "nekoproto/serialization/toml_serializer.hpp"
#include "nekoproto/serialization/to_string.hpp"
#endif // current component: serializer
