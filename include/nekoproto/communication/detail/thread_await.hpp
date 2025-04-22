/**
 * @file thread_await.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-10-02
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include <chrono>
#include <coroutine>
#include <future>
#include <ilias/ilias.hpp>
#include <ilias/task.hpp>

#include "nekoproto/global/global.hpp"
NEKO_BEGIN_NAMESPACE
namespace detail {

template <typename ReturnT>
struct ThreadAwaiter {
    ThreadAwaiter(std::function<ReturnT()> func) : mCallback(std::move(func)) {}
    auto await_ready() const noexcept { return false; }
    auto await_suspend(ILIAS_NAMESPACE::TaskView<> caller) -> bool {
        mCaller     = caller;
        auto future = std::async(
            std::launch::async,
            [](ThreadAwaiter<ReturnT>* self) {
                self->mResult = self->mCallback();
                self->mCaller.schedule();
            },
            this);
        return true;
    }
    auto await_resume() -> ILIAS_NAMESPACE::Result<ReturnT> { return std::move(mResult); }
    ILIAS_NAMESPACE::TaskView<> mCaller;
    std::function<ReturnT()> mCallback;
    ReturnT mResult;
};
} // namespace detail
NEKO_END_NAMESPACE