#pragma once

#include "nekoproto/global/global.hpp"

#if defined(NEKO_PROTO_ENABLE_RAPIDJSON)
#include "nekoproto/global/log.hpp"

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/rapidjson.h>
#include <type_traits>

#ifdef _WIN32
#pragma push_macro("GetObject")
#ifdef GetObject
#undef GetObject
#endif
#endif

#include "nekoproto/serialization/error.hpp"

namespace nekoproto {
namespace rapid {
class Writer {
public:
    using RawValueType = rapidjson::Document;

    struct OutputArrayType {
        rapidjson::Value* value;
    };

    struct OutputObjectType {
        rapidjson::Value* value;
    };

    struct OutputValueType {
        rapidjson::Value* value;
    };

    Writer() : doc_(rapidjson::kNullType) {}
    ~Writer() = default;

    static auto parseRawValue(std::string_view text, RawValueType& value) -> bool {
        value.Parse(text.data(), text.size());
        return !value.HasParseError();
    }

    auto doc() -> rapidjson::Document* { return &doc_; }

    auto arrayAsRoot(const std::size_t size) noexcept -> OutputArrayType {
        doc_.SetArray();
        if (size != static_cast<std::size_t>(-1)) {
            doc_.Reserve(static_cast<rapidjson::SizeType>(size), doc_.GetAllocator());
        }
        return {&doc_};
    }
    auto objectAsRoot(const std::size_t size) noexcept -> OutputObjectType {
        doc_.SetObject();
        if (size != static_cast<std::size_t>(-1)) {
            doc_.MemberReserve(static_cast<rapidjson::SizeType>(size), doc_.GetAllocator());
        }
        return {&doc_};
    }
    auto nullAsRoot() noexcept -> OutputValueType {
        doc_.SetNull();
        return {&doc_};
    }
    auto valueAsRoot(const rapidjson::Value& value) noexcept -> OutputValueType {
        doc_.CopyFrom(value, doc_.GetAllocator());
        return {&doc_};
    }
    template <typename T>
    auto valueAsRoot(const T& value) noexcept -> OutputValueType {
        auto val = fromBasicType(value);
        doc_.Swap(val);
        return {&doc_};
    }
    auto addArrayToArray(const std::size_t size, OutputArrayType* parent) -> OutputArrayType {
        rapidjson::Value child(rapidjson::kArrayType);
        if (size != static_cast<std::size_t>(-1)) {
            child.Reserve(static_cast<rapidjson::SizeType>(size), doc_.GetAllocator());
        }

        parent->value->PushBack(child, doc_.GetAllocator());

        auto& inserted = (*parent->value)[parent->value->Size() - 1];
        return {&inserted};
    }
    auto addArrayToObject(std::string_view name, const std::size_t size, OutputObjectType* parent) -> OutputArrayType {
        rapidjson::Value key;
        key.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()), doc_.GetAllocator());

        rapidjson::Value child(rapidjson::kArrayType);
        if (size != static_cast<std::size_t>(-1)) {
            child.Reserve(static_cast<rapidjson::SizeType>(size), doc_.GetAllocator());
        }

        parent->value->AddMember(key, child, doc_.GetAllocator());

        auto member = parent->value->MemberEnd();
        --member;
        return {&member->value};
    }
    auto addObjectToArray(const std::size_t size, OutputArrayType* parent) -> OutputObjectType {
        rapidjson::Value child(rapidjson::kObjectType);
        if (size != static_cast<std::size_t>(-1)) {
            child.MemberReserve(static_cast<rapidjson::SizeType>(size), doc_.GetAllocator());
        }

        parent->value->PushBack(child, doc_.GetAllocator());

        auto& inserted = (*parent->value)[parent->value->Size() - 1];
        return {&inserted};
    }
    auto addObjectToObject(std::string_view name, const std::size_t size, OutputObjectType* parent) -> OutputObjectType {
        rapidjson::Value key;
        key.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()), doc_.GetAllocator());

        rapidjson::Value child(rapidjson::kObjectType);
        if (size != static_cast<std::size_t>(-1)) {
            child.MemberReserve(static_cast<rapidjson::SizeType>(size), doc_.GetAllocator());
        }

        parent->value->AddMember(key, child, doc_.GetAllocator());

        auto member = parent->value->MemberEnd();
        --member;
        return {&member->value};
    }
    template <typename T>
    auto addValueToArray(const T& value, OutputArrayType* parent) -> OutputValueType {
        auto val = fromBasicType(value);
        parent->value->PushBack(val, doc_.GetAllocator());

        auto& inserted = (*parent->value)[parent->value->Size() - 1];
        return {&inserted};
    }
    auto addValueToArray(const rapidjson::Value& value, OutputArrayType* parent) -> OutputValueType {
        rapidjson::Value val;
        val.CopyFrom(value, doc_.GetAllocator());
        parent->value->PushBack(val, doc_.GetAllocator());

        auto& inserted = (*parent->value)[parent->value->Size() - 1];
        return {&inserted};
    }
    template <typename T>
    auto addValueToObject(std::string_view name, const T& value, OutputObjectType* parent) -> OutputValueType {
        rapidjson::Value key;
        key.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()), doc_.GetAllocator());

        auto val = fromBasicType(value);
        parent->value->AddMember(key, val, doc_.GetAllocator());

        auto member = parent->value->MemberEnd();
        --member;
        return {&member->value};
    }
    auto addValueToObject(std::string_view name, const rapidjson::Value& value, OutputObjectType* parent) -> OutputValueType {
        rapidjson::Value key;
        key.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()), doc_.GetAllocator());

        rapidjson::Value val;
        val.CopyFrom(value, doc_.GetAllocator());
        parent->value->AddMember(key, val, doc_.GetAllocator());

        auto member = parent->value->MemberEnd();
        --member;
        return {&member->value};
    }
    auto addNullToArray(OutputArrayType* parent) -> OutputValueType {
        rapidjson::Value val(rapidjson::kNullType);
        parent->value->PushBack(val, doc_.GetAllocator());

        auto& inserted = (*parent->value)[parent->value->Size() - 1];
        return {&inserted};
    }
    auto addNullToObject(std::string_view name, OutputObjectType* parent) -> OutputValueType {
        rapidjson::Value key;
        key.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()), doc_.GetAllocator());

        rapidjson::Value val(rapidjson::kNullType);
        parent->value->AddMember(key, val, doc_.GetAllocator());

        auto member = parent->value->MemberEnd();
        --member;
        return {&member->value};
    }
    void endArray(OutputArrayType* /*unused*/) noexcept {}
    void endObject(OutputObjectType* /*unused*/) noexcept {}

private:
    template <typename T>
    auto fromBasicType(const T& value) -> rapidjson::Value {
        using U = std::remove_cv_t<std::remove_reference_t<T>>;
        rapidjson::Value val;
        if constexpr (std::is_same_v<U, std::string>) {
            val.SetString(value.data(), static_cast<rapidjson::SizeType>(value.size()), doc_.GetAllocator());
        } else if constexpr (std::is_same_v<U, std::string_view>) {
            val.SetString(value.data(), static_cast<rapidjson::SizeType>(value.size()), doc_.GetAllocator());
        } else if constexpr (std::is_base_of_v<rapidjson::Value, U>) {
            val.CopyFrom(value, doc_.GetAllocator());
        } else if constexpr (std::is_same_v<U, bool>) {
            val.SetBool(value);
        } else if constexpr (std::is_floating_point_v<U>) {
            val.SetDouble(static_cast<double>(value));
        } else if constexpr (std::is_enum_v<U>) {
            using I = std::underlying_type_t<U>;
            return fromBasicType(static_cast<I>(value));
        } else if constexpr (std::is_integral_v<U> && std::is_signed_v<U>) {
            if constexpr (sizeof(U) <= sizeof(int)) {
                val.SetInt(static_cast<int>(value));
            } else {
                val.SetInt64(static_cast<std::int64_t>(value));
            }
        } else if constexpr (std::is_integral_v<U> && std::is_unsigned_v<U>) {
            if constexpr (sizeof(U) <= sizeof(unsigned)) {
                val.SetUint(static_cast<unsigned>(value));
            } else {
                val.SetUint64(static_cast<std::uint64_t>(value));
            }
        } else {
            static_assert(std::is_same_v<U, void>, "Unsupported JSON basic type");
        }

        return val;
    }

private:
    rapidjson::Document doc_;
};
} // namespace rapid
} // namespace nekoproto
#ifdef _WIN32
#pragma pop_macro("GetObject")
#endif
#endif // NEKO_PROTO_ENABLE_RAPIDJSON
