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

#include <cstddef>
#include <map>
#include <string>
#include <typeinfo>
#include <memory>

#include "global.hpp"

NEKO_BEGIN_NAMESPACE

template <typename T>
struct NamedField;

class ReflectionFieldBase {
public:
    ReflectionFieldBase()                                        = default;
    virtual ~ReflectionFieldBase()                               = default;
    virtual const NEKO_STRING_VIEW& name() const NEKO_NOEXCEPT   = 0;
    virtual const std::type_info& typeInfo() const NEKO_NOEXCEPT = 0;
};

template <typename T>
class ReflectionField : public ReflectionFieldBase {
public:
    explicit ReflectionField(const NEKO_STRING_VIEW& name, T* value) : mValue(value), mName(name) {
        if (value == nullptr) {
            throw std::invalid_argument("can not make reflection object " + std::string(name) + " for nullptr");
        }
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
    ReflectionObject() = default;
    inline ~ReflectionObject() { clear(); }
    inline void clear() NEKO_NOEXCEPT {
        for (auto& it : mFields) {
            delete it.second;
            it.second = nullptr;
        }
        mFields.clear();
    }

    template <typename T>
    T getField(const NEKO_STRING_VIEW& name, const T& defaultValue) const NEKO_NOEXCEPT {
        auto it = mFields.find(name);
        if (it == mFields.end()) {
            NEKO_LOG_ERROR("[ReflectionObject] field {} not found.", name);
            return defaultValue;
        }
        if (typeid(T) != it->second->typeInfo()) {
            NEKO_LOG_ERROR("[ReflectionObject] field {} type mismatch, expected {} but got {}.", name, typeid(T).name(),
                           it->second->typeInfo().name());
            return defaultValue;
        }
        auto reflectionField  = dynamic_cast<ReflectionField<T>*>(it->second);
        auto reflectionField1 = dynamic_cast<ReflectionField<const T>*>(it->second);
        if (reflectionField == nullptr && reflectionField1 == nullptr) {
            NEKO_LOG_ERROR("[ReflectionObject] field {} type mismatch, expected {} but got {}.", name, typeid(T).name(),
                           it->second->typeInfo().name());
            return defaultValue;
        }
        NEKO_ASSERT(((reflectionField != nullptr ? reflectionField->name() : reflectionField1->name()) == name),
                    "field name mismatch");
        return reflectionField != nullptr ? reflectionField->getField() : reflectionField1->getField();
    }

    template <typename T>
    bool getField(const NEKO_STRING_VIEW& name, T* result) const NEKO_NOEXCEPT {
        auto it = mFields.find(name);
        if (it == mFields.end()) {
            NEKO_LOG_ERROR("[ReflectionObject] field {} not found.", name);
            return false;
        }
        if (typeid(T) != it->second->typeInfo()) {
            NEKO_LOG_ERROR("[ReflectionObject] field {} type mismatch, expected {} but got {}.", name, typeid(T).name(),
                           it->second->typeInfo().name());
            return false;
        }
        auto reflectionField = dynamic_cast<ReflectionField<T>*>(it->second);
        if (reflectionField == nullptr) {
            NEKO_LOG_ERROR("[ReflectionObject] field {} type mismatch, expected {} but got {}.", name, typeid(T).name(),
                           it->second->typeInfo().name());
            return false;
        }
        NEKO_ASSERT(reflectionField->name() == name, "field name mismatch");
        if (result != nullptr) {
            *result = reflectionField->getField();
        }
        return true;
    }
    template <typename T>
    bool setField(const NEKO_STRING_VIEW& name, const T& value) NEKO_NOEXCEPT {
        auto it = mFields.find(name);
        if (it == mFields.end()) {
            NEKO_LOG_ERROR("[ReflectionObject] field {} not found.", name);
            return false;
        }
        if (typeid(T) != it->second->typeInfo()) {
            NEKO_LOG_ERROR("[ReflectionObject] field {} type mismatch, expected {} but got {}.", name, typeid(T).name(),
                           it->second->typeInfo().name());
            return false;
        }
        auto reflectionField = dynamic_cast<ReflectionField<T>*>(it->second);
        if (reflectionField == nullptr) {
            NEKO_LOG_ERROR("[ReflectionObject] field {} type mismatch, expected {} but got {}.", name, typeid(T).name(),
                           it->second->typeInfo().name());
            return false;
        }
        reflectionField->setField(value);
        return true;
    }

    template <typename T>
    ReflectionField<T>* bindField(const NEKO_STRING_VIEW& name, T* value) {
        auto it = mFields.find(name);
        if (it == mFields.end()) {
            mFields.insert(std::make_pair(name, new ReflectionField<T>(name, value)));
            return static_cast<ReflectionField<T>*>(mFields.find(name)->second);
        }
        NEKO_LOG_WARN("[ReflectionObject] field {} already exists, duplicate field will be overwritten.", name);
        delete it->second;
        it->second = new ReflectionField<T>(name, value);
        return static_cast<ReflectionField<T>*>(it->second);
    }

private:
    std::map<NEKO_STRING_VIEW, ReflectionFieldBase*> mFields;
};

class ReflectionSerializer {
public:
    ReflectionSerializer() { mObject.clear(); }
    ~ReflectionSerializer() = default;
    operator bool() const { return true; }

    template <typename T>
    inline bool operator()(const NamedField<T> &field) {
        return nullptr != mObject.bindField(NEKO_STRING_VIEW(field.name, field.nameLen), std::addressof(field.value));
    }

    inline ReflectionObject* getObject() { return &mObject; }

private:
    ReflectionObject mObject;
};

NEKO_END_NAMESPACE