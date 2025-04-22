
#include <atomic>
#include <condition_variable>
#include <coroutine>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "nekoproto/global/zip.hpp"

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
    virtual void resume()                                              = 0;
    virtual bool isReady() const                                       = 0;
    virtual bool isFinished() const                                    = 0;
    virtual void destroy()                                             = 0;
    virtual int getRefCount() const                                    = 0;
    virtual void increaseRefCount()                                    = 0;
    virtual void decreaseRefCount()                                    = 0;
    virtual void cancel()                                              = 0;
    virtual bool isCancel() const                                      = 0;
    virtual void setNotifyWaiter(std::function<void()> callback) const = 0;
};

class Scheduler {
    TRACER_CONTEXT(Scheduler)
public:
    Scheduler(const Scheduler&)            = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    ~Scheduler() {
        CTXTRACER
        exit();
    }
    void loop() { // NOLINT
        if (mThreadId != std::this_thread::get_id()) {
            printf("warning: event loop thread is not current thread\n");
        }
        CTXTRACER
        mExit = 0;
        while (mExit != 1) {
            std::vector<std::shared_ptr<TaskBase>> tasks;
            // printf("loop... tasks(%zu)\n", tasks_.size());
            do {
                std::unique_lock<std::mutex> lock(mUtex);
                if (mTasks.empty() && mExit == 2) {
                    return;
                }
                mCv.wait(lock, [this] { return !mTasks.empty(); });
                for (auto task : mTasks) {
                    if (task->isReady()) {
                        tasks.push_back(task);
                    }
                }
                for (auto task : tasks) {
                    mTasks.erase(std::remove(mTasks.begin(), mTasks.end(), task), mTasks.end());
                }
            } while (false);

            for (auto task : tasks) {
                task->resume();
            }

            for (auto task : tasks) {
                if (task->isFinished()) {
                    task->decreaseRefCount();
                }
            }
        }

        std::unique_lock<std::mutex> lock(mUtex);
        for (auto task : mTasks) {
            task->decreaseRefCount();
        }
        mExit = 0;
    }

    static Scheduler* current() {
        TRACER
        thread_local Scheduler* kScheduler = nullptr;
        if (kScheduler == nullptr) {
            kScheduler = getScheduler(std::this_thread::get_id());
        }
        if (kScheduler == nullptr) {
            kScheduler = new Scheduler();
        }
        return kScheduler;
    }

    void exit() {
        CTXTRACER
        mExit = 1;
        if (mThreadId != std::this_thread::get_id()) {
            return;
        }
        mCv.notify_all();
    }

    void exitAfterFinish() {
        CTXTRACER
        mExit = 2;
        if (mThreadId != std::this_thread::get_id()) {
            return;
        }
        mCv.notify_all();
    }

    template <typename T>
    std::shared_ptr<TaskBase> addTask(T task) {
        CTXTRACER
        std::unique_lock<std::mutex> lock(mUtex);
        task.increaseRefCount();
        mTasks.push_back(std::shared_ptr<TaskBase>(new T(task)));
        lock.unlock();
        mCv.notify_one();
        return mTasks.back();
    }
    template <typename T>
    std::shared_ptr<TaskBase> operator<<(const Task<T>& task) {
        CTXTRACER
        return addTask(task);
    }

    template <typename T>
    T operator>>(const Task<T>& task) {
        CTXTRACER
        T value;
        task.setNotifyWaiter([this, &value, &promise = task.promise()]() {
            exit();
            value = promise.value();
        });
        addTask(task);
        loop();
        return value;
    }

    template <typename T>
    void removeTask(const Task<T>& task) {
        CTXTRACER
        std::unique_lock<std::mutex> lock(mUtex);
        mTasks.erase(std::remove(mTasks.begin(), mTasks.end(), task), mTasks.end());
        lock.unlock();
    }
    static Scheduler* getScheduler(const std::thread::id& id) {
        TRACER
        for (auto* scheduler : gSchedulers) {
            if (scheduler->mThreadId == id) {
                return scheduler;
            }
        }
        return nullptr;
    }

private:
    Scheduler() {
        CTXTRACER
        mThreadId = std::this_thread::get_id();
        gSchedulers.push_back(this);
    }

    std::mutex mUtex;
    std::condition_variable mCv;
    std::vector<std::shared_ptr<TaskBase>> mTasks;
    std::atomic_int mExit{0};
    std::thread::id mThreadId;

    static std::vector<Scheduler*> gSchedulers;
};

std::vector<Scheduler*> Scheduler::gSchedulers;

struct PromiseBase {
    Scheduler* scheduler = Scheduler::current();
    std::atomic_int refCounter{0};
    std::string name;
    std::function<void()> notifyWaiter = nullptr;
    std::atomic_bool cancelled{false};

    void increaseRefCount() { refCounter++; }
    void decreaseRefCount() { refCounter--; }
    int getRefCount() { return refCounter.load(); }
    void setName(const std::string& name_) { name = name_; }
    const std::string& getName() const { return name; }
    void setNotifyWaiter(std::function<void()> notifyWaiter_) { notifyWaiter = notifyWaiter_; }
    void cancel() { cancelled = true; }
    bool isCancel() const { return cancelled.load(); }
};

template <typename T>
struct Promise : public PromiseBase {
    TRACER_CONTEXT(Promise)
public:
    Task<T> get_return_object() { // NOLINT
        CTXTRACER
        return Task<T>{Task<T>::handle_type::from_promise(*this), "unknow"};
    }
    std::suspend_always initial_suspend() noexcept { // NOLINT(readability-identifier-naming)
        CTXTRACER
        mValue = T{};
        return {};
    }
    std::suspend_always final_suspend() noexcept { // NOLINT(readability-identifier-naming)
        CTXTRACER
        return {};
    }

    void unhandled_exception() { CTXTRACER } // NOLINT(readability-identifier-naming)
    void return_value(T&& value) {           // NOLINT
        CTXTRACER
        mValue = std::forward<T>(value);
        if (notifyWaiter) {
            notifyWaiter();
        }
    }
    void return_value(const T& value) { // NOLINT(readability-identifier-naming)
        CTXTRACER
        mValue = std::forward<T>(value);
        if (notifyWaiter) {
            notifyWaiter();
        }
    }
    const T& value() const {
        CTXTRACER
        return mValue;
    }
    T& value() {
        CTXTRACER
        return mValue;
    }
    template <typename U>
    Awaitable<U, Task<T>> await_transform(U&& task) { // NOLINT(readability-identifier-naming)
        return transform(task, Task<T>{Task<T>::handle_type::from_promise(*this)});
    }

private:
    T mValue;
};

template <typename T>
struct Task : public TaskBase {
    TRACER_CONTEXT(Task<T>)
    using promise_type = Promise<T>;
    using handle_type  = std::coroutine_handle<promise_type>;

    Task(const handle_type& handle)
        : handle(handle){CTXTRACER} Task(const handle_type& handle, std::string name) : handle(handle) {
        CTXTRACER
        promise().setName(name);
    }
    void resume() override {
        CTXTRACER
        if (handle && !promise().isCancel()) {
            handle.resume();
        }
    }
    bool isReady() const override {
        CTXTRACER
        return true;
    }
    bool isFinished() const override {
        CTXTRACER
        return handle.done() || promise().isCancel();
    }
    void destroy() override {
        CTXTRACER
        if (handle) {
            handle.destroy();
            handle = nullptr;
        }
    }
    T value() const {
        if (!handle || promise().isCancel()) {
            return T();
        }
        return handle.promise().value();
    }
    promise_type& promise() const { return handle.promise(); }

    int getRefCount() const override {
        CTXTRACER
        if (!handle) {
            return 0;
        }
        return handle.promise().getRefCount();
    }

    void increaseRefCount() override {
        CTXTRACER
        if (!handle) {
            return;
        }
        handle.promise().increaseRefCount();
    }

    void decreaseRefCount() override {
        CTXTRACER
        if (!handle) {
            return;
        }
        handle.promise().decreaseRefCount();
        if (getRefCount() == 0) {
            destroy();
        }
    }

    void setNotifyWaiter(std::function<void()> callback) const override {
        CTXTRACER
        promise().setNotifyWaiter(callback);
    }

    void cancel() override {
        CTXTRACER
        promise().cancel();
    }

    bool isCancel() const override {
        CTXTRACER
        return promise().isCancel();
    }

    handle_type handle;
};

template <>
struct Promise<void> : public PromiseBase {
    TRACER_CONTEXT(Promise<void>)
    Task<void> get_return_object() { // NOLINT
        CTXTRACER
        return Task<void>{Task<void>::handle_type::from_promise(*this)};
    }
    static std::suspend_always initial_suspend() noexcept { // NOLINT(readability-identifier-naming)
        CTXTRACER
        return {};
    }
    static std::suspend_always final_suspend() noexcept { // NOLINT(readability-identifier-naming)
        CTXTRACER
        return {};
    }
    void unhandled_exception() { CTXTRACER } // NOLINT(readability-identifier-naming)
    void return_void() {                     // NOLINT(readability-identifier-naming)
        if (notifyWaiter) {
            notifyWaiter();
        }
        CTXTRACER
    }
    template <typename U>
    Awaitable<U, Task<void>> await_transform(U&& task) { // NOLINT(readability-identifier-naming)
        return transform(std::forward<U>(task), Task<void>{Task<void>::handle_type::from_promise(*this)});
    }
};

template <typename T>
struct Awaitable<int, Task<T>> {
    TRACER_CONTEXT(Awaitable_int)

    Awaitable(int&& value, const Task<T>& waiter)
        : mValue(value), mWaiter(waiter){CTXTRACER} Awaitable(const int& value) : mValue(value) {
        CTXTRACER
    }
    bool await_ready() { // NOLINT(readability-identifier-naming)
        CTXTRACER
        return true;
    }
    void await_suspend(std::coroutine_handle<> /*unused*/) { CTXTRACER } // NOLINT(readability-identifier-naming)
    int await_resume() {                                                 // NOLINT(readability-identifier-naming)
        CTXTRACER
        return mValue;
    }

private:
    int mValue;
    Task<T> mWaiter;
};

template <typename T, typename U>
struct Awaitable<Task<T>, Task<U>> {
    TRACER_CONTEXT(Awaitable_Task)

    Awaitable(Task<T>&& task, const Task<U>& waiter)
        : task(task),
          waiter(waiter){CTXTRACER} Awaitable(const Task<T>& task, const Task<U>& waiter) : task(task), waiter(waiter) {
        CTXTRACER
    }
    bool await_ready() { // NOLINT(readability-identifier-naming)
        CTXTRACER
        return static_cast<bool>(waiter.isCancel());
    }
    void await_suspend(std::coroutine_handle<> /*unused*/) { // NOLINT(readability-identifier-naming)
        CTXTRACER
        waiter.increaseRefCount();
        task.setNotifyWaiter([this]() {
            waiter.promise().scheduler->addTask(waiter);
            waiter.decreaseRefCount();
        });
        waiter.promise().scheduler->addTask(task);
    }
    T await_resume() { // NOLINT(readability-identifier-naming)
        CTXTRACER
        if (waiter.isCancel()) {
            return T();
        }
        return task.value();
    }
    Task<T> task;
    Task<U> waiter;
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
Task<std::string> to_string(T str) {
    TRACER
    co_return std::to_string(str);
}

template <typename T>
Task<T> add(T num1, T num2) {
    TRACER
    co_return num1 + num2;
}

Task<void> test() { // NOLINT
    TRACER
    auto num1 = 23;
    LOG("num1 = %d", num1);
    auto num2 = co_await add(num1, 42);
    LOG("num2 = %d", num2);
    auto num3 = 24;
    LOG("num3 = %d", num3);
    auto num4 = co_await add(num3, num2);
    LOG("num4 = %d", num4);
    co_return;
}

Task<void> exit(Scheduler* scheduler) { // NOLINT
    TRACER
    LOG("exit");
    scheduler->exitAfterFinish();
    co_return;
}

#define go    *Scheduler::current() <<
#define await *Scheduler::current() >>

NEKO_USE_NAMESPACE

int main() {
    std::cout << "NEKO_CPP_PLUS: " << NEKO_CPP_PLUS << std::endl;
    auto* scheduler = Scheduler::current();

    auto testHandle = go test();
    auto exitHandle = go exit(scheduler);
    exitHandle->setNotifyWaiter([testHandle]() { LOG("notify_waiter"); });
    scheduler->loop();

    int addResult = await add(23, 42);
    std::cout << "a = " << addResult << std::endl;

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

    std::cout << "end" << std::endl;

    return 0;
}