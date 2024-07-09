#pragma once
#include "../private/global.hpp"

#if NEKO_CPP_PLUS >= 20
#include <string>

#include "../serializer_base.hpp"

NEKO_BEGIN_NAMESPACE
template <typename Serializer>
inline bool save(Serializer& sa, const std::u8string& value) {
    return sa(std::string_view{reinterpret_cast<const char*>(value.data()), value.size()});
}

template <typename Serializer>
inline bool load(Serializer& sa, std::u8string& value) {
    std::string sv;
    if (!sa(sv)) {
        return false;
    }
    value = std::u8string(reinterpret_cast<const char8_t*>(sv.data()), sv.size());
    return true;
}
NEKO_END_NAMESPACE
#endif