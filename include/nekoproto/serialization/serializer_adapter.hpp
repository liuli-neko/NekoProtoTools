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
    explicit OutputSerializerAdapter(Args&&... args) : state_(std::forward<Args>(args)...) {}

    OutputSerializerAdapter(const OutputSerializerAdapter&)                    = delete;
    OutputSerializerAdapter(OutputSerializerAdapter&&)                         = delete;
    auto operator=(const OutputSerializerAdapter&) -> OutputSerializerAdapter& = delete;
    auto operator=(OutputSerializerAdapter&&) -> OutputSerializerAdapter&      = delete;

    ~OutputSerializerAdapter() { end(); }

    template <typename T>
    auto operator()(const T& value) -> bool {
        if (document_attempted_) {
            last_result_ = sa::error(sa::ErrorCode::InvalidLength,
                                    "A document serializer cannot write a second root value");
            return false;
        }
        document_attempted_ = true;
        last_result_        = sa::success();
        last_result_        = Backend::write(state_, value);
        return static_cast<bool>(last_result_);
    }

    auto end() -> bool {
        if constexpr (requires(StateType& state, sa::Result<void> result) {
                          { Backend::finish(state, result) } -> std::same_as<sa::Result<void>>;
                      }) {
            last_result_ = Backend::finish(state_, last_result_);
        } else if constexpr (requires(StateType& state) {
                                 { Backend::finish(state) } -> std::same_as<sa::Result<void>>;
                             }) {
            if (last_result_) {
                last_result_ = Backend::finish(state_);
            }
        }
        return static_cast<bool>(last_result_);
    }

    explicit operator bool() const noexcept {
        if constexpr (requires(const StateType& state, const sa::Result<void>& result) {
                          { Backend::outputReady(state, result) } -> std::convertible_to<bool>;
                      }) {
            return Backend::outputReady(state_, last_result_);
        } else {
            return static_cast<bool>(last_result_);
        }
    }
    auto error() const noexcept -> const sa::Error* { return sa::errorPtr(last_result_); }

    auto state() noexcept -> StateType& { return state_; }
    auto state() const noexcept -> const StateType& { return state_; }

private:
    StateType        state_;
    sa::Result<void> last_result_;
    bool             document_attempted_ = false;
};

template <typename Backend, typename SourceT = typename Backend::DefaultInputSource>
class InputSerializerAdapter {
public:
    using BackendType = Backend;
    using StateType   = typename Backend::template InputState<SourceT>;

    template <typename... Args>
    explicit InputSerializerAdapter(Args&&... args) : state_(std::forward<Args>(args)...) {
        init_result_ = initialResult();
        last_result_ = init_result_;
    }

    InputSerializerAdapter(const InputSerializerAdapter&)                    = delete;
    InputSerializerAdapter(InputSerializerAdapter&&)                         = delete;
    auto operator=(const InputSerializerAdapter&) -> InputSerializerAdapter& = delete;
    auto operator=(InputSerializerAdapter&&) -> InputSerializerAdapter&      = delete;

    template <typename T>
    auto operator()(T& value) -> bool {
        if (!init_result_) {
            last_result_ = init_result_;
            return false;
        }
        if (document_attempted_) {
            last_result_ = sa::error(sa::ErrorCode::InvalidLength,
                                    "A document deserializer cannot read the root value twice");
            return false;
        }
        document_attempted_ = true;
        last_result_        = sa::success();
        last_result_        = Backend::read(state_, value);
        if constexpr (requires(StateType& state, sa::Result<void> result) {
                          { Backend::finish(state, result) } -> std::same_as<sa::Result<void>>;
                      }) {
            last_result_ = Backend::finish(state_, last_result_);
        } else if constexpr (requires(StateType& state) {
                                 { Backend::finish(state) } -> std::same_as<sa::Result<void>>;
                             }) {
            if (last_result_) {
                last_result_ = Backend::finish(state_);
            }
        }
        return static_cast<bool>(last_result_);
    }

    explicit operator bool() const noexcept { return static_cast<bool>(last_result_); }
    auto error() const noexcept -> const sa::Error* { return sa::errorPtr(last_result_); }

    auto offset() const noexcept -> std::size_t
        requires requires(const StateType& state) {
            { Backend::offset(state) } -> std::convertible_to<std::size_t>;
        }
    {
        return Backend::offset(state_);
    }

    auto state() noexcept -> StateType& { return state_; }
    auto state() const noexcept -> const StateType& { return state_; }

private:
    auto initialResult() const -> sa::Result<void> {
        if constexpr (requires(const StateType& state) {
                          { Backend::inputResult(state) } -> std::same_as<sa::Result<void>>;
                      }) {
            return Backend::inputResult(state_);
        } else {
            return sa::success();
        }
    }

private:
    StateType        state_;
    sa::Result<void> init_result_;
    sa::Result<void> last_result_;
    bool             document_attempted_ = false;
};

namespace detail {
template <typename Backend, typename BufferT = typename Backend::DefaultOutputBuffer>
using OutputSerializerAdapter = nekoproto::OutputSerializerAdapter<Backend, BufferT>;

template <typename Backend, typename SourceT = typename Backend::DefaultInputSource>
using InputSerializerAdapter = nekoproto::InputSerializerAdapter<Backend, SourceT>;
} // namespace detail

} // namespace nekoproto
