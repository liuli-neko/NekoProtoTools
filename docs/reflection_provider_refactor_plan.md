# Reflection Provider Refactor Plan

## Goal

`Reflect<T>` is the public reflection API. It should stay stable while the implementation gains multiple metadata backends. The immediate refactor separates:

- public API and iteration algorithms,
- a normalized compile-time field model,
- metadata providers that adapt concrete backends.

This split prepares the current implementation for a C++26 native reflection backend without changing serializer, argparser, RPC, or schema call sites.

## Progress Checklist

This checklist is the source of truth for this refactor. Update it whenever a step is started or completed.

- [x] Write the provider/model/API layering plan.
- [x] Keep explicit metadata priority: `T::Neko`, then `Meta<T>`, then automatic discovery.
- [x] Add the transitional `ReflectProvider<T>` adapter for current metadata sources.
- [x] Add `ReflectModel<T>` for resolved `value_types`, `field_tags`, and `value_count`.
- [x] Remove the obsolete `ReflectHelper<T>` facade after all internal usage moved to `ReflectProvider<T>`.
- [x] Make non-enum `Reflect<T>` consume `ReflectProvider<T>` and `ReflectModel<T>` instead of deriving tags/types itself.
- [x] Add boundary comments for provider/model/public algorithm.
- [x] Add compile-time coverage for the `AutoUnwrap` provider path.
- [x] Add compile-time coverage for the explicit `LocalObject` provider path.
- [x] Verify `test_reflection`.
- [x] Verify `test_argparser` as a downstream reflection consumer.
- [x] Trim incidental formatting-only diff from the first provider/model patch if it obscures review.
- [x] Split provider selection from legacy `MetaKind` so future native reflection can replace only automatic discovery.
- [x] Add compile-time coverage for `ReflectProviderKind` on `AutoUnwrap`, `T::Neko`, and `Meta<T>` paths.
- [x] Add an index-based provider facade: `field_type<I>`, `name<I>()`, `tag<I>()`, and `get<I>(obj)`.
- [x] Move current tuple-accessor logic behind that index-based facade.
- [x] Add a gated `NativeReflectionProvider<T>` placeholder that is selected only for the automatic discovery path.
- [x] Add annotation-to-tag normalization hooks for native reflection metadata.
- [ ] Add compile-gated tests for native provider selection when compiler support exists. (blocked until a usable static-reflection feature macro/backend is available)

## Backend Priority

Explicit user metadata has priority over automatic discovery:

1. `T::Neko` metadata, including `Object`, `Array`, single value, names, values, and `field_tags`.
2. `Meta<T>` specializations.
3. Future C++26 native reflection provider.
4. Current aggregate `AutoUnwrap` fallback.

The future native backend is meant to replace the current `AutoUnwrap` path, not override user-declared metadata. Users who need private fields, custom accessors, ABI-stable field order, or manual tags should keep using `T::Neko` or `Meta<T>`.

## Layering

### Public API

`Reflect<T>` keeps the existing surface:

- `names()`, `name(index)`, `className()`, `size()`,
- `value<N>(obj)`, `value(obj, index)`, `value(obj, name)`,
- `forEach(obj, callback)`,
- `forEachMeta(callback)`,
- `value_types`, `field_tags`, `value_count`.

No downstream module should depend on provider-specific details.

### Field Model

`ReflectModel<T>` is the normalized compile-time view consumed by `Reflect<T>`:

- resolved field value types,
- normalized field tags,
- field count.

It also performs tag/type checks after provider metadata is normalized.

### Provider SPI

Providers adapt concrete metadata sources into a common shape. The transitional provider still exposes tuple-like accessors because the current codebase is built around `Object`, `Array`, `NEKO_SERIALIZER`, and aggregate unwrap. The desired long-term provider shape is index-based:

```cpp
Provider<T>::value_count
Provider<T>::has_names

Provider<T>::template field_type<I>
Provider<T>::template name<I>()
Provider<T>::template tag<I>()
Provider<T>::template get<I>(obj)
```

The index-based shape maps cleanly to C++26 native reflection while current tuple-based providers can implement it on top of `std::get<I>`.

## Names And Tags

`Reflect<T>::names()` returns reflected source names. Wire names and command-line option names remain tag semantics:

- serializer field rename uses `tag_property::name`,
- argparser long names use `tag_property::long_name`,
- comments, skip behavior, flat/unframed, fixed length, and similar metadata stay in tags.

Future C++26 annotations should be normalized by the native provider into the existing tag vocabulary:

```cpp
NoTags
JsonTag
BinaryTag
TagList<...>
```

`TagList` is the normalized carrier for explicit field tags. It is tuple-like for positional inspection:

```cpp
std::get<I>(tags)
tags.get<I>()
std::tuple_size_v<decltype(tags)>
```

Semantic lookup should go through `tag_query` only:

```cpp
tag_query::has<tag_property::name>(tags)
tag_query::get<tag_property::name>(tags)
tag_query::has_tag<JsonTag>(tags)
tag_query::get_tag<JsonTag>(tags)
```

When `make_tags` receives nested `TagList` values, it flattens them before storing field metadata. Property lookup and
type lookup both use last-wins semantics after flattening, so later tags can intentionally override earlier tags without
requiring merge rules for same-name tags.

The current hook points are:

- `NativeAnnotationTags<Annotations...>` / `native_annotation_tags_v<Annotations...>`: normalize annotation values into the same tag representation as `make_tags`.
- `NativeReflectionFieldTags<T, I>` / `native_reflection_field_tags_v<T, I>`: field-level override used by a future native backend. The default is `NoTags`.

Downstream code should continue to use `tag_query` and should not know whether tags came from `make_tags` or native annotations.

## First Refactor Step

The first step is intentionally conservative:

- add `ReflectProvider<T>` as the current metadata adapter,
- add `ReflectModel<T>` for `value_types`, `field_tags`, and `value_count`,
- remove the obsolete `ReflectHelper<T>` facade once no call sites need the old `getNames`/`getValues` names,
- update `Reflect<T>` to consume provider/model instead of directly inspecting `T::Neko` or `Meta<T>`.

After this step, behavior should be identical. Future work can replace the `AutoUnwrap` branch with `NativeReflectionProvider<T>` behind the same provider selection point.

## Verification Log

- 2026-06-30: `xmake build test_reflection` passed.
- 2026-06-30: `xmake run test_reflection` passed, 8 tests.
- 2026-06-30: `xmake build test_argparser` passed.
- 2026-06-30: `xmake run test_argparser` passed, 27 tests.
- 2026-06-30: Re-ran `xmake build/run test_reflection` and `xmake build/run test_argparser` after removing `ReflectHelper<T>`; all passed.
