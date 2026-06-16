#pragma once

#include "nekoproto/global/global.hpp"

#include <cstdint>

#ifndef _WIN32
#include <arpa/inet.h>
#endif

NEKO_BEGIN_NAMESPACE

inline std::int16_t htobe(const std::int16_t value) noexcept {
#ifdef _WIN32
    return static_cast<std::int16_t>(_byteswap_ushort(static_cast<std::uint16_t>(value)));
#else
    return static_cast<std::int16_t>(htobe16(static_cast<std::uint16_t>(value)));
#endif
}

inline std::uint16_t htobe(const std::uint16_t value) noexcept {
#ifdef _WIN32
    return _byteswap_ushort(value);
#else
    return htobe16(value);
#endif
}

inline std::int32_t htobe(const std::int32_t value) noexcept {
#ifdef _WIN32
    return static_cast<std::int32_t>(_byteswap_ulong(static_cast<std::uint32_t>(value)));
#else
    return static_cast<std::int32_t>(htobe32(static_cast<std::uint32_t>(value)));
#endif
}

inline std::uint32_t htobe(const std::uint32_t value) noexcept {
#ifdef _WIN32
    return _byteswap_ulong(value);
#else
    return htobe32(value);
#endif
}

inline std::int64_t htobe(const std::int64_t value) noexcept {
#ifdef _WIN32
    return static_cast<std::int64_t>(_byteswap_uint64(static_cast<std::uint64_t>(value)));
#else
    return static_cast<std::int64_t>(htobe64(static_cast<std::uint64_t>(value)));
#endif
}

inline std::uint64_t htobe(const std::uint64_t value) noexcept {
#ifdef _WIN32
    return _byteswap_uint64(value);
#else
    return htobe64(value);
#endif
}

inline std::int16_t betoh(const std::int16_t value) noexcept {
#ifdef _WIN32
    return static_cast<std::int16_t>(_byteswap_ushort(static_cast<std::uint16_t>(value)));
#else
    return static_cast<std::int16_t>(be16toh(static_cast<std::uint16_t>(value)));
#endif
}

inline std::uint16_t betoh(const std::uint16_t value) noexcept {
#ifdef _WIN32
    return _byteswap_ushort(value);
#else
    return be16toh(value);
#endif
}

inline std::int32_t betoh(const std::int32_t value) noexcept {
#ifdef _WIN32
    return static_cast<std::int32_t>(_byteswap_ulong(static_cast<std::uint32_t>(value)));
#else
    return static_cast<std::int32_t>(be32toh(static_cast<std::uint32_t>(value)));
#endif
}

inline std::uint32_t betoh(const std::uint32_t value) noexcept {
#ifdef _WIN32
    return _byteswap_ulong(value);
#else
    return be32toh(value);
#endif
}

inline std::int64_t betoh(const std::int64_t value) noexcept {
#ifdef _WIN32
    return static_cast<std::int64_t>(_byteswap_uint64(static_cast<std::uint64_t>(value)));
#else
    return static_cast<std::int64_t>(be64toh(static_cast<std::uint64_t>(value)));
#endif
}

inline std::uint64_t betoh(const std::uint64_t value) noexcept {
#ifdef _WIN32
    return _byteswap_uint64(value);
#else
    return be64toh(value);
#endif
}

NEKO_END_NAMESPACE
