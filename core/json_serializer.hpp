/**
 * @file proto_json_serializer.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-06-18
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include "private/global.hpp"

#include <fstream>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>
#include <type_traits>
#include <vector>
#if NEKO_CPP_PLUS >= 17
#include <optional>
#include <variant>
#endif

#include "serializer_base.hpp"

NEKO_BEGIN_NAMESPACE
template <typename... Args>
bool save(const Args&... args) {
    static_assert(((!std::is_class<Args>::value) && ...), "not implemented");
};
template <typename... Args>
bool load(const Args&... args) {
    static_assert(((!std::is_class<Args>::value) && ...), "not implemented");
}

using JsonValue    = rapidjson::Value;
using JsonDocument = rapidjson::Document;

template <typename WriterT, typename ValueT, typename T, class enable = void>
struct JsonConvert;

template <typename ValueType = JsonDocument, class enable = void>
class JsonDeserializer;

template <typename BufferT = OutBufferWrapper>
using JsonWriter = rapidjson::Writer<BufferT>;

template <typename BufferType = std::vector<char>, typename Writer = OutBufferWrapper>
class JsonInputSerializer;

// TODO: make it to be a interface ?
class OutBufferWrapper {
public:
    using Ch = char;

    OutBufferWrapper() NEKO_NOEXCEPT;
    OutBufferWrapper(std::vector<Ch>* vec) NEKO_NOEXCEPT;
    void setVector(std::vector<Ch>* vec) NEKO_NOEXCEPT;
    void Put(Ch c) NEKO_NOEXCEPT;
    void Flush() NEKO_NOEXCEPT;
    const Ch* GetString() const NEKO_NOEXCEPT;
    std::size_t GetSize() const NEKO_NOEXCEPT;
    void Clear() NEKO_NOEXCEPT;

private:
    std::shared_ptr<std::vector<Ch>> mVec;
};

inline OutBufferWrapper::OutBufferWrapper() NEKO_NOEXCEPT : mVec(new std::vector<Ch>()) {}
inline OutBufferWrapper::OutBufferWrapper(std::vector<Ch>* vec) NEKO_NOEXCEPT : mVec(vec, [](std::vector<Ch>* ptr) {}) {
}
inline void OutBufferWrapper::setVector(std::vector<Ch>* vec) NEKO_NOEXCEPT {
    if (vec != nullptr) {
        mVec.reset(vec, [](std::vector<Ch>* ptr) {});
    } else {
        mVec.reset(new std::vector<Ch>(), [](std::vector<Ch>* ptr) { delete ptr; });
    }
}
inline void OutBufferWrapper::Put(Ch c) NEKO_NOEXCEPT { mVec->push_back(c); }
inline void OutBufferWrapper::Flush() NEKO_NOEXCEPT {}
inline const OutBufferWrapper::Ch* OutBufferWrapper::GetString() const NEKO_NOEXCEPT { return mVec->data(); }
inline std::size_t OutBufferWrapper::GetSize() const NEKO_NOEXCEPT { return mVec->size(); }
inline void OutBufferWrapper::Clear() NEKO_NOEXCEPT { mVec->clear(); }

namespace {
inline auto makeJsonBufferWrapper(std::vector<char>& data) -> OutBufferWrapper { return OutBufferWrapper(&data); }
auto makeJsonBufferWrapper(OutBufferWrapper& bufferWrapper) -> OutBufferWrapper& { return bufferWrapper; }

template <typename Writer>
inline bool saveValue(FieldSize const& value, Writer& writer) {
    (void)value;
    (void)writer;
    return true;
}

template <typename Writer>
inline bool saveValue(const int16_t value, Writer& writer) {
    return writer.Int(value);
}

template <typename Writer>
inline bool saveValue(const uint16_t value, Writer& writer) {
    return writer.Uint(value);
}

template <typename Writer>
inline bool saveValue(const int32_t value, Writer& writer) {
    return writer.Int(value);
}

template <typename Writer>
inline bool saveValue(const uint32_t value, Writer& writer) {
    return writer.Uint(value);
}

template <typename Writer>
inline bool saveValue(const int64_t value, Writer& writer) {
    return writer.Int64(value);
}

template <typename Writer>
inline bool saveValue(const uint64_t value, Writer& writer) {
    return writer.Uint64(value);
}

template <typename Writer>
inline bool saveValue(const float value, Writer& writer) {
    return writer.Double(value);
}

template <typename Writer>
inline bool saveValue(const double value, Writer& writer) {
    return writer.Double(value);
}

template <typename Writer>
inline bool saveValue(const bool value, Writer& writer) {
    return writer.Bool(value);
}

template <typename Writer>
inline bool saveValue(const std::string& value, Writer& writer) {
    return writer.String(value.c_str(), value.size());
}

template <typename Writer>
inline bool saveValue(const char* value, Writer& writer) {
    return writer.String(value);
}

template <typename JsonValue>
inline bool loadValue(std::string& value, const JsonValue& json) {
    if (!json.IsString())
        return false;
    value = {json.GetString(), json.GetStringLength()};
    return true;
}

template <typename JsonValue>
inline bool loadValue(int16_t& value, const JsonValue& json) {
    if (!json.IsInt())
        return false;
    value = json.GetInt();
    return true;
}

template <typename JsonValue>
inline bool loadValue(int32_t& value, const JsonValue& json) {
    if (!json.IsInt())
        return false;
    value = json.GetInt();
    return true;
}

template <typename JsonValue>
inline bool loadValue(int64_t& value, const JsonValue& json) {
    if (!json.IsInt64())
        return false;
    value = json.GetInt64();
    return true;
}

template <typename JsonValue>
inline bool loadValue(uint16_t& value, const JsonValue& json) {
    if (!json.IsUint())
        return false;
    value = json.GetUint();
    return true;
}

template <typename JsonValue>
inline bool loadValue(uint32_t& value, const JsonValue& json) {
    if (!json.IsUint())
        return false;
    value = json.GetUint();
    return true;
}

template <typename JsonValue>
inline bool loadValue(uint64_t& value, const JsonValue& json) {
    if (!json.IsUint64())
        return false;
    value = json.GetUint64();
    return true;
}

template <typename JsonValue>
inline bool loadValue(float& value, const JsonValue& json) {
    if (!json.IsNumber())
        return false;
    value = json.GetFloat();
    return true;
}

template <typename JsonValue>
inline bool loadValue(double& value, const JsonValue& json) {
    if (!json.IsNumber())
        return false;
    value = json.GetDouble();
    return true;
}

template <typename JsonValue>
inline bool loadValue(bool& value, const JsonValue& json) {
    if (!json.IsBool())
        return false;
    value = json.GetBool();
    return true;
}

template <typename T, typename JsonValue>
inline bool loadValue(const NamedField<T>& value, const JsonValue& json) {
    std::unique_ptr<char[]> buf(new char[value.nameLen + 1]{0});
    std::memcpy(buf.get(), value.name, value.nameLen);
    auto it = json.FindMember(buf.get());
    if (it != json.MemberEnd() && it->value.IsArray()) {
        auto tmp = JsonDeserializer<JsonDocument::ConstArray>(it->value.GetArray());
        return tmp(value.value);
    }
    auto tmp = JsonDeserializer<::NekoProto::JsonValue>(it->value);
    return tmp(value.value);
}

template <typename JsonValue>
inline bool loadValue(FieldSize& value, const JsonValue& json) {
    if (!json.IsArray())
        return false;
    value.size = json.ArraySize();
    return true;
}

} // namespace

template <typename BufferType, typename Writer>
class JsonInputSerializer {
public:
    using BufferWrapperType = decltype(makeJsonBufferWrapper(std::declval<BufferType&>()));
    using WriterType        = Writer;

private:
    template <typename T, typename G, class enable = void>
    struct can_save : std::false_type {};

    template <typename T, typename G>
    struct can_save<T, G,
                    typename std::enable_if<
                        std::is_same<decltype(std::declval<T>().saveValue(std::declval<G>())), bool>::value>::type>
        : std::true_type {};

    template <typename T, class enable = void>
    struct is_named_field : std::false_type {};

    template <typename T>
    struct is_named_field<NamedField<T>, void> : std::true_type {};

    template <typename T, class enable = void>
    struct selector {
        static inline bool save(JsonInputSerializer& self, T& value) { return ::NekoProto::save(self, value); }
    };

    template <typename T>
    struct selector<T, typename std::enable_if<can_save<T, JsonInputSerializer>::value>::type> {
        static inline bool save(JsonInputSerializer& self, T& value) { return self.saveValue(value); }
    };

    template <typename T>
    struct selector<T, typename std::enable_if<can_serialize<T>::value>::type> {
        static inline bool save(JsonInputSerializer& self, T& value) {
            return value.serialize(JsonInputSerializer<BufferType&, Writer&>(self));
        }
    };

public:
    JsonInputSerializer(BufferType& out) NEKO_NOEXCEPT : mBuffer(makeJsonBufferWrapper(out)), mWriter(mBuffer) {
        mWriter.StartObject();
    }
    JsonInputSerializer(const JsonInputSerializer& other) NEKO_NOEXCEPT : mBuffer(other.mBuffer),
                                                                          mWriter(other.mWriter) {}
    JsonInputSerializer(JsonInputSerializer&& other) NEKO_NOEXCEPT : mBuffer(std::move(other.mBuffer)),
                                                                     mWriter(std::move(other.mWriter)) {}
    template <typename T>
    inline bool saveValue(const T& value) {
        return ::NekoProto::saveValue(value, mWriter);
    }
    template <typename T>
    inline bool saveValue(const NamedField<T>& value) {
        return mWriter.Key(value.name.c_str(), value.nameLen) && saveValue(value.value);
    }

    bool startArray(const std::size_t size) { return mWriter.StartArray(); }
    bool endArray() { return mWriter.EndArray(); }
    bool startObject() { return mWriter.StartObject(); }
    bool endObject() { return mWriter.EndObject(); }
    template <typename T>
    bool operator()(const T& value) {
#if NEKO_CPP_PLUS >= 17
        if constexpr (can_save<T, JsonInputSerializer>::value) {
            return saveValue(value);
        } else if constexpr (can_serialize<T>::value) {
            return value.serialize(JsonInputSerializer<BufferType&, Writer&>(*this));
        } else {
            return ::NekoProto::save(*this, value);
        }
#else
        return selector<T>::save(*this, value);
#endif
    }

    bool end() {
        auto ret = mWriter.EndObject();
        mWriter.Flush();
        return ret;
    }

private:
    JsonInputSerializer& operator=(const JsonInputSerializer&) = delete;
    JsonInputSerializer& operator=(JsonInputSerializer&&)      = delete;

private:
    WriterType mWriter;
    BufferWrapperType mBuffer;
};

template <typename ValueType, class enable>
class JsonDeserializer {
public:
    using NodeType = JsonDocument;

    template <typename T, typename G, class enable1 = void>
    struct can_load : std::false_type {};

    template <typename T, typename G>
    struct can_load<T, G,
                    typename std::enable_if<
                        std::is_same<decltype(std::declval<T>().loadValue(std::declval<G>())), bool>::value>::type>
        : std::true_type {};

    template <typename U, typename T, class enable2 = void>
    struct selector {
        static inline bool save(::NekoProto::JsonDeserializer<U>& self, T& value) { ::NekoProto::load(self, value); }
    };

    template <typename U>
    struct selector<U, FieldSize, void> {
        static inline bool save(::NekoProto::JsonDeserializer<U>& self, FieldSize& value) {
            value.size = self.size();
            return true;
        }
    };

    template <typename U, typename T>
    struct selector<U, NamedField<T>, void> {
        static inline bool save(::NekoProto::JsonDeserializer<U>& self, const NamedField<T>& value) {
            return self.loadValue(value);
        }
    };
    template <typename U, typename T>
    struct selector<U, T,
                    typename std::enable_if<can_load<T, typename ::NekoProto::JsonDeserializer<U>>::value>::type> {
        static inline bool save(::NekoProto::JsonDeserializer<U>& self, T& value) { return self.loadValue(value); }
    };

    template <typename U, typename T>
    struct selector<U, T, typename std::enable_if<can_serialize<T>::value>::type> {
        static inline bool save(::NekoProto::JsonDeserializer<U>& self, T& value) {
            return value.serialize(::NekoProto::JsonDeserializer<U>(self));
        }
    };

public:
    JsonDeserializer(const std::vector<char>& buf) { mNode.Parse(buf.data(), buf.size()); }
    JsonDeserializer(const char* buf, std::size_t size) { mNode.Parse(buf, size); }
    JsonDeserializer(rapidjson::IStreamWrapper& stream) { mNode.ParseStream(stream); }
    JsonDeserializer(JsonDeserializer&& other) NEKO_NOEXCEPT : mNode(std::move(other.mNode)) {}
    operator bool() const { return mNode.GetParseError() == rapidjson::kParseErrorNone; }
    std::size_t size() const { return mNode.Size(); }

    template <typename T>
    bool loadValue(T& value) const {
        return ::NekoProto::loadValue(value, mNode);
    }

    template <typename T>
    bool loadValue(T&& value) const {
        return ::NekoProto::loadValue(std::forward<T>(value), mNode);
    }

    template <typename T>
    bool operator()(T&& values) {
        return (selector<NodeType, T>::save(*this, std::forward<T>(values)));
    }

private:
    JsonDeserializer& operator=(const JsonDeserializer&) = delete;
    JsonDeserializer& operator=(JsonDeserializer&&)      = delete;

private:
    JsonDocument mNode;
};

template <>
class JsonDeserializer<JsonValue, void> {
public:
    using NodeType = JsonValue;

public:
    JsonDeserializer(const NodeType& node) NEKO_NOEXCEPT : mNode(node) {}
    operator bool() const { return true; }

    std::size_t size() const { return mNode.Size(); }
    template <typename T>
    bool loadValue(T& value) const {
        return ::NekoProto::loadValue(value, mNode);
    }
    template <typename T>
    bool loadValue(T&& value) const {
        return ::NekoProto::loadValue(std::forward<T>(value), mNode);
    }

    // template <typename T>
    // bool operator()(T& value) {
    //     return JsonDeserializer<JsonDocument>::selector<NodeType, T>::save(*this, value);
    // }

    template <typename T>
    bool operator()(T&& value) {
        return JsonDeserializer<JsonDocument>::selector<NodeType, T>::save(*this, std::forward<T>(value));
    }

private:
    JsonDeserializer& operator=(const JsonDeserializer&) = delete;
    JsonDeserializer& operator=(JsonDeserializer&&)      = delete;

private:
    const NodeType& mNode;
};

template <>
class JsonDeserializer<JsonDocument::ConstArray, void> {
public:
    using NodeType = JsonDocument::ConstArray;

public:
    JsonDeserializer(const NodeType& node) NEKO_NOEXCEPT : mNode(node), mIter(node.begin()) {}
    std::size_t size() const { return mNode.Size(); }

    template <typename T>
    bool loadValue(T& value) const {
        return ::NekoProto::loadValue(value, mIter);
    }
    template <typename T>
    bool loadValue(T&& value) const {
        return ::NekoProto::loadValue(std::forward<T>(value), mNode);
    }

    // template <typename T>
    // bool operator()(T& value) {
    //     if (std::is_same<T, FieldSize>::value) {
    //         return JsonDeserializer<JsonDocument>::selector<NodeType, T>::save(*this, value);
    //     } else {
    //         auto ret = JsonDeserializer<JsonDocument>::selector<NodeType, T>::save(*this, value);
    //         ++mIter;
    //         return ret;
    //     }
    // }

    template <typename T>
    bool operator()(T&& value) {
        if (std::is_same<T, FieldSize>::value) {
            return JsonDeserializer<JsonDocument>::selector<NodeType, T>::save(*this, std::forward<T>(value));
        } else {
            auto ret = JsonDeserializer<JsonDocument>::selector<NodeType, T>::save(*this, std::forward<T>(value));
            ++mIter;
            return ret;
        }
    }

private:
    JsonDeserializer& operator=(const JsonDeserializer&) = delete;
    JsonDeserializer& operator=(JsonDeserializer&&)      = delete;

private:
    const NodeType& mNode;
    mutable JsonDocument::Array::ConstValueIterator mIter;
};

struct JsonSerializer {
    template <typename T = std::vector<char>>
    using InputSerializer = JsonInputSerializer<T>;
    template <typename T = JsonDocument>
    using OutputSerializer = JsonDeserializer<T>;
};

#if NEKO_CPP_PLUS >= 20
template <typename WriterT, typename ValueT>
struct JsonConvert<WriterT, ValueT, std::u8string, void> {
    static bool toJsonValue(WriterT& writer, const std::u8string value) {
        return writer.String(reinterpret_cast<const char*>(value.data()), value.size(), true);
    }
    static bool fromJsonValue(std::u8string* dst, const ValueT& value) {
        NEKO_ASSERT(dst != nullptr, "dst is nullptr");
        if (!value.IsString()) {
            return false;
        }
        (*dst) = std::u8string(reinterpret_cast<const char8_t*>(value.GetString()), value.GetStringLength());
        return true;
    }
};
#endif
template <typename WriterT, typename ValueT, typename T>
struct JsonConvert<WriterT, ValueT, std::vector<T>, void> {
    static bool toJsonValue(WriterT& writer, const std::vector<T>& value) {
        bool ret = writer.StartArray();
        for (auto& v : value) {
            ret = JsonConvert<WriterT, ValueT, T>::toJsonValue(writer, v) && ret;
        }
        ret = writer.EndArray() && ret;
        return ret;
    }
    static bool fromJsonValue(std::vector<T>* dst, const ValueT& value) {
        NEKO_ASSERT(dst != nullptr, "dst is nullptr");
        if (!value.IsArray()) {
            return false;
        }
        dst->clear();
        for (auto& v : value.GetArray()) {
            T t;
            if (!JsonConvert<WriterT, ValueT, T>::fromJsonValue(&t, v)) {
                return false;
            }
            dst->push_back(t);
        }
        return true;
    }
};

#if NEKO_CPP_PLUS >= 17
template <typename WriterT, typename ValueT, typename... Ts>
struct JsonConvert<WriterT, ValueT, std::variant<Ts...>, void> {
    template <typename T, size_t N>
    static bool toJsonValueImp(WriterT& writer, const std::variant<Ts...>& value) {
        if (value.index() != N)
            return false;
        return JsonConvert<WriterT, ValueT, T>::toJsonValue(writer, std::get<N>(value));
    }

    template <size_t... Ns>
    static bool unfoldToJsonValue(WriterT& writer, const std::variant<Ts...>& value, std::index_sequence<Ns...>) {
        return (toJsonValueImp<std::variant_alternative_t<Ns, std::variant<Ts...>>, Ns>(writer, value) || ...);
    }

    static bool toJsonValue(WriterT& writer, const std::variant<Ts...>& value) {
        return unfoldToJsonValue(writer, value, std::make_index_sequence<sizeof...(Ts)>());
    }

    template <typename T, size_t N>
    static bool fromJsonValueImp(std::variant<Ts...>* dst, const ValueT& value) {
        T tmp = {};
        if (JsonConvert<WriterT, ValueT, T>::fromJsonValue(&tmp, value)) {
            *dst = tmp;
            return true;
        }
        return false;
    }

    template <size_t... Ns>
    static bool unfoldFromJsonValue(std::variant<Ts...>* dst, const ValueT& value, std::index_sequence<Ns...>) {
        return (fromJsonValueImp<std::variant_alternative_t<Ns, std::variant<Ts...>>, Ns>(dst, value) || ...);
    }

    static bool fromJsonValue(std::variant<Ts...>* dst, const ValueT& value) {
        NEKO_ASSERT(dst != nullptr, "[qBittorrent] std::variant dst is nullptr in from json value");
        return unfoldFromJsonValue(dst, value, std::make_index_sequence<sizeof...(Ts)>());
    }
};
#endif

NEKO_END_NAMESPACE