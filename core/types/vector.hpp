#include <vector>

#include "../serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename T>
inline bool save(Serializer& sa, const std::vector<T>& value) {
    auto ret = sa.startArray(value.size());
    for (const auto& v : value)
        ret = sa(v) && ret;
    ret = sa.endArray() && ret;
    return ret;
}

template <typename Serializer, typename T>
inline bool loadValue(Serializer& sa, std::vector<T>& value) {
    auto s   = FieldSize();
    auto ret = sa(s);
    if (!ret)
        return false;
    value.resize(s.size());
    for (size_t i = 0; i < s.size(); ++i)
        ret = sa(value[i]) && ret;
    return ret;
}

NEKO_END_NAMESPACE