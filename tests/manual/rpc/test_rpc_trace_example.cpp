#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <ilias/io/duplex.hpp>
#include <ilias/net.hpp>
#include <ilias/platform.hpp>
#include <ilias/task.hpp>
#include <ilias/task/scope.hpp>

#include "nekoproto/argparser/argparser.hpp"
#include "nekoproto/jsonrpc/backend.hpp"
#include "nekoproto/jsonrpc/message_stream_wrapper.hpp"
#include "nekoproto/rpc/backend.hpp"
#include "nekoproto/rpc/console.hpp"
#include "nekoproto/rpc/rpc.hpp"
#include "nekoproto/rpc/tracing.hpp"
#include "nekoproto/serialization/binary_serializer.hpp"

using namespace nekoproto;
using namespace nekoproto::argparser;
using namespace std::chrono_literals;

namespace {

std::atomic<bool> gRunning{true};

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        gRunning = false;
        std::cout << "\n[!] Caught shutdown signal (Ctrl+C), stopping workload gracefully...\n" << std::flush;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Command Line Arguments
// ─────────────────────────────────────────────────────────────────────────────

struct TraceExampleOptions {
    std::string webui       = "127.0.0.1:18080";
    std::string backend     = "jsonrpc";
    int concurrency         = 8;
    int rate                = 40;
    int totalTasks          = 0;
    int timeoutMs           = 2000;
    int chaosPct            = 8;
    int cancelPct           = 6;
    int durationSec         = 0;
    bool verbose            = false;

    struct Neko {
        constexpr static auto value = // NOLINT
            Object("webui",
                   makeTags<arg_group<"WebUI">,
                            arg_value_name<"HOST:PORT">,
                            arg_default<"127.0.0.1:18080"_cs>,
                            arg_help<"WebUI dashboard bind address">>(&TraceExampleOptions::webui),

                   "backend",
                   makeTags<arg_short_name<'b'>,
                            arg_group<"RPC">,
                            arg_default<"jsonrpc"_cs>,
                            arg_choices<"jsonrpc", "nekorpc">,
                            arg_case_insensitive_choices,
                            arg_help<"RPC backend to use">>(&TraceExampleOptions::backend),

                   "concurrency",
                   makeTags<arg_short_name<'c'>,
                            arg_group<"Workload">,
                            arg_value_name<"N">,
                            arg_default<8>,
                            arg_help<"Max concurrent in-flight calls">,
                            ArgTags{.range_min = 1, .range_max = 256}>(&TraceExampleOptions::concurrency),

                   "rate",
                   makeTags<arg_short_name<'r'>,
                            arg_group<"Workload">,
                            arg_value_name<"RPS">,
                            arg_default<40>,
                            arg_help<"Approximate target request rate per second">,
                            ArgTags{.range_min = 1, .range_max = 10000}>(&TraceExampleOptions::rate),

                   "totalTasks",
                   makeTags<arg_long_name<"total-tasks">,
                            arg_group<"Workload">,
                            arg_value_name<"N">,
                            arg_default<0>,
                            arg_help<"Total tasks to run (0 = run forever until Ctrl+C)">,
                            ArgTags{.range_min = 0, .range_max = 10000000}>(&TraceExampleOptions::totalTasks),

                   "timeoutMs",
                   makeTags<arg_long_name<"timeout-ms">,
                            arg_group<"RPC">,
                            arg_value_name<"MS">,
                            arg_default<2000>,
                            arg_help<"Default call timeout in milliseconds">,
                            ArgTags{.range_min = 10, .range_max = 60000}>(&TraceExampleOptions::timeoutMs),

                   "chaosPct",
                   makeTags<arg_long_name<"chaos-pct">,
                            arg_group<"Workload">,
                            arg_value_name<"PCT">,
                            arg_default<8>,
                            arg_help<"Percentage of chaos calls (errors, timeouts, not-found)">,
                            ArgTags{.range_min = 0, .range_max = 100}>(&TraceExampleOptions::chaosPct),

                   "cancelPct",
                   makeTags<arg_long_name<"cancel-pct">,
                            arg_group<"Workload">,
                            arg_value_name<"PCT">,
                            arg_default<6>,
                            arg_help<"Percentage of cancellable calls aborted from registry">,
                            ArgTags{.range_min = 0, .range_max = 100}>(&TraceExampleOptions::cancelPct),

                   "durationSec",
                   makeTags<arg_short_name<'d'>,
                            arg_long_name<"duration">,
                            arg_group<"Workload">,
                            arg_value_name<"SEC">,
                            arg_default<0>,
                            arg_help<"Execution duration in seconds (0 = run until Ctrl+C)">,
                            ArgTags{.range_min = 0, .range_max = 86400}>(&TraceExampleOptions::durationSec),

                   "verbose",
                   makeTags<arg_short_name<'v'>,
                            arg_group<"General">,
                            arg_help<"Enable verbose logging to stdout">,
                            ArgTags{.flag = true}>(&TraceExampleOptions::verbose));
    };
};

// ─────────────────────────────────────────────────────────────────────────────
// Peer Definitions
// ─────────────────────────────────────────────────────────────────────────────

const std::vector<RpcPeerInfo> kPeers = {
    RpcPeerInfo{.id = "gateway-edge-01", .attributes = {{"region", "us-east-1"}, {"tier", "edge"}, {"role", "ingress"}}},
    RpcPeerInfo{.id = "worker-node-42", .attributes = {{"region", "eu-central-1"}, {"tier", "compute"}, {"env", "prod"}}},
    RpcPeerInfo{.id = "mobile-client-alice", .attributes = {{"device", "android-14"}, {"app_ver", "3.2.0"}, {"tenant", "vip-alice"}}},
    RpcPeerInfo{.id = "billing-svc-03", .attributes = {{"service", "billing"}, {"cluster", "k8s-finance"}, {"zone", "az-b"}}},
    RpcPeerInfo{.id = "monitor-probe-09", .attributes = {{"probe_type", "synthetic"}, {"interval_s", "5"}}},
    RpcPeerInfo{.id = "legacy-daemon-x", .attributes = {{"protocol", "v1-compat"}, {"host", "srv-old-01"}}},
};

// ─────────────────────────────────────────────────────────────────────────────
// Computation Helpers
// ─────────────────────────────────────────────────────────────────────────────

auto computeFib(int n) -> std::uint64_t {
    if (n <= 1) return n;
    std::uint64_t a = 0, b = 1;
    for (int i = 2; i <= n; ++i) {
        std::uint64_t c = a + b;
        a = b;
        b = c;
    }
    return b;
}

auto primeFactors(std::uint64_t n) -> std::vector<std::uint64_t> {
    std::vector<std::uint64_t> factors;
    while (n % 2 == 0) {
        factors.push_back(2);
        n /= 2;
    }
    for (std::uint64_t d = 3; d * d <= n; d += 2) {
        while (n % d == 0) {
            factors.push_back(d);
            n /= d;
        }
    }
    if (n > 2) {
        factors.push_back(n);
    }
    return factors;
}

auto calcChecksum(std::string_view text) -> std::uint32_t {
    std::uint32_t hash = 5381;
    for (char c : text) {
        hash = ((hash << 5) + hash) + static_cast<unsigned char>(c);
    }
    return hash;
}

// ─────────────────────────────────────────────────────────────────────────────
// Method Registration Helper
// ─────────────────────────────────────────────────────────────────────────────

template <typename ServerT>
void registerExampleMethods(ServerT& server) {
    // 1. Fast Math Calculations
    server.template bindMethod<"n">("math.fibonacci", traits::FunctionT<std::uint64_t(int)>([](int n) {
        return computeFib(std::clamp(n, 0, 80));
    }));

    server.template bindMethod<"n">("math.prime_factors", traits::FunctionT<std::vector<std::uint64_t>(std::uint64_t)>([](std::uint64_t n) {
        return primeFactors(std::clamp<std::uint64_t>(n, 2, 10'000'000'000ULL));
    }));

    server.template bindMethod<"dim">("math.matrix_mult", traits::FunctionT<std::uint64_t(int)>([](int dim) {
        const int size = std::clamp(dim, 2, 40);
        std::uint64_t sum = 0;
        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                for (int k = 0; k < size; ++k) {
                    sum += (i * size + k) * (k * size + j);
                }
            }
        }
        return sum;
    }));

    // 2. Database & I/O Simulation
    server.template bindMethod<"user_id">("db.query_user", traits::FunctionT<ilias::IoTask<std::string>(int)>([](int uid) -> ilias::IoTask<std::string> {
        // Simulates DB read latency (5ms ~ 30ms)
        const auto delay = std::chrono::milliseconds(5 + (uid % 25));
        co_await ilias::sleep(delay);
        std::ostringstream oss;
        oss << "{\"id\":" << uid << ",\"name\":\"User_" << uid << "\",\"status\":\"active\"}";
        co_return oss.str();
    }));

    server.template bindMethod<"table", "key", "val">("db.insert_record", traits::FunctionT<ilias::IoTask<bool>(std::string, std::string, std::string)>(
        [](std::string table, std::string key, std::string val) -> ilias::IoTask<bool> {
            // Simulates DB write commit latency (10ms ~ 50ms)
            const auto delay = std::chrono::milliseconds(10 + (calcChecksum(key) % 40));
            co_await ilias::sleep(delay);
            co_return true;
        }));

    // 3. String & Data Transformation
    server.template bindMethod<"input", "mode">("data.transform", traits::FunctionT<std::string(std::string, std::string)>(
        [](std::string input, std::string mode) {
            if (mode == "upper") {
                for (char& c : input) c = static_cast<char>(std::toupper(c));
            } else if (mode == "reverse") {
                std::reverse(input.begin(), input.end());
            } else {
                input = "[" + mode + "] " + input;
            }
            return input;
        }));

    server.template bindMethod<"text">("data.checksum", traits::FunctionT<std::uint32_t(std::string)>([](std::string text) {
        return calcChecksum(text);
    }));

    // 4. Heavy / Long-running Cancellable Tasks (with RpcRequestContext)
    server.template bindMethodWithContext<"duration_ms", "step_ms">(
        "job.cancellable_sleep",
        traits::FunctionT<ilias::IoTask<std::string>(const RpcRequestContext&, int, int)>(
            [](const RpcRequestContext& context, int duration_ms, int step_ms) -> ilias::IoTask<std::string> {
                const int totalMs = std::clamp(duration_ms, 50, 5000);
                const int step = std::clamp(step_ms, 10, 100);
                int elapsed = 0;
                while (elapsed < totalMs) {
                    if (context.cancellationToken().stop_requested()) {
                        co_return ilias::Err(ilias::IoError::Canceled);
                    }
                    co_await ilias::sleep(std::chrono::milliseconds(step));
                    elapsed += step;
                }
                co_return "Completed after " + std::to_string(elapsed) + "ms";
            }));

    server.template bindMethodWithContext<"iterations", "delay_ms">(
        "job.heavy_compute",
        traits::FunctionT<ilias::IoTask<std::uint64_t>(const RpcRequestContext&, int, int)>(
            [](const RpcRequestContext& context, int iterations, int delay_ms) -> ilias::IoTask<std::uint64_t> {
                const int iters = std::clamp(iterations, 1, 20);
                const int delay = std::clamp(delay_ms, 10, 100);
                std::uint64_t total = 0;
                for (int i = 0; i < iters; ++i) {
                    if (context.cancellationToken().stop_requested()) {
                        co_return ilias::Err(ilias::IoError::Canceled);
                    }
                    total += computeFib(30 + (i % 10));
                    co_await ilias::sleep(std::chrono::milliseconds(delay));
                }
                co_return total;
            }));

    // 5. Faulty / Chaos Injections
    server.template bindMethod<"seed">("chaos.random_fail", traits::FunctionT<ilias::IoTask<int>(int)>([](int seed) -> ilias::IoTask<int> {
        co_await ilias::sleep(15ms);
        if (seed % 3 == 0) {
            co_return ilias::Err(RpcError::InternalError);
        }
        if (seed % 5 == 0) {
            throw std::runtime_error("Simulated unhandled server exception!");
        }
        co_return seed * 2;
    }));

    server.template bindMethod<"hang_ms">("chaos.slow_hang", traits::FunctionT<ilias::IoTask<std::string>(int)>([](int hangMs) -> ilias::IoTask<std::string> {
        // Sleeps longer than typical timeout to trigger DeadlineExceeded
        co_await ilias::sleep(std::chrono::milliseconds(hangMs));
        co_return "Delayed response";
    }));

    // 6. Auth & System Info
    server.template bindMethod<"user", "pass">("auth.login", traits::FunctionT<std::string(std::string, std::string)>(
        [](std::string user, std::string /*pass*/) {
            return "session_token_for_" + user;
        }));

    server.bindMethod("metrics.system_status", traits::FunctionT<std::string()>([]() {
        return "{\"status\":\"healthy\",\"uptime_s\":12345,\"load\":0.42}";
    }));
}

// ─────────────────────────────────────────────────────────────────────────────
// Runner Implementation
// ─────────────────────────────────────────────────────────────────────────────

template <typename BackendT>
auto runWorkload(ilias::PlatformContext& context, const TraceExampleOptions& opt) -> ilias::Task<void> {
    using ServerType = RpcServer<BackendT>;
    using ClientType = RpcClient<BackendT>;

    typename BackendT::Options backendOptions;
    backendOptions.request_timeout = std::chrono::milliseconds(opt.timeoutMs);

    ServerType server{context, backendOptions};
    registerExampleMethods(server);

    // Setup simulated clients with different peer identities
    struct ClientSession {
        std::unique_ptr<ClientType> client;
        RpcPeerInfo peer;
    };

    std::vector<ClientSession> sessions;
    sessions.reserve(kPeers.size());

    for (const auto& peer : kPeers) {
        auto [clientStream, serverStream] = ilias::DuplexStream::make(131072);
        server.addEndpoint(std::move(serverStream), peer);

        auto client = std::make_unique<ClientType>(context, backendOptions);
        client->setEndpoint(std::move(clientStream));
        sessions.push_back(ClientSession{.client = std::move(client), .peer = peer});
    }

    std::atomic<std::uint64_t> dispatchedCount{0};
    std::atomic<std::uint64_t> completedCount{0};
    std::atomic<std::uint64_t> failedCount{0};
    std::atomic<std::uint64_t> canceledCount{0};
    std::atomic<std::uint64_t> timeoutCount{0};

    auto startTime = std::chrono::steady_clock::now();

    // Spawn stats reporter task
    auto statsReporter = [&]() -> ilias::Task<void> {
        while (gRunning) {
            co_await ilias::sleep(2000ms);
            if (!gRunning) break;

            const auto now = std::chrono::steady_clock::now();
            const auto elapsedSec = std::chrono::duration<double>(now - startTime).count();
            const auto rps = elapsedSec > 0 ? (completedCount.load() + failedCount.load()) / elapsedSec : 0.0;

            std::cout << "[Stats] "
                      << "Dispatched: " << dispatchedCount.load()
                      << " | Completed: " << completedCount.load()
                      << " | Failed: " << failedCount.load()
                      << " | Canceled: " << canceledCount.load()
                      << " | TimedOut: " << timeoutCount.load()
                      << " | Speed: " << static_cast<int>(rps) << " req/s"
                      << " | WebUI: http://" << opt.webui
                      << "\r" << std::flush;

            if (opt.durationSec > 0 && elapsedSec >= opt.durationSec) {
                std::cout << "\n[!] Target duration reached (" << opt.durationSec << "s), stopping.\n";
                gRunning = false;
                break;
            }
        }
    };

    // Client worker coroutine
    auto workerLoop = [&](std::size_t workerId) -> ilias::Task<void> {
        std::mt19937_64 rng(std::random_device{}() + workerId * 1000);
        std::uniform_int_distribution<int> peerDist(0, static_cast<int>(sessions.size() - 1));
        std::uniform_int_distribution<int> actionDist(0, 11);
        std::uniform_int_distribution<int> pctDist(1, 100);

        const auto sleepDelay = std::chrono::microseconds(std::max<int>(100, (1'000'000 * opt.concurrency) / opt.rate));

        while (gRunning) {
            if (opt.totalTasks > 0 && dispatchedCount.load() >= static_cast<std::uint64_t>(opt.totalTasks)) {
                gRunning = false;
                break;
            }

            auto& session = sessions[peerDist(rng)];
            auto& client = *session.client;
            const int action = actionDist(rng);
            const int pct = pctDist(rng);

            ++dispatchedCount;

            try {
                if (pct <= opt.chaosPct) {
                    // Inject chaos
                    const int chaosType = pct % 3;
                    if (chaosType == 0) {
                        // Call non-existent method
                        auto res = co_await client.template callRemote<int>("system.missing_service_method", 42);
                        if (!res) ++failedCount; else ++completedCount;
                    } else if (chaosType == 1) {
                        // Faulty method that throws / fails
                        auto res = co_await client.template callRemote<int>("chaos.random_fail", static_cast<int>(dispatchedCount.load()));
                        if (!res) ++failedCount; else ++completedCount;
                    } else {
                        // Slow hang exceeding timeout
                        RpcCallOptions callOpt;
                        callOpt.timeout = 50ms;
                        auto res = co_await client.template callRemoteWithOptions<std::string>("chaos.slow_hang", callOpt, 200);
                        if (!res) {
                            ++timeoutCount;
                            ++failedCount;
                        } else {
                            ++completedCount;
                        }
                    }
                } else if (action == 0) {
                    auto res = co_await client.template callRemote<std::uint64_t>("math.fibonacci", static_cast<int>(10 + (rng() % 35)));
                    if (res) ++completedCount; else ++failedCount;
                } else if (action == 1) {
                    auto res = co_await client.template callRemote<std::vector<std::uint64_t>>("math.prime_factors", static_cast<std::uint64_t>(1000 + (rng() % 500000)));
                    if (res) ++completedCount; else ++failedCount;
                } else if (action == 2) {
                    auto res = co_await client.template callRemote<std::uint64_t>("math.matrix_mult", static_cast<int>(5 + (rng() % 25)));
                    if (res) ++completedCount; else ++failedCount;
                } else if (action == 3) {
                    auto res = co_await client.template callRemote<std::string>("db.query_user", static_cast<int>(1 + (rng() % 1000)));
                    if (res) ++completedCount; else ++failedCount;
                } else if (action == 4) {
                    auto res = co_await client.template callRemote<bool>("db.insert_record", "users", "user_" + std::to_string(rng() % 500), "payload_data");
                    if (res) ++completedCount; else ++failedCount;
                } else if (action == 5) {
                    const char* modes[] = {"upper", "reverse", "audit", "encode"};
                    auto res = co_await client.template callRemote<std::string>("data.transform", "Hello NekoProto RPC Tracing!", modes[rng() % 4]);
                    if (res) ++completedCount; else ++failedCount;
                } else if (action == 6) {
                    auto res = co_await client.template callRemote<std::uint32_t>("data.checksum", "payload_to_hash_key_" + std::to_string(rng() % 10000));
                    if (res) ++completedCount; else ++failedCount;
                } else if (action == 7) {
                    auto res = co_await client.template callRemote<std::string>("auth.login", "user_alice_" + std::to_string(rng() % 20), "pass123");
                    if (res) ++completedCount; else ++failedCount;
                } else if (action == 8) {
                    auto res = co_await client.template callRemote<std::string>("metrics.system_status");
                    if (res) ++completedCount; else ++failedCount;
                } else if (action == 9) {
                    // Heavy cancellable compute task
                    if (pctDist(rng) <= opt.cancelPct) {
                        // Launch task and cancel it from registry
                        auto taskHandle = ilias::spawn(client.template callRemote<std::uint64_t>("job.heavy_compute", 8, 40));
                        co_await ilias::sleep(20ms);
                        // Trigger registry cancellation
                        bool cancelTriggered = RpcTraceRegistry::instance().requestCancelByTraceId(dispatchedCount.load());
                        if (!cancelTriggered) {
                            cancelTriggered = RpcTraceRegistry::instance().requestCancel(std::to_string(dispatchedCount.load()));
                        }
                        auto res = co_await std::move(taskHandle);
                        if (!res || !res.value().has_value()) {
                            ++canceledCount;
                            ++failedCount;
                        } else {
                            ++completedCount;
                        }
                    } else {
                        auto res = co_await client.template callRemote<std::uint64_t>("job.heavy_compute", 4, 20);
                        if (res) ++completedCount; else ++failedCount;
                    }
                } else {
                    // Cancellable sleep task
                    if (pctDist(rng) <= opt.cancelPct) {
                        auto taskHandle = ilias::spawn(client.template callRemote<std::string>("job.cancellable_sleep", 500, 20));
                        co_await ilias::sleep(25ms);
                        RpcTraceRegistry::instance().requestCancelByTraceId(dispatchedCount.load());
                        auto res = co_await std::move(taskHandle);
                        if (!res || !res.value().has_value()) {
                            ++canceledCount;
                            ++failedCount;
                        } else {
                            ++completedCount;
                        }
                    } else {
                        auto res = co_await client.template callRemote<std::string>("job.cancellable_sleep", 80, 20);
                        if (res) ++completedCount; else ++failedCount;
                    }
                }
            } catch (...) {
                ++failedCount;
            }

            co_await ilias::sleep(sleepDelay);
        }
    };

    co_await ilias::TaskScope::enter([&](ilias::TaskScope& scope) -> ilias::Task<void> {
        scope.spawn(statsReporter());
        for (int i = 0; i < opt.concurrency; ++i) {
            scope.spawn(workerLoop(static_cast<std::size_t>(i)));
        }
        co_return;
    });

    // Cleanup clients & server
    for (auto& s : sessions) {
        s.client->close();
    }
    server.close();
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Main Entry Point
// ─────────────────────────────────────────────────────────────────────────────

auto main(int argc, char** argv) -> int {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    ArgParserConfig parserCfg;
    parserCfg.programName = argc > 0 ? argv[0] : "test_rpc_trace_example";
    parserCfg.description = "NekoProto RPC Remote Call Tracing & WebUI Monitoring Demonstration";
    parserCfg.version = "1.0.0";
    parserCfg.allowShortCluster = true;

    auto parseResult = parser<TraceExampleOptions>(argc, argv, parserCfg);
    if (!parseResult) {
        if (parseResult.error() == makeErrorCode(ArgParserError::HelpRequested)) {
            std::cout << formatHelp<TraceExampleOptions>(argc, argv, parserCfg);
            return 0;
        }
        if (parseResult.error() == makeErrorCode(ArgParserError::VersionRequested)) {
            std::cout << formatVersion(parserCfg);
            return 0;
        }
        std::cerr << "Parse error: " << parseResult.error().message() << "\n";
        std::cout << formatHelp<TraceExampleOptions>(argc, argv, parserCfg);
        return 1;
    }

    const auto& opt = *parseResult;

    if (!opt.verbose) {
        NEKO_LOG_SET_LEVEL(NEKO_LOG_LEVEL_WARN);
    } else {
        NEKO_LOG_SET_LEVEL(NEKO_LOG_LEVEL_TRACE);
    }

    std::cout << "==================================================================\n";
    std::cout << "      NekoProto RPC Tracing & WebUI Monitoring Console Demo       \n";
    std::cout << "==================================================================\n";
    std::cout << " [Configuration]\n";
    std::cout << "  - WebUI Monitor     : http://" << opt.webui << "\n";
    std::cout << "  - RPC Backend       : " << opt.backend << "\n";
    std::cout << "  - Concurrency       : " << opt.concurrency << " workers\n";
    std::cout << "  - Target Rate       : ~" << opt.rate << " req/s\n";
    std::cout << "  - Call Timeout      : " << opt.timeoutMs << " ms\n";
    std::cout << "  - Chaos Rate        : " << opt.chaosPct << " %\n";
    std::cout << "  - Cancel Rate       : " << opt.cancelPct << " %\n";
    std::cout << "  - Max Tasks         : " << (opt.totalTasks > 0 ? std::to_string(opt.totalTasks) : "Unlimited (Ctrl+C to stop)") << "\n";
    std::cout << "  - Duration          : " << (opt.durationSec > 0 ? std::to_string(opt.durationSec) + "s" : "Unlimited") << "\n";
    std::cout << "------------------------------------------------------------------\n";
    std::cout << " [*] Open http://" << opt.webui << " in your browser to monitor live calls!\n";
    std::cout << " [*] Press Ctrl+C at any time to gracefully terminate.\n";
    std::cout << "------------------------------------------------------------------\n\n";

    ilias::PlatformContext context;
    context.install();

    // Start WebUI Dashboard Server
    RpcTracingWebUi webui(opt.webui);
    if (!webui.install()) {
        std::cerr << "[Warning] Failed to start WebUI server on " << opt.webui << " (port might be in use)\n";
    }

    // Run Workload
    if (opt.backend == "nekorpc") {
        runWorkload<BinaryRpcBackend>(context, opt).wait();
    } else {
        runWorkload<JsonRpcBackend>(context, opt).wait();
    }

    std::cout << "\n\n[✓] Workload finished cleanly. Exiting.\n";
    return 0;
}
