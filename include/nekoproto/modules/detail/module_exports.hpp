// Loaded after the module declaration; headers above it are dependencies.
#pragma once
#undef NEKO_EXPORT
#undef NEKO_BEGIN_EXPORT
#undef NEKO_END_EXPORT
#define NEKO_EXPORT export
#define NEKO_BEGIN_EXPORT export {
#define NEKO_END_EXPORT }

#include "nekoproto/global/macros.hpp"
