#pragma once

#include "nekoproto/global/global.hpp"
#include "nekoproto/serialization/error.hpp"

#include <concepts>
#include <cstddef>
#include <utility>

namespace nekoproto {

template <typename Backend, typename BufferT = typename Backend::DefaultOutputBuffer>
class OutputSerializerAdapter {
public:
    using BackendType = Backend;
    using StateType   = typename Backend::template OutputState<BufferT>;

    template <typename... Args>
    explicit OutputSerializerAdapter(Args&&... args) : mState(std::forward<Args>(args)...) {}

    OutputSerializerAdapter(const OutputSerializerAdapter&)            = delete;
    OutputSerializerAdapter(OutputSerializerAdapter&&)                 = delete;
    auto operator=(const OutputSerializerAdapter&) -> OutputSerializerAdapter& = delete;
    auto operator=(OutputSerializerAdapter&&) -> OutputSerializerAdapter&      = delete;

    ~OutputSerializerAdapter() { end(); }

    template <typename T>
    auto operator()(const T& value) -> bool {
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

    auto end() -> bool {
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
    auto error() const noexcept -> const sa::Error* { return sa::errorPtr(mLastResult); }

    auto state() noexcept -> StateType& { return mState; }
    auto state() const noexcept -> const StateType& { return mState; }

private:
    StateType mState;
    sa::Result<void> mLastResult;
    bool mDocumentAttempted = false;
};

template <typename Backend, typename SourceT = typename Backend::DefaultInputSource>
class InputSerializerAdapter {
public:
    using BackendType = Backend;
    using StateType   = typename Backend::template InputState<SourceT>;

    template <typename... Args>
    explicit InputSerializerAdapter(Args&&... args) : mState(std::forward<Args>(args)...) {
        mInitResult = initialResult();
        mLastResult = mInitResult;
    }

    InputSerializerAdapter(const InputSerializerAdapter&)            = delete;
    InputSerializerAdapter(InputSerializerAdapter&&)                 = delete;
    auto operator=(const InputSerializerAdapter&) -> InputSerializerAdapter& = delete;
    auto operator=(InputSerializerAdapter&&) -> InputSerializerAdapter&      = delete;

    template <typename T>
    auto operator()(T& value) -> bool {
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
    auto error() const noexcept -> const sa::Error* { return sa::errorPtr(mLastResult); }

    auto offset() const noexcept -> std::size_t
        requires requires(const StateType& state) {
            { Backend::offset(state) } -> std::convertible_to<std::size_t>;
        }
    {
        return Backend::offset(mState);
    }

    auto state() noexcept -> StateType& { return mState; }
    auto state() const noexcept -> const StateType& { return mState; }

private:
    auto initialResult() const -> sa::Result<void> {
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

namespace detail {
template <typename Backend, typename BufferT = typename Backend::DefaultOutputBuffer>
using OutputSerializerAdapter = nekoproto::OutputSerializerAdapter<Backend, BufferT>;

template <typename Backend, typename SourceT = typename Backend::DefaultInputSource>
using InputSerializerAdapter = nekoproto::InputSerializerAdapter<Backend, SourceT>;
} // namespace detail

} // namespace nekoproto
