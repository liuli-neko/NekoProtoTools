#include <map>

#include "../serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename K, typename V>
inline bool save(Serializer& sa, const std::map<K, V>& value) {
    bool ret = sa.startArray(value.size());
    for (const auto& v : value) {
        ret = sa.startObject() && ret;
        ret = sa(NamedField("key", 4, v.first)) && ret;
        ret = sa(NamedField("value", 6, v.second)) && ret;
    }
    return ret && sa.endArray();
}

template <typename Serializer, typename K, typename V>
inline bool load(Serializer& sa, std::map<K, V>& value) {
    K k;
    V v;
    bool ret;
    FieldSize s;
    ret = sa(s);
    while (ret) {
        if (sa(NamedField("key", 4, k)) && sa(NamedField("value", 6, v))) {
            value.insert(std::make_pair(k, v));
        } else {
            break;
        }
    }
    return ret && (s.size == value.size());
}

NEKO_END_NAMESPACE