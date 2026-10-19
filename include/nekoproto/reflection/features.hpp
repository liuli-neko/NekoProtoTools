#pragma once

#include <nekoproto/global/config.h>
#include <version>

#define NEKO_REFLECTION_LEGACY 0
#define NEKO_REFLECTION_NATIVE 1
#define NEKO_REFLECTION_AUTO 2

#ifndef NEKO_PROTO_REFLECTION_BACKEND
#define NEKO_PROTO_REFLECTION_BACKEND NEKO_REFLECTION_LEGACY
#endif

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L && \
    defined(__cpp_lib_reflection) && __cpp_lib_reflection >= 202506L
#define NEKO_HAS_NATIVE_REFLECTION 1
#else
#define NEKO_HAS_NATIVE_REFLECTION 0
#endif

#if defined(__cpp_expansion_statements) && __cpp_expansion_statements >= 202506L
#define NEKO_HAS_EXPANSION_STATEMENTS 1
#else
#define NEKO_HAS_EXPANSION_STATEMENTS 0
#endif

#if defined(__cpp_structured_bindings) && __cpp_structured_bindings >= 202411L
#define NEKO_HAS_STRUCTURED_BINDING_PACKS 1
#else
#define NEKO_HAS_STRUCTURED_BINDING_PACKS 0
#endif

#if defined(__cpp_pack_indexing) && __cpp_pack_indexing >= 202311L
#define NEKO_HAS_PACK_INDEXING 1
#else
#define NEKO_HAS_PACK_INDEXING 0
#endif

#if NEKO_PROTO_REFLECTION_BACKEND < NEKO_REFLECTION_LEGACY || \
    NEKO_PROTO_REFLECTION_BACKEND > NEKO_REFLECTION_AUTO
#error "NEKO_PROTO_REFLECTION_BACKEND must be LEGACY (0), NATIVE (1), or AUTO (2)"
#endif
#if NEKO_PROTO_REFLECTION_BACKEND == NEKO_REFLECTION_NATIVE && !NEKO_HAS_NATIVE_REFLECTION
#error "native reflection requires standard reflection feature macros (and, with GCC, -freflection)"
#endif
// Backend selection follows language capability. The release default remains
// legacy; opting into auto selects native discovery for supported object types.
#ifndef NEKO_USE_NATIVE_REFLECTION
#define NEKO_USE_NATIVE_REFLECTION \
    (NEKO_HAS_NATIVE_REFLECTION && NEKO_PROTO_REFLECTION_BACKEND != NEKO_REFLECTION_LEGACY)
#endif
#if NEKO_USE_NATIVE_REFLECTION && !NEKO_HAS_NATIVE_REFLECTION
#error "the selected module configuration requires native reflection support"
#endif

// Independent controls; header consumers may override them before inclusion.
// Binding packs remove the generated 128-field dispatch limit. Pack indexing
// avoids constructing a tuple just to select a single tag value.
#ifndef NEKO_USE_STRUCTURED_BINDING_PACKS
#define NEKO_USE_STRUCTURED_BINDING_PACKS NEKO_HAS_STRUCTURED_BINDING_PACKS
#endif
#ifndef NEKO_USE_EXPANSION_STATEMENTS
#define NEKO_USE_EXPANSION_STATEMENTS 0
#endif
#ifndef NEKO_USE_PACK_INDEXING
#define NEKO_USE_PACK_INDEXING NEKO_HAS_PACK_INDEXING
#endif
#if NEKO_USE_STRUCTURED_BINDING_PACKS && !NEKO_HAS_STRUCTURED_BINDING_PACKS
#error "structured binding packs are unavailable"
#endif
#if NEKO_USE_EXPANSION_STATEMENTS && !NEKO_HAS_EXPANSION_STATEMENTS
#error "expansion statements are unavailable"
#endif
#if NEKO_USE_PACK_INDEXING && !NEKO_HAS_PACK_INDEXING
#error "pack indexing is unavailable"
#endif
