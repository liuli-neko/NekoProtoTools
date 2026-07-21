#include <atomic>
#include <barrier>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// 用户原始版本，仅加入可选 yield 来放大原有竞态窗口。
class BrokenMMQueue {
public:
    explicit BrokenMMQueue(std::size_t size) : mHead(0), mTail(0), mBuffer(size) {}

    bool tryPush(int value) {
        int head      = mHead.load(std::memory_order_acquire);
        int tail      = mTail.load(std::memory_order_acquire);
        int next_tail = (tail + 1) % mBuffer.capacity();
        if (next_tail == head) {
            return false;
        }
        if (mTail.compare_exchange_strong(tail, next_tail, std::memory_order_release)) {
#ifdef WIDEN_BROKEN_WINDOW
            for (int i = 0; i < 32; ++i)
                std::this_thread::yield();
#endif
            mBuffer[static_cast<std::size_t>(tail)] = value;
            return true;
        }
        return false;
    }

    bool tryPop(int& value) {
        int head      = mHead.load(std::memory_order_acquire);
        int tail      = mTail.load(std::memory_order_acquire);
        int next_head = (head + 1) % mBuffer.capacity();
        if (head == tail) {
            return false;
        }
        if (mHead.compare_exchange_strong(head, next_head, std::memory_order_release)) {
#ifdef WIDEN_BROKEN_WINDOW
            for (int i = 0; i < 32; ++i)
                std::this_thread::yield();
#endif
            value = mBuffer[static_cast<std::size_t>(head)];
            return true;
        }
        return false;
    }

private:
    std::atomic<int> mHead;
    std::atomic<int> mTail;
    std::vector<int> mBuffer;
};

// 有界 MPMC 队列：每个槽位有独立 sequence。
// head/tail 仅预约位置，sequence 才发布“可读/可写”。
class BoundedMPMCQueue {
private:
    struct Cell {
        std::atomic<std::size_t> sequence;
        int data{};
    };

public:
    explicit BoundedMPMCQueue(std::size_t capacity)
        : m_capacity(capacity), m_mask(capacity - 1), mBuffer(std::make_unique<Cell[]>(capacity)) {
        if (capacity < 2 || (capacity & (capacity - 1)) != 0) {
            throw std::invalid_argument("capacity 必须是 >= 2 的 2 次幂");
        }
        for (std::size_t i = 0; i < capacity; ++i) {
            mBuffer[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    BoundedMPMCQueue(const BoundedMPMCQueue&)            = delete;
    BoundedMPMCQueue& operator=(const BoundedMPMCQueue&) = delete;

    bool tryPush(int value) {
        Cell* cell      = nullptr;
        std::size_t pos = mEnqueuePos.load(std::memory_order_relaxed);

        for (;;) {
            cell                  = &mBuffer[pos & m_mask];
            const std::size_t seq = cell->sequence.load(std::memory_order_acquire);
            const auto diff       = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos);

            if (diff == 0) {
                if (mEnqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed,
                                                      std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                return false;
            } else {
                pos = mEnqueuePos.load(std::memory_order_relaxed);
            }
        }

        cell->data = value;
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    bool tryPop(int& value) {
        Cell* cell      = nullptr;
        std::size_t pos = mDequeuePos.load(std::memory_order_relaxed);

        for (;;) {
            cell                  = &mBuffer[pos & m_mask];
            const std::size_t seq = cell->sequence.load(std::memory_order_acquire);
            const auto diff       = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos + 1);

            if (diff == 0) {
                if (mDequeuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed,
                                                      std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                return false;
            } else {
                pos = mDequeuePos.load(std::memory_order_relaxed);
            }
        }

        value = cell->data;
        cell->sequence.store(pos + m_capacity, std::memory_order_release);
        return true;
    }

private:
    const std::size_t m_capacity;
    const std::size_t m_mask;
    std::unique_ptr<Cell[]> mBuffer;
    alignas(64) std::atomic<std::size_t> mEnqueuePos{0};
    alignas(64) std::atomic<std::size_t> mDequeuePos{0};
};

struct StressResult {
    std::uint64_t total{};
    std::uint64_t produced{};
    std::uint64_t consumed{};
    std::uint64_t missing{};
    std::uint64_t duplicate{};
    std::uint64_t out_of_range{};
    double seconds{};
};

template <typename Queue>
StressResult run_stress(Queue& queue, int producer_count, int consumer_count, int items_per_producer) {

    const std::uint64_t total =
        static_cast<std::uint64_t>(producer_count) * static_cast<std::uint64_t>(items_per_producer);

    auto seen = std::make_unique<std::atomic<std::uint32_t>[]>(total);
    for (std::uint64_t i = 0; i < total; ++i) {
        seen[i].store(0, std::memory_order_relaxed);
    }

    std::atomic<std::uint64_t> produced{0};
    std::atomic<std::uint64_t> consumed{0};
    std::atomic<std::uint64_t> duplicate{0};
    std::atomic<std::uint64_t> out_of_range{0};

    std::barrier start_line(producer_count + consumer_count + 1);
    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(producer_count + consumer_count));

    for (int producer = 0; producer < producer_count; ++producer) {
        threads.emplace_back([&, producer] {
            start_line.arrive_and_wait();
            for (int i = 0; i < items_per_producer; ++i) {
                const int value = (producer * items_per_producer) + i + 1;
                while (!queue.tryPush(value)) {
                    std::this_thread::yield();
                }
                produced.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (int consumer = 0; consumer < consumer_count; ++consumer) {
        threads.emplace_back([&] {
            start_line.arrive_and_wait();
            for (;;) {
                if (consumed.load(std::memory_order_acquire) >= total) {
                    break;
                }

                int value = 0;
                if (!queue.tryPop(value)) {
                    std::this_thread::yield();
                    continue;
                }

                const std::uint64_t ticket = consumed.fetch_add(1, std::memory_order_acq_rel);
                if (ticket >= total) {
                    out_of_range.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }

                if (value <= 0 || static_cast<std::uint64_t>(value) > total) {
                    out_of_range.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }

                const std::uint64_t index    = static_cast<std::uint64_t>(value - 1);
                const std::uint32_t previous = seen[index].fetch_add(1, std::memory_order_relaxed);
                if (previous != 0) {
                    duplicate.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    const auto begin = std::chrono::steady_clock::now();
    start_line.arrive_and_wait();
    for (auto& thread : threads) {
        thread.join();
    }
    const auto end = std::chrono::steady_clock::now();

    std::uint64_t missing = 0;
    for (std::uint64_t i = 0; i < total; ++i) {
        if (seen[i].load(std::memory_order_relaxed) == 0) {
            ++missing;
        }
    }

    return {
        .total        = total,
        .produced     = produced.load(std::memory_order_relaxed),
        .consumed     = consumed.load(std::memory_order_relaxed),
        .missing      = missing,
        .duplicate    = duplicate.load(std::memory_order_relaxed),
        .out_of_range = out_of_range.load(std::memory_order_relaxed),
        .seconds      = std::chrono::duration<double>(end - begin).count(),
    };
}

void print_result(const std::string& name, const StressResult& r1) {
    std::cout << "\n[" << name << "]\n"
              << "  total        = " << r1.total << '\n'
              << "  produced     = " << r1.produced << '\n'
              << "  consumed     = " << r1.consumed << '\n'
              << "  missing      = " << r1.missing << '\n'
              << "  duplicate    = " << r1.duplicate << '\n'
              << "  out-of-range = " << r1.out_of_range << '\n'
              << "  seconds      = " << r1.seconds << '\n'
              << "  throughput   = " << (r1.seconds > 0.0 ? static_cast<double>(r1.consumed) / r1.seconds : 0.0)
              << " pops/s\n";

    const bool passed = r1.produced == r1.total && r1.consumed == r1.total && r1.missing == 0 && r1.duplicate == 0 &&
                        r1.out_of_range == 0;
    std::cout << "  RESULT       = " << (passed ? "PASS" : "FAIL") << '\n';
}

void run_broken_race_test(int producers, int consumers, int items_per_producer) {
    const std::size_t total = static_cast<std::size_t>(producers) * static_cast<std::size_t>(items_per_producer);

    // 分配 total+1，故意避免越界，让测试只攻击发布顺序竞态。
    BrokenMMQueue queue(total + 1);
    print_result("原始队列：不回绕竞态测试", run_stress(queue, producers, consumers, items_per_producer));
}

void run_broken_wrap_test() {
    BrokenMMQueue queue(64);
    for (int i = 0; i < 100'000; ++i) {
        if (!queue.tryPush(i + 1)) {
            std::cout << "队列在第 " << i << " 次 push 报告满。\n";
            return;
        }
    }
    std::cout << "原始队列接受了 100000 次 push；开启 ASan 时通常会更早报告堆越界。\n";
}

void run_fixed_test(int producers, int consumers, int items_per_producer, std::size_t capacity) {
    BoundedMPMCQueue queue(capacity);
    print_result("修正版有界 MPMC 队列", run_stress(queue, producers, consumers, items_per_producer));
}

int main(int argc, char** argv) {
    const std::string mode     = argc >= 2 ? argv[1] : "fixed";
    const int producers        = argc >= 3 ? std::atoi(argv[2]) : 8;
    const int consumers        = argc >= 4 ? std::atoi(argv[3]) : 8;
    const int items_per_producer = argc >= 5 ? std::atoi(argv[4]) : 500'000;
    const std::size_t capacity = argc >= 6 ? static_cast<std::size_t>(std::strtoull(argv[5], nullptr, 10)) : 1024;

    if (mode == "broken-race") {
        run_broken_race_test(producers, consumers, items_per_producer);
    } else if (mode == "broken-wrap") {
        run_broken_wrap_test();
    } else if (mode == "fixed") {
        run_fixed_test(producers, consumers, items_per_producer, capacity);
    } else {
        std::cerr << "用法：\n"
                  << "  " << argv[0] << " fixed [生产者数] [消费者数] [每生产者元素数] [容量]\n"
                  << "  " << argv[0] << " broken-race [生产者数] [消费者数] [每生产者元素数]\n"
                  << "  " << argv[0] << " broken-wrap\n";
        return 2;
    }
}