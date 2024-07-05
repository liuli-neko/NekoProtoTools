#include <map>

#include "../serializer_base.hpp"

NEKO_BEGIN_NAMESPACE

template <typename Serializer, typename K, typename V>
inline bool save(Serializer& sa, const std::map<K, V>& value) {
    bool ret = sa.startArray();
    for (const auto& v : value) {
        ret = sa.startObject() && ret;
        ret = sa(MakeNamedField("key", 4, v.first)) && ret;
        ret = sa(MakeNamedField("value", 6, v.second)) && ret;
    }
    return ret && sa.endArray();
}

template <typename Serializer, typename K, typename V>
inline bool loadValue(Serializer& sa, std::map<K, V>& value) {
    K k;
    V v;
    NamedField kField("key", 4, k), vField("value", 6, v);
    bool ret;
    while (true) {
        if (sa(kField) && sa(vField)) {
            value.insert(std::make_pair(k.value, v.value));
        } else {
            break;
        }
    }
}

NEKO_END_NAMESPACE