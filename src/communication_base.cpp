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

#include <ilias/task.hpp>
#include <system_error>

using namespace ILIAS_NAMESPACE;

NEKO_BEGIN_NAMESPACE

auto ErrorCategory::instance() -> const ErrorCategory& {
    static ErrorCategory kInstance;
    return kInstance;
}

auto ErrorCategory::name() const noexcept -> const char* { return "NekoCommunicationError"; }

auto ErrorCategory::equivalent(int value, const std::error_condition& other) const noexcept -> bool {
    return other.category().name() == name() && value == other.value();
}

NEKO_END_NAMESPACE
