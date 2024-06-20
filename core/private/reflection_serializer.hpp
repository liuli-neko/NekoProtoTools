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
#include <string>
#include <typeinfo>
#include <map>

#include "global.hpp"

NEKO_BEGIN_NAMESPACE

class ReflectionFieldBase {
public:
    ReflectionFieldBase() = default;
    virtual ~ReflectionFieldBase() = default;
    virtual const NEKO_STRING_VIEW& getFieldName() const NEKO_NOEXCEPT = 0;
    virtual const std::type_info& getFieldType() const NEKO_NOEXCEPT = 0;
};

template <typename T>
class ReflectionField : public ReflectionFieldBase {
public:
    explicit ReflectionField(const NEKO_STRING_VIEW &name, T* value) : mValue(value), mName(name) {}
    const T& getField() const NEKO_NOEXCEPT { return *mValue; }
    void setField(const T& value) NEKO_NOEXCEPT { (*mValue) = value; }
    const NEKO_STRING_VIEW& getFieldName() const NEKO_NOEXCEPT override { return mName; }
    const std::type_info& getFieldType() const NEKO_NOEXCEPT override { return typeid(T); }
private:
    T *const mValue;
    const NEKO_STRING_VIEW mName;
};

class ReflectionObject {
public:
    ReflectionObject() = default;
    inline ~ReflectionObject() {
        clear();
    }
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
        if (typeid(T) != it->second->getFieldType()) {
            NEKO_LOG_ERROR("[ReflectionObject] field {} type mismatch, expected {} but got {}.", name, typeid(T).name(), it->second->getFieldType().name());
            return defaultValue;
        }
        auto reflectionField = dynamic_cast<ReflectionField<T>*>(it->second);
        if (reflectionField == nullptr) {
            NEKO_LOG_ERROR("[ReflectionObject] field {} type mismatch, expected {} but got {}.", name, typeid(T).name(), it->second->getFieldType().name());
            return defaultValue;
        }
        return reflectionField->getField();
    }

    template <typename T>
    bool getField(const NEKO_STRING_VIEW& name, T* result) const NEKO_NOEXCEPT {
        auto it = mFields.find(name);
        if (it == mFields.end()) {
            NEKO_LOG_ERROR("[ReflectionObject] field {} not found.", name);
            return false;
        }
        if (typeid(T) != it->second->getFieldType()) {
            NEKO_LOG_ERROR("[ReflectionObject] field {} type mismatch, expected {} but got {}.", name, typeid(T).name(), it->second->getFieldType().name());
            return false;
        }
        auto reflectionField = dynamic_cast<ReflectionField<T>*>(it->second);
        if (reflectionField == nullptr) {
            NEKO_LOG_ERROR("[ReflectionObject] field {} type mismatch, expected {} but got {}.", name, typeid(T).name(), it->second->getFieldType().name());
            return false;
        }
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
        if (typeid(T) != it->second->getFieldType()) {
            NEKO_LOG_ERROR("[ReflectionObject] field {} type mismatch, expected {} but got {}.", name, typeid(T).name(), it->second->getFieldType().name());
            return false;
        }
        auto reflectionField = dynamic_cast<ReflectionField<T>*>(it->second);
        if (reflectionField == nullptr) {
            NEKO_LOG_ERROR("[ReflectionObject] field {} type mismatch, expected {} but got {}.", name, typeid(T).name(), it->second->getFieldType().name());
            return false;
        }
        reflectionField->setField(value);
        return true;
    }

    template <typename T>
    ReflectionField<T>* bindField(const NEKO_STRING_VIEW & name, T* value) NEKO_NOEXCEPT {
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
    ReflectionSerializer() = default;
    ~ReflectionSerializer() = default;

    inline void start() NEKO_NOEXCEPT {
        mObject.clear();
    }
    template <typename T>
    bool get(const char* name, const size_t len, T* value) NEKO_NOEXCEPT {
        return mObject.bindField(NEKO_STRING_VIEW(name, len) , value) != nullptr;
    }
    inline ReflectionObject* getObject() {
        return &mObject;
    }

private:
    ReflectionObject mObject;
};

NEKO_END_NAMESPACE