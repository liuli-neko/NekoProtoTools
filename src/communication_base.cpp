/**
 * @file communication_base.cpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-06-18
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "nekoproto/communication/communication_base.hpp"

#include <ilias/detail/expected.hpp>
#include <ilias/task.hpp>
#include <regex>


#ifdef NEKO_COMMUNICATION_DEBUG
#include "nekoproto/proto/to_string.hpp"
#include "nekoproto/proto/types/binary_data.hpp"
#endif

using namespace ILIAS_NAMESPACE;

NEKO_BEGIN_NAMESPACE

auto ErrorCategory::instance() -> const ErrorCategory& {
    static ErrorCategory instance;
    return instance;
}

auto ErrorCategory::message(int64_t value) const -> std::string {
    switch (value) {
#define NEKO_CHANNEL_ERROR(name, code, message, _)                                                                     \
    case code:                                                                                                         \
        return message;
        NEKO_CHANNEL_ERROR_CODE_TABLE
#undef NEKO_CHANNEL_ERROR
    default:
        return "unknown error";
    }
}

auto ErrorCategory::name() const -> std::string_view { return "NekoCommunicationError"; }

auto ErrorCategory::equivalent(int64_t self, const ILIAS_NAMESPACE::Error& other) const -> bool {
    return other.category().name() == name() && self == static_cast<uint32_t>(other.value());
}

NEKO_END_NAMESPACE
