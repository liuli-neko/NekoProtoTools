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

NEKO_BEGIN_NAMESPACE
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

    Writer() : mDoc(rapidjson::kNullType) {}
    ~Writer() = default;

    static bool parseRawValue(std::string_view text, RawValueType& value) {
        value.Parse(text.data(), text.size());
        return !value.HasParseError();
    }

    rapidjson::Document* doc() { return &mDoc; }

    OutputArrayType arrayAsRoot(const std::size_t size) noexcept {
        mDoc.SetArray();
        if (size != static_cast<std::size_t>(-1)) {
            mDoc.Reserve(static_cast<rapidjson::SizeType>(size), mDoc.GetAllocator());
        }
        return {&mDoc};
    }
    OutputObjectType objectAsRoot(const std::size_t size) noexcept {
        mDoc.SetObject();
        if (size != static_cast<std::size_t>(-1)) {
            mDoc.MemberReserve(static_cast<rapidjson::SizeType>(size), mDoc.GetAllocator());
        }
        return {&mDoc};
    }
    OutputValueType nullAsRoot() noexcept {
        mDoc.SetNull();
        return {&mDoc};
    }
    OutputValueType valueAsRoot(const rapidjson::Value& value) noexcept {
        mDoc.CopyFrom(value, mDoc.GetAllocator());
        return {&mDoc};
    }
    template <typename T>
    OutputValueType valueAsRoot(const T& value) noexcept {
        auto val = _fromBasicType(value);
        mDoc.Swap(val);
        return {&mDoc};
    }
    OutputArrayType addArrayToArray(const std::size_t size, OutputArrayType* parent) {
        rapidjson::Value child(rapidjson::kArrayType);
        if (size != static_cast<std::size_t>(-1)) {
            child.Reserve(static_cast<rapidjson::SizeType>(size), mDoc.GetAllocator());
        }

        parent->value->PushBack(child, mDoc.GetAllocator());

        auto& inserted = (*parent->value)[parent->value->Size() - 1];
        return {&inserted};
    }
    OutputArrayType addArrayToObject(std::string_view name, const std::size_t size, OutputObjectType* parent) {
        rapidjson::Value key;
        key.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()), mDoc.GetAllocator());

        rapidjson::Value child(rapidjson::kArrayType);
        if (size != static_cast<std::size_t>(-1)) {
            child.Reserve(static_cast<rapidjson::SizeType>(size), mDoc.GetAllocator());
        }

        parent->value->AddMember(key, child, mDoc.GetAllocator());

        auto member = parent->value->MemberEnd();
        --member;
        return {&member->value};
    }
    OutputObjectType addObjectToArray(const std::size_t size, OutputArrayType* parent) {
        rapidjson::Value child(rapidjson::kObjectType);
        if (size != static_cast<std::size_t>(-1)) {
            child.MemberReserve(static_cast<rapidjson::SizeType>(size), mDoc.GetAllocator());
        }

        parent->value->PushBack(child, mDoc.GetAllocator());

        auto& inserted = (*parent->value)[parent->value->Size() - 1];
        return {&inserted};
    }
    OutputObjectType addObjectToObject(std::string_view name, const std::size_t size, OutputObjectType* parent) {
        rapidjson::Value key;
        key.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()), mDoc.GetAllocator());

        rapidjson::Value child(rapidjson::kObjectType);
        if (size != static_cast<std::size_t>(-1)) {
            child.MemberReserve(static_cast<rapidjson::SizeType>(size), mDoc.GetAllocator());
        }

        parent->value->AddMember(key, child, mDoc.GetAllocator());

        auto member = parent->value->MemberEnd();
        --member;
        return {&member->value};
    }
    template <typename T>
    OutputValueType addValueToArray(const T& value, OutputArrayType* parent) {
        auto val = _fromBasicType(value);
        parent->value->PushBack(val, mDoc.GetAllocator());

        auto& inserted = (*parent->value)[parent->value->Size() - 1];
        return {&inserted};
    }
    OutputValueType addValueToArray(const rapidjson::Value& value, OutputArrayType* parent) {
        rapidjson::Value val;
        val.CopyFrom(value, mDoc.GetAllocator());
        parent->value->PushBack(val, mDoc.GetAllocator());

        auto& inserted = (*parent->value)[parent->value->Size() - 1];
        return {&inserted};
    }
    template <typename T>
    OutputValueType addValueToObject(std::string_view name, const T& value, OutputObjectType* parent) {
        rapidjson::Value key;
        key.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()), mDoc.GetAllocator());

        auto val = _fromBasicType(value);
        parent->value->AddMember(key, val, mDoc.GetAllocator());

        auto member = parent->value->MemberEnd();
        --member;
        return {&member->value};
    }
    OutputValueType addValueToObject(std::string_view name, const rapidjson::Value& value, OutputObjectType* parent) {
        rapidjson::Value key;
        key.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()), mDoc.GetAllocator());

        rapidjson::Value val;
        val.CopyFrom(value, mDoc.GetAllocator());
        parent->value->AddMember(key, val, mDoc.GetAllocator());

        auto member = parent->value->MemberEnd();
        --member;
        return {&member->value};
    }
    OutputValueType addNullToArray(OutputArrayType* parent) {
        rapidjson::Value val(rapidjson::kNullType);
        parent->value->PushBack(val, mDoc.GetAllocator());

        auto& inserted = (*parent->value)[parent->value->Size() - 1];
        return {&inserted};
    }
    OutputValueType addNullToObject(std::string_view name, OutputObjectType* parent) {
        rapidjson::Value key;
        key.SetString(name.data(), static_cast<rapidjson::SizeType>(name.size()), mDoc.GetAllocator());

        rapidjson::Value val(rapidjson::kNullType);
        parent->value->AddMember(key, val, mDoc.GetAllocator());

        auto member = parent->value->MemberEnd();
        --member;
        return {&member->value};
    }
    void endArray(OutputArrayType* /*unused*/) noexcept {}
    void endObject(OutputObjectType* /*unused*/) noexcept {}

private:
    template <typename T>
    rapidjson::Value _fromBasicType(const T& value) {
        using U = std::remove_cv_t<std::remove_reference_t<T>>;
        rapidjson::Value val;
        if constexpr (std::is_same_v<U, std::string>) {
            val.SetString(value.data(), static_cast<rapidjson::SizeType>(value.size()), mDoc.GetAllocator());
        } else if constexpr (std::is_same_v<U, std::string_view>) {
            val.SetString(value.data(), static_cast<rapidjson::SizeType>(value.size()), mDoc.GetAllocator());
        } else if constexpr (std::is_base_of_v<rapidjson::Value, U>) {
            val.CopyFrom(value, mDoc.GetAllocator());
        } else if constexpr (std::is_same_v<U, bool>) {
            val.SetBool(value);
        } else if constexpr (std::is_floating_point_v<U>) {
            val.SetDouble(static_cast<double>(value));
        } else if constexpr (std::is_enum_v<U>) {
            using I = std::underlying_type_t<U>;
            return _fromBasicType(static_cast<I>(value));
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
    rapidjson::Document mDoc;
};
} // namespace rapid
NEKO_END_NAMESPACE
#ifdef _WIN32
#pragma pop_macro("GetObject")
#endif
#endif // NEKO_PROTO_ENABLE_RAPIDJSON
