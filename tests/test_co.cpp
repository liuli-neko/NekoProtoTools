#include <time.h>

#include <atomic>
#include <condition_variable>
#include <coroutine>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>
#include <functional>
#include <string>

#include "../core/private/zip.hpp"

#ifdef TRACER_ENABLE
#define TRACER printf("%s:%d [%s]\n", __FILE__, __LINE__, __FUNCTION__);
#define TRACER_CONTEXT(ctx)                                                                                            \
private:                                                                                                               \
    constexpr static const char* tracer_ctx_name_ = #ctx;                                                              \
                                                                                                                       \
public:
#define CTXTRACER printf("%s -- %s:%d [%s]\n", tracer_ctx_name_, __FILE__, __LINE__, __FUNCTION__);
#else
#define TRACER
#define TRACER_CONTEXT(ctx)
#define CTXTRACER
#endif

#ifndef NO_LOG
#define LOG(fmt, ...) printf("[%s:%d][%s] " fmt "\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__);
#else
#define LOG(fmt, ...)
#endif

#define ASSERT(cond, fmt, ...)                                                                                         \
    if (!(cond)) {                                                                                                     \
        printf(fmt "\n", ##__VA_ARGS__);                                                                               \
        ::exit(-1);                                                                                                    \
    }

struct TaskBase;
template <typename T>
struct Task;
template <typename T>
struct Promise;
template <typename T, typename U>
struct Awaitable;

struct TaskBase {
    TRACER_CONTEXT(TaskBase)
public:
    TaskBase() { CTXTRACER }
    ~TaskBase() { CTXTRACER }
    virtual void resume() = 0;
    virtual bool is_ready() const = 0;
    virtual bool is_finished() const = 0;
    virtual void destroy() = 0;
    virtual int get_ref_count() const = 0;
    virtual void increase_ref_count() = 0;
    virtual void decrease_ref_count() = 0;
    virtual void cancel() = 0;
    virtual bool is_cancel() const = 0;
    virtual void set_notify_waiter(std::function<void()> callback) const = 0;
};

class Scheduler {
    TRACER_CONTEXT(Scheduler)
public:
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    ~Scheduler() {
        CTXTRACER
        exit();
    }
    void loop() {
        if (thread_id != std::this_thread::get_id()) {
            printf("warning: event loop thread is not current thread\n");
        }
        CTXTRACER
        exit_ = 0;
        while (exit_ != 1) {
            std::vector<std::shared_ptr<TaskBase>> tasks;
            // printf("loop... tasks(%zu)\n", tasks_.size());
            do {
                std::unique_lock<std::mutex> lock(mutex_);
                if (tasks_.empty() && exit_ == 2) {
                    return;
                }
                cv_.wait(lock, [this] { return !tasks_.empty(); });
                for (auto task : tasks_) {
                    if (task->is_ready()) {
                        tasks.push_back(task);
                    }
                }
                for (auto task : tasks) {
                    tasks_.erase(std::remove(tasks_.begin(), tasks_.end(), task), tasks_.end());
                }
            } while (false);

            for (auto task : tasks) {
                task->resume();
            }

            for (auto task : tasks) {
                if (task->is_finished()) {
                    task->decrease_ref_count();
                }
            }
        }

        std::unique_lock<std::mutex> lock(mutex_);
        for (auto task : tasks_) {
            task->decrease_ref_count();
        }

        exit_ = 0;
    }

    static Scheduler* current() {
        TRACER
        thread_local Scheduler* scheduler = nullptr;
        if (scheduler == nullptr) {
            scheduler = get_scheduler(std::this_thread::get_id());
        }
        if (scheduler == nullptr) {
            scheduler = new Scheduler();
        }
        return scheduler;
    }

    void exit() {
        CTXTRACER
        exit_ = 1;
        if (thread_id != std::this_thread::get_id()) {
            return;
        }
        cv_.notify_all();
    }

    void exit_after_finish() {
        CTXTRACER
        exit_ = 2;
        if (thread_id != std::this_thread::get_id()) {
            return;
        }
        cv_.notify_all();
    }

    template <typename T>
    std::shared_ptr<TaskBase> add_task(T task) {
        CTXTRACER
        std::unique_lock<std::mutex> lock(mutex_);
        task.increase_ref_count();
        tasks_.push_back(std::shared_ptr<TaskBase>(new T(task)));
        lock.unlock();
        cv_.notify_one();
        return tasks_.back();
    }
    template <typename T>
    std::shared_ptr<TaskBase> operator<<(const Task<T>& task) {
        CTXTRACER
        return add_task(task);
    }

    template <typename T>
    T operator>>(const Task<T>& task) {
        CTXTRACER
        T value;
        task.set_notify_waiter([this, &value, &promise = task.promise()]() {
            exit();
            value = promise.value();
        });
        add_task(task);
        loop();
        return value;
    }

    template <typename T>
    void remove_task(const Task<T>& task) {
        CTXTRACER
        std::unique_lock<std::mutex> lock(mutex_);
        tasks_.erase(std::remove(tasks_.begin(), tasks_.end(), task), tasks_.end());
        lock.unlock();
    }
    static Scheduler* get_scheduler(const std::thread::id& id) {
        TRACER
        for (auto scheduler : schedulers_) {
            if (scheduler->thread_id == id) {
                return scheduler;
            }
        }
        return nullptr;
    }

private:
    Scheduler() {
        CTXTRACER
        thread_id = std::this_thread::get_id();
        schedulers_.push_back(this);
    }

    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::shared_ptr<TaskBase>> tasks_;
    std::atomic_int exit_{0};
    std::thread::id thread_id;

    static std::vector<Scheduler*> schedulers_;
};

std::vector<Scheduler*> Scheduler::schedulers_;

struct PromiseBase {
    Scheduler* scheduler_ = Scheduler::current();
    std::atomic_int ref_counter_{0};
    std::string name_;
    std::function<void()> notify_waiter_ = nullptr;
    std::atomic_bool cancel_{0};

    void increase_ref_count() { ref_counter_++; }
    void decrease_ref_count() { ref_counter_--; }
    int get_ref_count() { return ref_counter_.load(); }
    void set_name(const std::string& name) { name_ = name; }
    const std::string& get_name() { return name_; }
    void set_notify_waiter(std::function<void()> notify_waiter) { notify_waiter_ = notify_waiter; }
    void cancel() { cancel_ = 1; }
    bool is_cancel() { return cancel_.load(); }
};

template <typename T>
struct Promise : public PromiseBase {
    TRACER_CONTEXT(Promise)
public:
    Task<T> get_return_object() {
        CTXTRACER
        return Task<T>{Task<T>::handle_type::from_promise(*this), "unknow"};
    }
    std::suspend_always initial_suspend() noexcept {
        CTXTRACER
        value_ = T{};
        return {};
    }
    std::suspend_always final_suspend() noexcept {
        CTXTRACER
        return {};
    }

    void unhandled_exception() { CTXTRACER }
    void return_value(T&& value) {
        CTXTRACER
        value_ = std::forward<T>(value);
        if (notify_waiter_) {
            notify_waiter_();
        }
    }
    void return_value(const T& value) {
        CTXTRACER
        value_ = std::forward<T>(value);
        if (notify_waiter_) {
            notify_waiter_();
        }
    }
    const T& value() const {
        CTXTRACER
        return value_;
    }
    T& value() {
        CTXTRACER
        return value_;
    }
    template <typename U>
    Awaitable<U, Task<T>> await_transform(U&& task) {
        return transform(task, Task<T>{Task<T>::handle_type::from_promise(*this)});
    }

private:
    T value_;
};

template <typename T>
struct Task : public TaskBase {
    TRACER_CONTEXT(Task<T>)
    using promise_type = Promise<T>;
    using handle_type = std::coroutine_handle<promise_type>;

    Task(const handle_type& h) : handle_(h){CTXTRACER} Task(const handle_type& h, std::string name) : handle_(h) {
        CTXTRACER
        promise().set_name(name);
    }
    void resume() override {
        CTXTRACER
        if (handle_ && !promise().is_cancel()) {
            handle_.resume();
        }
    }
    bool is_ready() const override {
        CTXTRACER
        return true;
    }
    bool is_finished() const override {
        CTXTRACER
        return handle_.done() || promise().is_cancel();
    }
    void destroy() override {
        CTXTRACER
        if (handle_) {
            handle_.destroy();
            handle_ = nullptr;
        }
    }
    T value() const {
        if (!handle_ || promise().is_cancel()) {
            return T();
        }
        return handle_.promise().value();
    }
    promise_type& promise() const { return handle_.promise(); }

    int get_ref_count() const override {
        CTXTRACER
        if (!handle_) {
            return 0;
        }
        return handle_.promise().get_ref_count();
    }

    void increase_ref_count() override {
        CTXTRACER
        if (!handle_) {
            return;
        }
        handle_.promise().increase_ref_count();
    }

    void decrease_ref_count() override {
        CTXTRACER
        if (!handle_) {
            return;
        }
        handle_.promise().decrease_ref_count();
        if (get_ref_count() == 0) {
            destroy();
        }
    }

    void set_notify_waiter(std::function<void()> callback) const override {
        CTXTRACER
        promise().set_notify_waiter(callback);
    }

    void cancel() {
        CTXTRACER
        promise().cancel();
    }

    bool is_cancel() const override {
        CTXTRACER
        return promise().is_cancel();
    }

    handle_type handle_;
};

template <>
struct Promise<void> : public PromiseBase {
    TRACER_CONTEXT(Promise<void>)
    Task<void> get_return_object() {
        CTXTRACER
        return Task<void>{Task<void>::handle_type::from_promise(*this)};
    }
    std::suspend_always initial_suspend() noexcept {
        CTXTRACER
        return {};
    }
    std::suspend_always final_suspend() noexcept {
        CTXTRACER
        return {};
    }
    void unhandled_exception() { CTXTRACER }
    void return_void() {
        if (notify_waiter_) {
            notify_waiter_();
        }
        CTXTRACER
    }
    template <typename U>
    Awaitable<U, Task<void>> await_transform(U&& task) {
        return transform(std::forward<U>(task), Task<void>{Task<void>::handle_type::from_promise(*this)});
    }
};

template <typename T>
struct Awaitable<int, Task<T>> {
    TRACER_CONTEXT(Awaitable_int)

    Awaitable(int&& value, const Task<T>& waiter)
        : value_(value), waiter_(waiter){CTXTRACER} Awaitable(const int& value) : value_(value) {
        CTXTRACER
    }
    bool await_ready() {
        CTXTRACER
        return true;
    }
    void await_suspend(std::coroutine_handle<>) { CTXTRACER }
    int await_resume() {
        CTXTRACER
        return value_;
    }

private:
    int value_;
    Task<T> waiter_;
};

template <typename T, typename U>
struct Awaitable<Task<T>, Task<U>> {
    TRACER_CONTEXT(Awaitable_Task)

    Awaitable(Task<T>&& task, const Task<U>& waiter)
        : task_(task), waiter_(waiter){CTXTRACER} Awaitable(const Task<T>& task, const Task<U>& waiter)
        : task_(task), waiter_(waiter) {
        CTXTRACER
    }
    bool await_ready() {
        CTXTRACER
        if (waiter_.is_cancel()) {
            return true;
        }
        return false;
    }
    void await_suspend(std::coroutine_handle<> handle) {
        CTXTRACER
        waiter_.increase_ref_count();
        task_.set_notify_waiter([this]() {
            waiter_.promise().scheduler_->add_task(waiter_);
            waiter_.decrease_ref_count();
        });
        waiter_.promise().scheduler_->add_task(task_);
    }
    T await_resume() {
        CTXTRACER
        if (waiter_.is_cancel()) {
            return T();
        }
        return task_.value();
    }
    Task<T> task_;
    Task<U> waiter_;
};

template <typename T>
Awaitable<int, Task<T>> transform(int&& value, const Task<T>& waiter) {
    TRACER
    return Awaitable<int, Task<T>>(std::forward<int>(value), waiter);
}

template <typename T, typename U>
Awaitable<Task<T>, Task<U>> transform(const Task<T>& task, const Task<U>& waiter) {
    TRACER
    return Awaitable<Task<T>, Task<U>>(task, waiter);
}

template <typename T>
Task<std::string> toString(T a) {
    TRACER
    co_return std::to_string(a);
}

template <typename T>
Task<T> add(T a, T b) {
    TRACER
    co_return a + b;
}

Task<void> test() {
    TRACER
    auto a = 23;
    LOG("a = %d", a);
    auto b = co_await add(a, 42);
    LOG("b = %d", b);
    auto c = 24;
    LOG("c = %d", c);
    auto d = co_await add(b, c);
    LOG("d = %d", d);
    co_return;
}

Task<void> exit(Scheduler* scheduler) {
    TRACER
    LOG("exit");
    scheduler->exit_after_finish();
    co_return;
}

#define go    *Scheduler::current() <<
#define await *Scheduler::current() >>

NEKO_USE_NAMESPACE

int main() {
    std::cout << "NEKO_CPP_PLUS: " << NEKO_CPP_PLUS << std::endl;
    auto scheduler = Scheduler::current();

    auto t = go test();
    auto e = go exit(scheduler);
    e->set_notify_waiter([t]() { LOG("notify_waiter"); });
    scheduler->loop();

    int a = await add(23, 42);
    std::cout << "a = " << a << std::endl;

    std::vector<int> va{1, 2, 12, 3, 4, 5, 6, 7};
    std::list<double> vb{12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0, 19.0, 20.0};
    std::set<std::string> vc{"123", "1234", "12345", "123456", "1234567", "12345678", "123456789", "1234567890"};
    std::map<std::string, int> vd{
        {"1", 1}, {"12", 2}, {"123", 3}, {"1234", 4}, {"12345", 5}, {"123456", 6}, {"1234567", 7}, {"12345678", 8},
    };
    for (auto [a, b, c, d] : Zip(va, vb, vc, vd)) {
        std::cout << "a = " << a << ", b = " << b << ", c = " << c << " key:" << d.first << " value: " << d.second
                  << std::endl;
    }
    for (auto [a, b, c, d] : ReverseZip(va, vb, vc, vd)) {
        std::cout << "a = " << a << ", b = " << b << ", c = " << c << " key:" << d.first << " value: " << d.second
                  << std::endl;
    }
    //  auto zipd = Zip(va, vb, vc, vd);
    //  for (auto items = zipd.begin(); items != zipd.end(); ++items) {
    //    auto [a ,b ,c ,d] = *items;
    //    std::cout << "a = " << a << ", b = " << b << ", c = " << c << " key:" << d.first << " value: " << d.second
    //                  << std::endl;
    //  }

    std::cout << "end" << std::endl;

    return 0;
}