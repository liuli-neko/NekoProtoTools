#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/serialization/error.hpp"

#include <concepts>
#include <cstddef>
#include <utility>

NEKO_BEGIN_NAMESPACE
namespace detail {

template <typename Backend, typename BufferT>
class OutputSerializerAdapter {
public:
    using BackendType = Backend;
    using StateType   = typename Backend::template OutputState<BufferT>;

    template <typename... Args>
    explicit OutputSerializerAdapter(Args&&... args) : mState(std::forward<Args>(args)...) {}

    OutputSerializerAdapter(const OutputSerializerAdapter&)            = delete;
    OutputSerializerAdapter(OutputSerializerAdapter&&)                 = delete;
    OutputSerializerAdapter& operator=(const OutputSerializerAdapter&) = delete;
    OutputSerializerAdapter& operator=(OutputSerializerAdapter&&)      = delete;

    ~OutputSerializerAdapter() { end(); }

    template <typename T>
    bool operator()(const T& value) {
        if (mDocumentAttempted) {
            mLastResult = sa::error(sa::ErrorCode::InvalidLength,
                                    "A document serializer cannot write a second root value");
            return false;
        }
        mDocumentAttempted = true;
        mLastResult = sa::success();
        mLastResult = Backend::write(mState, value);
        return static_cast<bool>(mLastResult);
    }

    bool end() {
        if constexpr (requires(StateType& state, sa::Result<void> result) {
                          { Backend::finish(state, result) } -> std::same_as<sa::Result<void>>;
                      }) {
            mLastResult = Backend::finish(mState, mLastResult);
        } else if constexpr (requires(StateType& state) {
                                 { Backend::finish(state) } -> std::same_as<sa::Result<void>>;
                             }) {
            if (mLastResult) {
                mLastResult = Backend::finish(mState);
            }
        }
        return static_cast<bool>(mLastResult);
    }

    explicit operator bool() const noexcept {
        if constexpr (requires(const StateType& state, const sa::Result<void>& result) {
                          { Backend::outputReady(state, result) } -> std::convertible_to<bool>;
                      }) {
            return Backend::outputReady(mState, mLastResult);
        } else {
            return static_cast<bool>(mLastResult);
        }
    }
    const sa::Error* error() const noexcept { return sa::error_ptr(mLastResult); }

    StateType& state() noexcept { return mState; }
    const StateType& state() const noexcept { return mState; }

private:
    StateType mState;
    sa::Result<void> mLastResult;
    bool mDocumentAttempted = false;
};

template <typename Backend, typename SourceT>
class InputSerializerAdapter {
public:
    using BackendType = Backend;
    using StateType   = typename Backend::template InputState<SourceT>;

    template <typename... Args>
    explicit InputSerializerAdapter(Args&&... args) : mState(std::forward<Args>(args)...) {
        mInitResult = _initialResult();
        mLastResult = mInitResult;
    }

    InputSerializerAdapter(const InputSerializerAdapter&)            = delete;
    InputSerializerAdapter(InputSerializerAdapter&&)                 = delete;
    InputSerializerAdapter& operator=(const InputSerializerAdapter&) = delete;
    InputSerializerAdapter& operator=(InputSerializerAdapter&&)      = delete;

    template <typename T>
    bool operator()(T& value) {
        if (!mInitResult) {
            mLastResult = mInitResult;
            return false;
        }
        if (mDocumentAttempted) {
            mLastResult = sa::error(sa::ErrorCode::InvalidLength,
                                    "A document deserializer cannot read the root value twice");
            return false;
        }
        mDocumentAttempted = true;
        mLastResult = sa::success();
        mLastResult = Backend::read(mState, value);
        if constexpr (requires(StateType& state, sa::Result<void> result) {
                          { Backend::finish(state, result) } -> std::same_as<sa::Result<void>>;
                      }) {
            mLastResult = Backend::finish(mState, mLastResult);
        } else if constexpr (requires(StateType& state) {
                                 { Backend::finish(state) } -> std::same_as<sa::Result<void>>;
                             }) {
            if (mLastResult) {
                mLastResult = Backend::finish(mState);
            }
        }
        return static_cast<bool>(mLastResult);
    }

    explicit operator bool() const noexcept { return static_cast<bool>(mLastResult); }
    const sa::Error* error() const noexcept { return sa::error_ptr(mLastResult); }

    std::size_t offset() const noexcept
        requires requires(const StateType& state) {
            { Backend::offset(state) } -> std::convertible_to<std::size_t>;
        }
    {
        return Backend::offset(mState);
    }

    StateType& state() noexcept { return mState; }
    const StateType& state() const noexcept { return mState; }

private:
    sa::Result<void> _initialResult() const {
        if constexpr (requires(const StateType& state) {
                          { Backend::inputResult(state) } -> std::same_as<sa::Result<void>>;
                      }) {
            return Backend::inputResult(mState);
        } else {
            return sa::success();
        }
    }

private:
    StateType mState;
    sa::Result<void> mInitResult;
    sa::Result<void> mLastResult;
    bool mDocumentAttempted = false;
};

} // namespace detail
NEKO_END_NAMESPACE
