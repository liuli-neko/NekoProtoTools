/**
 * @file reflection_serializer.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-06-19
 *
 * @copyright Copyright (c) 2024
 *
 */

#pragma once

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeinfo>

#include "nekoproto/global/global.hpp"
#include "nekoproto/global/log.hpp"

#include "nekoproto/serialization/types/struct_unwrap.hpp"

NEKO_BEGIN_NAMESPACE

template <typename T>
struct NameValuePair;

namespace detail {
template <typename T>
struct is_name_value_pair { // NOLINT(readability-identifier-naming)
    static constexpr bool Value = false;
};

template <typename T>
struct is_name_value_pair<NameValuePair<T>> {
    static constexpr bool Value = true;
};

class ReflectionFieldBase {
public:
    ReflectionFieldBase() NEKO_NOEXCEPT                          = default;
    virtual ~ReflectionFieldBase() NEKO_NOEXCEPT                 = default;
    virtual const NEKO_STRING_VIEW& name() const NEKO_NOEXCEPT   = 0;
    virtual const std::type_info& typeInfo() const NEKO_NOEXCEPT = 0;
};

template <typename T>
class ReflectionField : public ReflectionFieldBase {
public:
    explicit ReflectionField(const NEKO_STRING_VIEW& name, T* value) : mValue(value), mName(name) {
        NEKO_ASSERT(value != nullptr, "ReflectionSerializer", "can not make reflection object {} for nullptr", name);
    }
    const T& getField() const NEKO_NOEXCEPT { return *mValue; }
    void setField(const T& value) NEKO_NOEXCEPT { (*mValue) = value; }
    const NEKO_STRING_VIEW& name() const NEKO_NOEXCEPT override { return mName; }
    const std::type_info& typeInfo() const NEKO_NOEXCEPT override { return typeid(T); }

private:
    T* const mValue;
    const NEKO_STRING_VIEW mName;
};

class ReflectionObject {
public:
    ReflectionObject()                        = default;
    ReflectionObject(const ReflectionObject&) = delete;
    ReflectionObject(ReflectionObject&& other) : mFields(std::move(other.mFields)) {}
    ReflectionObject& operator=(const ReflectionObject&) = delete;
    ReflectionObject& operator=(ReflectionObject&& other) {
        if (this != &other) {
            mFields = std::move(other.mFields);
        }
        return *this;
    }

    inline ~ReflectionObject() { clear(); }
    inline void clear() NEKO_NOEXCEPT { mFields.clear(); }

    template <typename T>
    T getField(const NEKO_STRING_VIEW& name, const T& defaultValue) const NEKO_NOEXCEPT {
        auto it = mFields.find(name);
        if (it == mFields.end()) {
            NEKO_LOG_ERROR("ReflectionSerializer", "field {} not found.", name);
            return defaultValue;
        }
        if (typeid(T) != it->second->typeInfo()) {
            NEKO_LOG_ERROR("ReflectionSerializer", "field {} type mismatch, expected {} but got {}.", name,
                           typeid(T).name(), it->second->typeInfo().name());
            return defaultValue;
        }
        auto reflectionField  = dynamic_cast<ReflectionField<T>*>(it->second.get());
        auto reflectionField1 = dynamic_cast<ReflectionField<const T>*>(it->second.get());
        if (reflectionField == nullptr && reflectionField1 == nullptr) {
            NEKO_LOG_ERROR("ReflectionSerializer", "field {} type mismatch, expected {} but got {}.", name,
                           typeid(T).name(), it->second->typeInfo().name());
            return defaultValue;
        }
        NEKO_ASSERT(((reflectionField != nullptr ? reflectionField->name() : reflectionField1->name()) == name),
                    "ReflectionSerializer", "field name mismatch");
        return reflectionField != nullptr ? reflectionField->getField() : reflectionField1->getField();
    }

    template <typename T>
    bool getField(const NEKO_STRING_VIEW& name, T* result) const NEKO_NOEXCEPT {
        auto it = mFields.find(name);
        if (it == mFields.end()) {
            NEKO_LOG_ERROR("ReflectionSerializer", "field {} not found.", name);
            return false;
        }
        if (typeid(T) != it->second->typeInfo()) {
            NEKO_LOG_ERROR("ReflectionSerializer", "field {} type mismatch, expected {} but got {}.", name,
                           typeid(T).name(), it->second->typeInfo().name());
            return false;
        }
        auto reflectionField = dynamic_cast<ReflectionField<T>*>(it->second.get());
        if (reflectionField == nullptr) {
            NEKO_LOG_ERROR("ReflectionSerializer", "field {} type mismatch, expected {} but got {}.", name,
                           typeid(T).name(), it->second->typeInfo().name());
            return false;
        }
        NEKO_ASSERT(reflectionField->name() == name, "ReflectionSerializer", "field name mismatch");
        if (result != nullptr) {
            *result = reflectionField->getField();
        }
        return true;
    }
    template <typename T>
    bool setField(const NEKO_STRING_VIEW& name, const T& value) NEKO_NOEXCEPT {
        auto it = mFields.find(name);
        if (it == mFields.end()) {
            NEKO_LOG_ERROR("ReflectionSerializer", "field {} not found.", name);
            return false;
        }
        if (typeid(T) != it->second->typeInfo()) {
            NEKO_LOG_ERROR("ReflectionSerializer", "field {} type mismatch, expected {} but got {}.", name,
                           typeid(T).name(), it->second->typeInfo().name());
            return false;
        }
        auto reflectionField = dynamic_cast<ReflectionField<T>*>(it->second.get());
        if (reflectionField == nullptr) {
            NEKO_LOG_ERROR("ReflectionSerializer", "field {} type mismatch, expected {} but got {}.", name,
                           typeid(T).name(), it->second->typeInfo().name());
            return false;
        }
        reflectionField->setField(value);
        return true;
    }

    template <typename T>
    ReflectionField<T>* bindField(const NEKO_STRING_VIEW& name, T* value) {
        if (value == nullptr) {
            return nullptr;
        }
        auto it = mFields.find(name);
        if (it == mFields.end()) {
            mFields.insert(std::make_pair<const NEKO_STRING_VIEW&, std::unique_ptr<ReflectionFieldBase>>(
                name, std::make_unique<ReflectionField<T>>(name, value)));
            return static_cast<ReflectionField<T>*>(mFields.find(name)->second.get());
        }
        NEKO_LOG_WARN("ReflectionSerializer", "field {} already exists, duplicate field will be overwritten.", name);
        it->second = std::make_unique<ReflectionField<T>>(name, value);
        return static_cast<ReflectionField<T>*>(it->second.get());
    }

private:
    std::map<NEKO_STRING_VIEW, std::unique_ptr<ReflectionFieldBase>> mFields;
};
} // namespace detail

class ReflectionSerializer {
public:
    ReflectionSerializer() { mObject.clear(); }
    ReflectionSerializer(const ReflectionSerializer&)            = delete;
    ReflectionSerializer& operator=(const ReflectionSerializer&) = delete;
    ReflectionSerializer(ReflectionSerializer&& other) : mObject(std::move(other.mObject)) {}
    ReflectionSerializer& operator=(ReflectionSerializer&& other) {
        mObject = std::move(other.mObject);
        return *this;
    }
    ~ReflectionSerializer() = default;
    operator bool() const { return true; }
    bool startNode() { return true; }  // NOLINT
    bool finishNode() { return true; } // NOLINT
    template <typename T>
        requires traits::can_be_serializable<T>
    static ReflectionSerializer reflection(T& obj) {
        ReflectionSerializer rs;
        auto ret = true;
        if constexpr (traits::has_function_load<T, ReflectionSerializer>) {
            ret = load(rs, obj);
        } else if constexpr (traits::has_method_load<T, ReflectionSerializer>) {
            ret = obj.load(rs);
        } else {
            static_assert(traits::has_function_load<T, ReflectionSerializer>,
                          "Reflection operations cannot be performed on this type");
        }
        NEKO_ASSERT(ret, "ReflectionSerializer", "{} get reflection object error", detail::class_nameof<T>);
        NEKO_ASSERT(rs.getObject() != nullptr, "ReflectionSerializer", "reflection object is nullptr");
        return rs;
    }

    template <typename... Ts>
    bool operator()(const Ts&... fields) {
        return _process(fields...);
    }

    inline detail::ReflectionObject* getObject() { return &mObject; }

private:
    template <typename T, typename... Ts>
    bool _process(const T& field, const Ts&... fields) {
        return _process(field) && _process(fields...);
    }
    template <typename T>
    bool _process(const NameValuePair<T>& field) {
        return nullptr != mObject.bindField(NEKO_STRING_VIEW(field.name, field.nameLen), &field.value);
    }
    template <typename T, typename std::enable_if<!detail::is_name_value_pair<T>::Value, char>::type = 0>
    bool _process(const T& /*unused*/) {
        NEKO_LOG_WARN("ReflectionSerializer", "Types other than NameValuePair are not supported reflection");
        return true;
    }

private:
    detail::ReflectionObject mObject;
};

NEKO_END_NAMESPACE