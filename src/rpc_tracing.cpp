#include "nekoproto/rpc/console.hpp"
#include "nekoproto/rpc/tracing.hpp"

#if defined(_MSC_VER)
#pragma warning(disable : 4996)
#endif

#if defined(NEKO_PROTO_RPC_TRACE)

#include <ilias/io.hpp>
#include <ilias/net.hpp>
#include <ilias/task.hpp>
#include <ilias/task/scope.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace nekoproto {

namespace {

auto escapeJson(std::string_view str) -> std::string {
    std::string out;
    out.reserve(str.size() + 8);
    for (char ch : str) {
        switch (ch) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(ch));
                out += buf;
            } else {
                out += ch;
            }
            break;
        }
    }
    return out;
}

auto formatTimePoint(std::chrono::system_clock::time_point tp) -> std::string {
    if (tp == std::chrono::system_clock::time_point{}) {
        return "-";
    }
    const auto timeT = std::chrono::system_clock::to_time_t(tp);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count() % 1000;
    std::tm tmBuf{};
#if defined(_WIN32)
    localtime_s(&tmBuf, &timeT);
#else
    localtime_r(&timeT, &tmBuf);
#endif
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                  tmBuf.tm_year + 1900, tmBuf.tm_mon + 1, tmBuf.tm_mday,
                  tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec, static_cast<int>(ms));
    return buf;
}

void serializeSpanJson(std::string& out, const RpcCallTraceSpan& span) {
    out += '{';
    out += "\"trace_id\":" + std::to_string(span.traceId) + ',';
    out += "\"request_id\":\"" + escapeJson(span.requestId) + "\",";
    out += "\"method\":\"" + escapeJson(span.methodName) + "\",";
    out += "\"stage\":\"" + std::string(toString(span.stage)) + "\",";
    out += "\"peer_id\":\"" + escapeJson(span.peerId) + "\",";

    char sessionBuf[32];
    std::snprintf(sessionBuf, sizeof(sessionBuf), "%p", span.session);
    out += "\"session\":\"" + std::string(sessionBuf) + "\",";

    out += "\"peer_attributes\":{";
    bool firstAttr = true;
    for (const auto& [k, v] : span.peerAttributes) {
        if (!firstAttr) out += ',';
        firstAttr = false;
        out += "\"" + escapeJson(k) + "\":\"" + escapeJson(v) + "\"";
    }
    out += "},";

    out += "\"received_time\":\"" + formatTimePoint(span.receivedTime) + "\",";
    out += "\"queue_duration_ms\":" + std::to_string(span.queueDurationMs()) + ',';
    out += "\"exec_duration_ms\":" + std::to_string(span.execDurationMs()) + ',';
    out += "\"total_duration_ms\":" + std::to_string(span.totalDurationMs()) + ',';

    if (auto remaining = span.remainingDeadlineMs(); remaining.has_value()) {
        out += "\"deadline_remaining_ms\":" + std::to_string(*remaining) + ',';
    } else {
        out += "\"deadline_remaining_ms\":null,";
    }

    if (span.timeout.has_value()) {
        const auto timeoutMs = std::chrono::duration<double, std::milli>(*span.timeout).count();
        out += "\"timeout_ms\":" + std::to_string(timeoutMs) + ',';
    } else {
        out += "\"timeout_ms\":null,";
    }

    out += "\"request_bytes\":" + std::to_string(span.requestBytes) + ',';
    out += "\"response_bytes\":" + std::to_string(span.responseBytes) + ',';
    out += "\"error_code\":" + std::to_string(span.errorCode) + ',';
    out += "\"error_message\":\"" + escapeJson(span.errorMessage) + "\",";
    out += "\"is_notification\":" + std::string(span.isNotification ? "true" : "false");
    out += '}';
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Embedded WebUI Resources
// ─────────────────────────────────────────────────────────────────────────────

constexpr std::string_view kIndexHtml = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>NekoProto RPC Console</title>
    <link rel="stylesheet" href="/styles.css">
</head>
<body>
    <header class="navbar">
        <div class="brand">
            <div class="logo">🐱</div>
            <div class="brand-text">
                <h1>NekoProto RPC Console</h1>
                <span class="subtext">Remote Call Lifecycle &amp; Service Monitor</span>
            </div>
        </div>
        <nav class="main-nav">
            <button class="nav-tab active" data-view="calls" onclick="switchView('calls')">📊 Calls Monitor</button>
            <button class="nav-tab" data-view="services" onclick="switchView('services')">🧩 Services &amp; Methods</button>
            <button class="nav-tab" data-view="server" onclick="switchView('server')">⚙️ Server &amp; Config</button>
        </nav>
        <div class="header-metrics" id="headerMetrics">
            <div class="metric-pill active-pill" title="Currently Active"><span class="pill-label">Active</span><span class="pill-val" id="metricActive">0</span></div>
            <div class="metric-pill queued-pill" title="In Queue"><span class="pill-label">Queued</span><span class="pill-val" id="metricQueued">0</span></div>
            <div class="metric-pill completed-pill" title="Total Completed"><span class="pill-label">Completed</span><span class="pill-val" id="metricCompleted">0</span></div>
            <div class="metric-pill failed-pill" title="Total Failed/Errors"><span class="pill-label">Failed</span><span class="pill-val" id="metricFailed">0</span></div>
            <div class="metric-pill canceled-pill" title="Total Canceled"><span class="pill-label">Canceled</span><span class="pill-val" id="metricCanceled">0</span></div>
        </div>
        <div class="header-actions">
            <button id="pauseBtn" class="btn btn-outline" onclick="togglePause()">Pause</button>
            <div class="dropdown">
                <select id="refreshRate" class="select" onchange="changeRefreshRate(this.value)">
                    <option value="500">500 ms</option>
                    <option value="1000" selected>1 s</option>
                    <option value="2000">2 s</option>
                    <option value="5000">5 s</option>
                </select>
            </div>
        </div>
    </header>

    <main class="container">
        <!-- 1. Calls Monitor View -->
        <section id="viewCalls" class="view-section active">
            <div class="controls-bar">
                <div class="tabs" id="tabBar">
                    <button class="tab-btn active" data-tab="active" onclick="switchTab('active')">Active Calls <span class="tab-count" id="countActive">0</span></button>
                    <button class="tab-btn" data-tab="completed" onclick="switchTab('completed')">Recent Completed <span class="tab-count" id="countCompleted">0</span></button>
                    <button class="tab-btn" data-tab="failed" onclick="switchTab('failed')">Failed &amp; Canceled <span class="tab-count" id="countFailed">0</span></button>
                </div>
                <div class="search-box">
                    <input type="text" id="searchInput" placeholder="Filter by method, id, peer..." oninput="onSearchChange()">
                </div>
            </div>

            <div class="table-card">
                <div class="table-responsive">
                    <table class="data-table">
                        <thead>
                            <tr>
                                <th onclick="setSort('trace_id')">Trace # <span id="sort-trace_id" class="sort-icon"></span></th>
                                <th onclick="setSort('request_id')">Req ID <span id="sort-request_id" class="sort-icon"></span></th>
                                <th onclick="setSort('method')">Method <span id="sort-method" class="sort-icon"></span></th>
                                <th onclick="setSort('peer_id')">Peer / Session <span id="sort-peer_id" class="sort-icon"></span></th>
                                <th onclick="setSort('stage')">Stage <span id="sort-stage" class="sort-icon"></span></th>
                                <th onclick="setSort('queue_duration_ms')">Queue <span id="sort-queue_duration_ms" class="sort-icon"></span></th>
                                <th onclick="setSort('exec_duration_ms')">Exec <span id="sort-exec_duration_ms" class="sort-icon"></span></th>
                                <th onclick="setSort('total_duration_ms')">Total <span id="sort-total_duration_ms" class="sort-icon"></span></th>
                                <th onclick="setSort('deadline_remaining_ms')">Deadline <span id="sort-deadline_remaining_ms" class="sort-icon"></span></th>
                                <th>In / Out</th>
                                <th>Actions</th>
                            </tr>
                        </thead>
                        <tbody id="tableBody"></tbody>
                    </table>
                </div>
                <div id="emptyState" class="empty-state" style="display: none;">
                    <div class="empty-icon">📭</div>
                    <div class="empty-text">No matching RPC calls recorded</div>
                </div>
            </div>
        </section>

        <!-- 2. Services & Methods View (Hierarchical Cards) -->
        <section id="viewServices" class="view-section">
            <div class="controls-bar">
                <div class="section-title-wrap">
                    <h2>Registered Services &amp; RPC Methods</h2>
                    <span class="sub-counter" id="servicesSummaryText">0 Methods registered</span>
                </div>
                <div class="search-box">
                    <input type="text" id="methodSearchInput" placeholder="Search methods, namespaces, signatures..." oninput="onMethodSearchChange()">
                </div>
            </div>

            <div id="servicesContainer" class="services-tree"></div>
        </section>

        <!-- 3. Server Info & Configuration View -->
        <section id="viewServer" class="view-section">
            <div class="server-grid">
                <div class="server-card">
                    <div class="card-icon">🚀</div>
                    <div class="card-content">
                        <span class="card-label">RPC Protocol Backend</span>
                        <h3 id="serverBackendName">JSON-RPC 2.0</h3>
                        <span class="card-desc">Active Transport &amp; Serializer Engine</span>
                    </div>
                </div>
                <div class="server-card">
                    <div class="card-icon">⚡</div>
                    <div class="card-content">
                        <span class="card-label">Concurrency Limiter</span>
                        <h3 id="serverMaxConcurrent">4096 Workers</h3>
                        <span class="card-desc">Max In-flight Active Coroutines</span>
                    </div>
                </div>
                <div class="server-card">
                    <div class="card-icon">📥</div>
                    <div class="card-content">
                        <span class="card-label">Queue Capacity</span>
                        <h3 id="serverMaxQueue">4096 Requests</h3>
                        <span class="card-desc">Maximum Admission Queue Size</span>
                    </div>
                </div>
                <div class="server-card">
                    <div class="card-icon">⏱️</div>
                    <div class="card-content">
                        <span class="card-label">Default Call Timeout</span>
                        <h3 id="serverDefaultTimeout">None</h3>
                        <span class="card-desc">Server-enforced Deadline Limit</span>
                    </div>
                </div>
                <div class="server-card">
                    <div class="card-icon">⏳</div>
                    <div class="card-content">
                        <span class="card-label">Server Uptime</span>
                        <h3 id="serverUptime">0s</h3>
                        <span class="card-desc" id="serverStartTime">Started at -</span>
                    </div>
                </div>
                <div class="server-card">
                    <div class="card-icon">📜</div>
                    <div class="card-content">
                        <span class="card-label">Registered Methods</span>
                        <h3 id="serverMethodCount">0</h3>
                        <span class="card-desc">Available RPC Dispatch Handlers</span>
                    </div>
                </div>
            </div>

            <div class="info-banner">
                <div class="banner-title">💡 Architecture Note</div>
                <p>NekoProto RPC operates on the single-threaded asynchronous <strong>Ilias Coroutine Reactor</strong>. All method invocations, timeout timers, WebUI monitoring, and cancellation signals are executed cooperatively without OS thread blocking or lock contention.</p>
            </div>
        </section>
    </main>

    <!-- Detail Modal -->
    <div id="detailModal" class="modal">
        <div class="modal-content">
            <div class="modal-header">
                <h3 id="modalTitle">Call Trace Details</h3>
                <span class="close-btn" onclick="closeModal()">&times;</span>
            </div>
            <div class="modal-body" id="modalContent"></div>
        </div>
    </div>

    <!-- Notification Toast -->
    <div id="toast" class="toast"></div>

    <script src="/script.js" defer></script>
</body>
</html>)HTML";

constexpr std::string_view kStylesCss1 = R"CSS(:root {
    --bg-main: #0d1117;
    --bg-card: #161b22;
    --bg-hover: #21262d;
    --border-color: #30363d;
    --border-glow: #388bfd44;
    --text-main: #c9d1d9;
    --text-muted: #8b949e;
    --text-heading: #f0f6fc;
    --primary: #58a6ff;
    --primary-hover: #79b8ff;
    --stage-received: #388bfd;
    --stage-queued: #a371f7;
    --stage-executing: #d29922;
    --stage-sending: #39c5bb;
    --stage-completed: #3fb950;
    --stage-failed: #f85149;
    --stage-canceled: #6e7681;
    --stage-timeout: #db6d28;
    --font-mono: ui-monospace, SFMono-Regular, "SF Mono", Menlo, Consolas, "Liberation Mono", monospace;
    --font-sans: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif;
}

* { box-sizing: border-box; margin: 0; padding: 0; }
body {
    background-color: var(--bg-main);
    color: var(--text-main);
    font-family: var(--font-sans);
    font-size: 14px;
    line-height: 1.5;
    min-height: 100vh;
}

.navbar {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 12px 24px;
    background-color: var(--bg-card);
    border-bottom: 1px solid var(--border-color);
    position: sticky;
    top: 0;
    z-index: 100;
    gap: 16px;
}

.brand {
    display: flex;
    align-items: center;
    gap: 12px;
}
.logo { font-size: 24px; }
.brand-text h1 {
    font-size: 16px;
    color: var(--text-heading);
    font-weight: 600;
}
.brand-text .subtext {
    font-size: 11px;
    color: var(--text-muted);
}

.main-nav {
    display: flex;
    gap: 4px;
    background-color: #0d1117;
    padding: 4px;
    border-radius: 8px;
    border: 1px solid var(--border-color);
}
.nav-tab {
    background: transparent;
    border: 1px solid transparent;
    color: var(--text-muted);
    padding: 6px 16px;
    border-radius: 6px;
    cursor: pointer;
    font-weight: 600;
    font-size: 13px;
    transition: all 0.15s ease;
    display: flex;
    align-items: center;
    gap: 6px;
}
.nav-tab:hover {
    color: var(--text-heading);
    background-color: var(--bg-hover);
}
.nav-tab.active {
    color: #ffffff;
    background-color: #1f6feb;
    border-color: #388bfd;
    box-shadow: 0 0 10px rgba(56, 139, 253, 0.4);
}

.header-metrics {
    display: flex;
    gap: 8px;
}
.metric-pill {
    display: flex;
    align-items: center;
    gap: 6px;
    padding: 4px 10px;
    border-radius: 20px;
    font-size: 12px;
    font-weight: 500;
    background-color: #21262d;
    border: 1px solid var(--border-color);
}
.active-pill .pill-val { color: var(--stage-executing); font-weight: 700; }
.queued-pill .pill-val { color: var(--stage-queued); font-weight: 700; }
.completed-pill .pill-val { color: var(--stage-completed); font-weight: 700; }
.failed-pill .pill-val { color: var(--stage-failed); font-weight: 700; }
.canceled-pill .pill-val { color: var(--stage-canceled); font-weight: 700; }

.header-actions {
    display: flex;
    align-items: center;
    gap: 10px;
}

.btn {
    padding: 5px 12px;
    border-radius: 6px;
    font-size: 12px;
    font-weight: 500;
    cursor: pointer;
    border: 1px solid transparent;
    transition: background-color 0.15s ease;
}
.btn-outline {
    background-color: transparent;
    border-color: var(--border-color);
    color: var(--text-main);
}
.btn-outline:hover {
    background-color: var(--bg-hover);
    color: var(--text-heading);
}
.btn-outline.paused {
    background-color: #d2992222;
    border-color: #d29922;
    color: #d29922;
}
.btn-danger {
    background-color: #da3633;
    color: #ffffff;
}
.btn-danger:hover {
    background-color: #b62324;
}

.select {
    background-color: var(--bg-main);
    color: var(--text-main);
    border: 1px solid var(--border-color);
    padding: 4px 8px;
    border-radius: 6px;
    font-size: 12px;
}

.container {
    max-width: 1480px;
    margin: 20px auto;
    padding: 0 20px;
}

.view-section {
    display: none;
}
.view-section.active {
    display: block;
}

.controls-bar {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 16px;
    gap: 16px;
}
.section-title-wrap h2 {
    font-size: 18px;
    color: var(--text-heading);
    font-weight: 600;
}
.section-title-wrap .sub-counter {
    font-size: 12px;
    color: var(--text-muted);
}

.tabs {
    display: flex;
    gap: 6px;
    background-color: var(--bg-card);
    padding: 4px;
    border-radius: 8px;
    border: 1px solid var(--border-color);
}
.tab-btn {
    background: transparent;
    border: none;
    color: var(--text-muted);
    padding: 6px 14px;
    border-radius: 6px;
    cursor: pointer;
    font-weight: 500;
    display: flex;
    align-items: center;
    gap: 8px;
    transition: all 0.15s ease;
}
.tab-btn:hover {
    color: var(--text-heading);
}
.tab-btn.active {
    background-color: var(--bg-hover);
    color: var(--text-heading);
    border: 1px solid var(--border-color);
}
.tab-count {
    background-color: #30363d;
    padding: 1px 7px;
    border-radius: 10px;
    font-size: 11px;
}

.search-box input {
    background-color: var(--bg-card);
    border: 1px solid var(--border-color);
    color: var(--text-main);
    padding: 7px 14px;
    border-radius: 6px;
    width: 280px;
    font-size: 13px;
}
.search-box input:focus {
    outline: none;
    border-color: var(--primary);
}

.table-card {
    background-color: var(--bg-card);
    border: 1px solid var(--border-color);
    border-radius: 8px;
    overflow: hidden;
}
.table-responsive {
    overflow-x: auto;
}
.data-table {
    width: 100%;
    border-collapse: collapse;
    text-align: left;
}
.data-table th {
    background-color: #1c2128;
    color: var(--text-muted);
    font-weight: 600;
    padding: 10px 14px;
    border-bottom: 1px solid var(--border-color);
    font-size: 12px;
    user-select: none;
    cursor: pointer;
    white-space: nowrap;
}
.data-table th:hover {
    color: var(--text-heading);
}
.data-table td {
    padding: 10px 14px;
    border-bottom: 1px solid #21262d;
    font-size: 13px;
    white-space: nowrap;
}
.data-table tbody tr {
    cursor: pointer;
    transition: background-color 0.1s ease;
}
.data-table tbody tr:hover {
    background-color: var(--bg-hover);
}
)CSS";

constexpr std::string_view kStylesCss2 = R"CSS(
.mono {
    font-family: var(--font-mono);
    font-size: 12px;
}
.method-name {
    font-family: var(--font-mono);
    font-weight: 600;
    color: var(--primary);
}

.badge-stage {
    display: inline-block;
    padding: 2px 8px;
    border-radius: 12px;
    font-size: 11px;
    font-weight: 600;
    text-transform: uppercase;
}
.badge-received  { background-color: #388bfd22; color: var(--stage-received); border: 1px solid #388bfd55; }
.badge-queued    { background-color: #a371f722; color: var(--stage-queued); border: 1px solid #a371f755; }
.badge-executing { background-color: #d2992222; color: var(--stage-executing); border: 1px solid #d2992255; }
.badge-sending   { background-color: #39c5bb22; color: var(--stage-sending); border: 1px solid #39c5bb55; }
.badge-completed { background-color: #3fb95022; color: var(--stage-completed); border: 1px solid #3fb95055; }
.badge-failed    { background-color: #f8514922; color: var(--stage-failed); border: 1px solid #f8514955; }
.badge-canceled  { background-color: #6e768122; color: var(--stage-canceled); border: 1px solid #6e768155; }
.badge-timedout  { background-color: #db6d2822; color: var(--stage-timeout); border: 1px solid #db6d2855; }
.badge-rejected  { background-color: #f8514922; color: var(--stage-failed); border: 1px solid #f8514955; }

.empty-state {
    padding: 60px 20px;
    text-align: center;
}
.empty-icon { font-size: 36px; margin-bottom: 10px; }
.empty-text { color: var(--text-muted); font-size: 14px; }

/* ─── Services & Method Cards ─── */
.services-tree {
    display: flex;
    flex-direction: column;
    gap: 20px;
}
.service-group {
    background-color: var(--bg-card);
    border: 1px solid var(--border-color);
    border-radius: 8px;
    overflow: hidden;
}
.service-group-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 12px 18px;
    background-color: #1c2128;
    border-bottom: 1px solid var(--border-color);
    cursor: pointer;
    user-select: none;
}
.service-group-title {
    display: flex;
    align-items: center;
    gap: 10px;
    font-weight: 600;
    font-size: 15px;
    color: var(--text-heading);
}
.service-count-badge {
    background-color: #30363d;
    padding: 2px 8px;
    border-radius: 12px;
    font-size: 11px;
    color: var(--text-muted);
}
.method-grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(340px, 1fr));
    gap: 14px;
    padding: 16px;
}
.method-card {
    background-color: #0e1217;
    border: 1px solid var(--border-color);
    border-radius: 6px;
    padding: 14px;
    display: flex;
    flex-direction: column;
    gap: 10px;
    transition: all 0.15s ease;
}
.method-card:hover {
    border-color: var(--primary);
    box-shadow: 0 0 10px var(--border-glow);
}
.method-card-top {
    display: flex;
    justify-content: space-between;
    align-items: flex-start;
    gap: 8px;
}
.method-card-name {
    font-family: var(--font-mono);
    font-size: 14px;
    font-weight: 700;
    color: var(--primary);
    word-break: break-all;
}
.method-badges {
    display: flex;
    flex-wrap: wrap;
    gap: 4px;
}
.m-badge {
    font-size: 10px;
    padding: 2px 6px;
    border-radius: 4px;
    font-weight: 600;
}
.m-badge-notif { background: #6e768133; color: #8b949e; border: 1px solid #6e768155; }
.m-badge-ctx { background: #a371f722; color: #a371f7; border: 1px solid #a371f755; }
.m-badge-bound { background: #3fb95022; color: #3fb950; border: 1px solid #3fb95055; }
.m-badge-ver { background: #21262d; color: var(--text-muted); border: 1px solid var(--border-color); }

.method-sig {
    background-color: #161b22;
    border: 1px solid #30363d;
    padding: 6px 10px;
    border-radius: 4px;
    font-family: var(--font-mono);
    font-size: 12px;
    color: #e6edf3;
    overflow-x: auto;
}
.method-args-wrap {
    display: flex;
    flex-wrap: wrap;
    gap: 6px;
}
.arg-pill {
    background-color: #21262d;
    border: 1px solid #30363d;
    border-radius: 12px;
    padding: 2px 8px;
    font-family: var(--font-mono);
    font-size: 11px;
    color: #79c0ff;
}
.method-desc {
    font-size: 12px;
    color: var(--text-muted);
    line-height: 1.4;
}

/* ─── Server Info Grid ─── */
.server-grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
    gap: 16px;
    margin-bottom: 24px;
}
.server-card {
    background-color: var(--bg-card);
    border: 1px solid var(--border-color);
    border-radius: 8px;
    padding: 20px;
    display: flex;
    align-items: center;
    gap: 16px;
}
.card-icon {
    font-size: 32px;
}
.card-content h3 {
    font-size: 18px;
    color: var(--text-heading);
    font-weight: 700;
    margin: 2px 0;
}
.card-label {
    font-size: 11px;
    color: var(--text-muted);
    text-transform: uppercase;
    font-weight: 600;
}
.card-desc {
    font-size: 12px;
    color: var(--text-muted);
}

.info-banner {
    background-color: #161b22;
    border: 1px solid #388bfd44;
    border-left: 4px solid var(--primary);
    border-radius: 6px;
    padding: 16px;
}
.banner-title {
    font-weight: 600;
    color: var(--primary);
    margin-bottom: 6px;
}
.info-banner p {
    font-size: 13px;
    color: var(--text-main);
    line-height: 1.5;
}

/* ─── Modal ─── */
.modal {
    display: none;
    position: fixed;
    top: 0; left: 0; width: 100%; height: 100%;
    background-color: rgba(0, 0, 0, 0.7);
    z-index: 200;
    justify-content: center;
    align-items: center;
}
.modal.open {
    display: flex;
}
.modal-content {
    background-color: var(--bg-card);
    border: 1px solid var(--border-color);
    border-radius: 8px;
    width: 650px;
    max-width: 90vw;
    max-height: 85vh;
    overflow-y: auto;
    box-shadow: 0 10px 30px rgba(0, 0, 0, 0.5);
}
.modal-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 14px 20px;
    border-bottom: 1px solid var(--border-color);
}
.modal-header h3 {
    font-size: 16px;
    color: var(--text-heading);
}
.close-btn {
    font-size: 20px;
    color: var(--text-muted);
    cursor: pointer;
}
.close-btn:hover { color: var(--text-heading); }
.modal-body {
    padding: 20px;
}
.detail-row {
    display: flex;
    margin-bottom: 10px;
    font-size: 13px;
}
.detail-label {
    width: 140px;
    color: var(--text-muted);
    font-weight: 500;
}
.detail-val {
    flex: 1;
    color: var(--text-heading);
    word-break: break-all;
}

/* ─── Toast ─── */
.toast {
    position: fixed;
    bottom: 24px;
    right: 24px;
    background-color: #21262d;
    border: 1px solid var(--border-color);
    color: var(--text-heading);
    padding: 10px 18px;
    border-radius: 6px;
    box-shadow: 0 4px 12px rgba(0,0,0,0.4);
    transform: translateY(100px);
    opacity: 0;
    transition: all 0.2s ease;
    z-index: 300;
}
.toast.show {
    transform: translateY(0);
    opacity: 1;
}
)CSS";

constexpr std::string_view kScriptJs1 = R"JS(
let currentView = 'calls';
let currentTab = 'active';
let refreshInterval = 1000;
let timerId = null;
let isPaused = false;
let sortField = 'trace_id';
let sortAsc = false;
let searchQuery = '';
let methodSearchQuery = '';

let metrics = {};
let activeData = [];
let recentCompletedData = [];
let recentFailedData = [];
let serverData = {};
let methodsData = [];

document.addEventListener('DOMContentLoaded', () => {
    fetchSnapshot();
    fetchServerInfo();
    startTimer();
});

function startTimer() {
    if (timerId) clearInterval(timerId);
    if (!isPaused && refreshInterval > 0) {
        timerId = setInterval(() => {
            fetchSnapshot();
            if (currentView === 'server' || currentView === 'services') {
                fetchServerInfo();
            }
        }, refreshInterval);
    }
}

function togglePause() {
    isPaused = !isPaused;
    const btn = document.getElementById('pauseBtn');
    if (isPaused) {
        btn.innerText = 'Resume';
        btn.classList.add('paused');
    } else {
        btn.innerText = 'Pause';
        btn.classList.remove('paused');
        fetchSnapshot();
    }
}

function changeRefreshRate(val) {
    refreshInterval = parseInt(val, 10);
    startTimer();
}

function switchView(view) {
    currentView = view;
    document.querySelectorAll('.nav-tab').forEach(b => {
        b.classList.toggle('active', b.dataset.view === view);
    });
    document.querySelectorAll('.view-section').forEach(sec => {
        sec.classList.remove('active');
    });
    if (view === 'calls') {
        document.getElementById('viewCalls').classList.add('active');
        renderTable();
    } else if (view === 'services') {
        document.getElementById('viewServices').classList.add('active');
        fetchServerInfo();
    } else if (view === 'server') {
        document.getElementById('viewServer').classList.add('active');
        fetchServerInfo();
    }
}

function switchTab(tab) {
    currentTab = tab;
    document.querySelectorAll('.tab-btn').forEach(btn => {
        btn.classList.toggle('active', btn.dataset.tab === tab);
    });
    renderTable();
}

function onSearchChange() {
    searchQuery = document.getElementById('searchInput').value.trim().toLowerCase();
    renderTable();
}

function onMethodSearchChange() {
    methodSearchQuery = document.getElementById('methodSearchInput').value.trim().toLowerCase();
    renderServicesTree();
}

function setSort(field) {
    if (sortField === field) {
        sortAsc = !sortAsc;
    } else {
        sortField = field;
        sortAsc = false;
    }
    renderTable();
}

async function fetchSnapshot() {
    if (isPaused) return;
    try {
        const res = await fetch('/api/calls');
        if (!res.ok) return;
        const data = await res.json();
        metrics = data.metrics || metrics;
        activeData = data.active || [];
        recentCompletedData = data.recent_completed || [];
        recentFailedData = data.recent_failed || [];

        updateMetricsHeader();
        if (currentView === 'calls') {
            renderTable();
        }
    } catch (e) {
        console.error('Failed to fetch snapshot:', e);
    }
}

async function fetchServerInfo() {
    try {
        const res = await fetch('/api/server');
        if (!res.ok) return;
        const data = await res.json();
        serverData = data.server || {};
        methodsData = data.methods || [];

        renderServerView();
        renderServicesTree();
    } catch (e) {
        console.error('Failed to fetch server info:', e);
    }
}

function updateMetricsHeader() {
    document.getElementById('metricActive').innerText = metrics.active || 0;
    document.getElementById('metricQueued').innerText = metrics.queued || 0;
    document.getElementById('metricCompleted').innerText = metrics.completed || 0;
    document.getElementById('metricFailed').innerText = (metrics.timed_out + metrics.rejected + metrics.failed) || 0;
    document.getElementById('metricCanceled').innerText = metrics.canceled || 0;

    document.getElementById('countActive').innerText = activeData.length;
    document.getElementById('countCompleted').innerText = recentCompletedData.length;
    document.getElementById('countFailed').innerText = recentFailedData.length;
}

function getDataSource() {
    if (currentTab === 'active') {
        return activeData.filter(s => s.stage === 'Received' || s.stage === 'Queued' || s.stage === 'Executing' || s.stage === 'Sending');
    }
    if (currentTab === 'completed') {
        return recentCompletedData.filter(s => s.stage === 'Completed');
    }
    if (currentTab === 'failed') {
        return recentFailedData.filter(s => s.stage === 'Failed' || s.stage === 'Canceled' || s.stage === 'TimedOut' || s.stage === 'Rejected');
    }
    return [];
}

function formatDuration(ms) {
    if (ms === undefined || ms === null) return '-';
    if (ms < 1) return (ms * 1000).toFixed(0) + ' µs';
    if (ms < 1000) return ms.toFixed(2) + ' ms';
    return (ms / 1000).toFixed(2) + ' s';
}
)JS";

constexpr std::string_view kScriptJs2 = R"JS(
function renderTable() {
    let list = [...getDataSource()];

    if (searchQuery) {
        list = list.filter(item => {
            return (item.method && item.method.toLowerCase().includes(searchQuery)) ||
                   (item.request_id && item.request_id.toLowerCase().includes(searchQuery)) ||
                   (item.peer_id && item.peer_id.toLowerCase().includes(searchQuery)) ||
                   (item.session && item.session.toLowerCase().includes(searchQuery)) ||
                   (item.stage && item.stage.toLowerCase().includes(searchQuery));
        });
    }

    list.sort((a, b) => {
        let va = a[sortField];
        let vb = b[sortField];
        if (typeof va === 'string') {
            return sortAsc ? va.localeCompare(vb) : vb.localeCompare(va);
        }
        return sortAsc ? (va || 0) - (vb || 0) : (vb || 0) - (va || 0);
    });

    const tbody = document.getElementById('tableBody');
    const emptyState = document.getElementById('emptyState');
    tbody.innerHTML = '';

    if (list.length === 0) {
        emptyState.style.display = 'block';
    } else {
        emptyState.style.display = 'none';
    }

    list.forEach(span => {
        const tr = document.createElement('tr');
        tr.onclick = () => openDetails(span);

        const stageLower = (span.stage || 'unknown').toLowerCase();
        const stageClass = `badge-${stageLower}`;
        const canCancel = (span.stage === 'Received' || span.stage === 'Queued' || span.stage === 'Executing');

        let deadlineText = '-';
        if (span.deadline_remaining_ms !== null && span.deadline_remaining_ms !== undefined) {
            deadlineText = span.deadline_remaining_ms > 0 ? (span.deadline_remaining_ms.toFixed(0) + ' ms rem') : 'Expired';
        } else if (span.timeout_ms !== null && span.timeout_ms !== undefined) {
            deadlineText = span.timeout_ms.toFixed(0) + ' ms limit';
        }

        tr.innerHTML = `
            <td class="mono">#${span.trace_id}</td>
            <td class="mono">${escapeHtml(span.request_id || '-')}</td>
            <td class="method-name">${escapeHtml(span.method || '')}</td>
            <td class="mono" title="${escapeHtml(span.peer_id || span.session || '-')}">${escapeHtml(span.peer_id || span.session || '-')}</td>
            <td><span class="badge-stage ${stageClass}">${escapeHtml(span.stage)}</span></td>
            <td>${formatDuration(span.queue_duration_ms)}</td>
            <td>${formatDuration(span.exec_duration_ms)}</td>
            <td>${formatDuration(span.total_duration_ms)}</td>
            <td>${deadlineText}</td>
            <td class="mono">${span.request_bytes || 0} / ${span.response_bytes || 0} B</td>
            <td>
                ${canCancel ? `<button class="btn btn-danger" onclick="cancelCall(event, '${escapeHtml(span.request_id)}', ${span.trace_id})">Cancel</button>` : '-'}
            </td>
        `;
        tbody.appendChild(tr);
    });

    document.querySelectorAll('.sort-icon').forEach(el => el.innerText = '');
    const sortEl = document.getElementById(`sort-${sortField}`);
    if (sortEl) {
        sortEl.innerText = sortAsc ? ' ▲' : ' ▼';
    }
}

function renderServicesTree() {
    const container = document.getElementById('servicesContainer');
    const summaryText = document.getElementById('servicesSummaryText');
    if (!container) return;

    let methods = [...methodsData];
    if (methodSearchQuery) {
        methods = methods.filter(m => {
            return (m.name && m.name.toLowerCase().includes(methodSearchQuery)) ||
                   (m.signature && m.signature.toLowerCase().includes(methodSearchQuery)) ||
                   (m.description && m.description.toLowerCase().includes(methodSearchQuery));
        });
    }

    if (summaryText) {
        summaryText.innerText = `${methods.length} of ${methodsData.length} methods registered`;
    }

    if (methods.length === 0) {
        container.innerHTML = `
            <div class="empty-state">
                <div class="empty-icon">🔍</div>
                <div class="empty-text">No matching services or methods registered</div>
            </div>`;
        return;
    }

    const groups = {};
    methods.forEach(m => {
        const dotIdx = m.name.indexOf('.');
        const groupName = dotIdx > 0 ? m.name.substring(0, dotIdx) : 'general';
        if (!groups[groupName]) groups[groupName] = [];
        groups[groupName].push(m);
    });

    container.innerHTML = '';
    const sortedGroupNames = Object.keys(groups).sort();

    sortedGroupNames.forEach(grp => {
        const items = groups[grp];
        const groupCard = document.createElement('div');
        groupCard.className = 'service-group';

        let groupIcon = '📦';
        if (grp === 'math') groupIcon = '🧮';
        else if (grp === 'db') groupIcon = '🗄️';
        else if (grp === 'job') groupIcon = '⚙️';
        else if (grp === 'data') groupIcon = '🔄';
        else if (grp === 'chaos') groupIcon = '💥';
        else if (grp === 'rpc') groupIcon = '🔌';
        else if (grp === 'auth') groupIcon = '🔒';

        groupCard.innerHTML = `
            <div class="service-group-header">
                <div class="service-group-title">
                    <span>${groupIcon}</span>
                    <span>${escapeHtml(grp)}</span>
                    <span class="service-count-badge">${items.length} methods</span>
                </div>
            </div>
            <div class="method-grid" id="grid-${grp}"></div>
        `;

        const grid = groupCard.querySelector(`#grid-${grp}`);
        items.forEach(m => {
            const card = document.createElement('div');
            card.className = 'method-card';

            const argPills = (m.args && m.args.length > 0)
                ? m.args.map(a => `<span class="arg-pill">param: ${escapeHtml(a)}</span>`).join('')
                : '<span class="card-desc">No parameters</span>';

            card.innerHTML = `
                <div class="method-card-top">
                    <span class="method-card-name">${escapeHtml(m.name)}</span>
                    <div class="method-badges">
                        ${m.is_notification ? '<span class="m-badge m-badge-notif">Notification</span>' : '<span class="m-badge m-badge-bound">Req-Resp</span>'}
                        ${m.uses_context ? '<span class="m-badge m-badge-ctx">Context</span>' : ''}
                        ${m.version ? `<span class="m-badge m-badge-ver">${escapeHtml(m.version)}</span>` : ''}
                    </div>
                </div>
                <div class="method-sig">${escapeHtml(m.signature || m.name)}</div>
                <div class="method-args-wrap">${argPills}</div>
                ${m.description ? `<div class="method-desc">${escapeHtml(m.description)}</div>` : ''}
            `;
            grid.appendChild(card);
        });

        container.appendChild(groupCard);
    });
}
)JS";

constexpr std::string_view kScriptJs3 = R"JS(
function renderServerView() {
    if (!serverData) return;
    if (document.getElementById('serverBackendName')) {
        document.getElementById('serverBackendName').innerText = serverData.backend || 'Unknown';
    }
    if (document.getElementById('serverMaxConcurrent')) {
        document.getElementById('serverMaxConcurrent').innerText = (serverData.max_concurrent || 4096) + ' Workers';
    }
    if (document.getElementById('serverMaxQueue')) {
        document.getElementById('serverMaxQueue').innerText = (serverData.max_queue || 4096) + ' Requests';
    }
    if (document.getElementById('serverDefaultTimeout')) {
        document.getElementById('serverDefaultTimeout').innerText = serverData.default_timeout_ms ? (serverData.default_timeout_ms + ' ms') : 'None';
    }
    if (document.getElementById('serverStartTime')) {
        document.getElementById('serverStartTime').innerText = 'Started at ' + (serverData.start_time || '-');
    }
    if (document.getElementById('serverMethodCount')) {
        document.getElementById('serverMethodCount').innerText = serverData.methods_count || methodsData.length || 0;
    }
    if (document.getElementById('serverUptime')) {
        const sec = serverData.uptime_seconds || 0;
        const h = Math.floor(sec / 3600);
        const m = Math.floor((sec % 3600) / 60);
        const s = sec % 60;
        document.getElementById('serverUptime').innerText = `${h}h ${m}m ${s}s`;
    }
}

async function cancelCall(e, reqId, traceId) {
    e.stopPropagation();
    if (!confirm(`Cancel RPC call (Req ID: ${reqId}, Trace: #${traceId})?`)) return;

    try {
        const res = await fetch('/api/cancel', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ id: reqId, trace_id: traceId })
        });
        const resp = await res.json();
        if (resp.ok) {
            showToast(`Cancelled call #${traceId}`);
            fetchSnapshot();
        } else {
            showToast(`Cancel failed: ${resp.error || 'Unknown error'}`);
        }
    } catch (err) {
        showToast(`Error sending cancel request: ${err.message}`);
    }
}

function openDetails(span) {
    const modal = document.getElementById('detailModal');
    const title = document.getElementById('modalTitle');
    const content = document.getElementById('modalContent');

    title.innerText = `Trace #${span.trace_id} Details (${span.method})`;

    let peerAttrsHtml = '-';
    if (span.peer_attributes && Object.keys(span.peer_attributes).length > 0) {
        peerAttrsHtml = Object.entries(span.peer_attributes)
            .map(([k, v]) => `<div><code>${escapeHtml(k)}</code>: <code>${escapeHtml(v)}</code></div>`)
            .join('');
    }

    content.innerHTML = `
        <div class="detail-row"><span class="detail-label">Trace ID:</span><span class="detail-val mono">#${span.trace_id}</span></div>
        <div class="detail-row"><span class="detail-label">Request ID:</span><span class="detail-val mono">${escapeHtml(span.request_id || '-')}</span></div>
        <div class="detail-row"><span class="detail-label">Method Name:</span><span class="detail-val method-name">${escapeHtml(span.method || '-')}</span></div>
        <div class="detail-row"><span class="detail-label">Current Stage:</span><span class="detail-val"><span class="badge-stage badge-${(span.stage||'').toLowerCase()}">${escapeHtml(span.stage)}</span></span></div>
        <div class="detail-row"><span class="detail-label">Peer ID:</span><span class="detail-val mono">${escapeHtml(span.peer_id || '-')}</span></div>
        <div class="detail-row"><span class="detail-label">Session Ptr:</span><span class="detail-val mono">${escapeHtml(span.session || '-')}</span></div>
        <div class="detail-row"><span class="detail-label">Peer Attributes:</span><span class="detail-val">${peerAttrsHtml}</span></div>
        <div class="detail-row"><span class="detail-label">Received At:</span><span class="detail-val mono">${escapeHtml(span.received_time || '-')}</span></div>
        <div class="detail-row"><span class="detail-label">Queue Time:</span><span class="detail-val mono">${formatDuration(span.queue_duration_ms)}</span></div>
        <div class="detail-row"><span class="detail-label">Execution Time:</span><span class="detail-val mono">${formatDuration(span.exec_duration_ms)}</span></div>
        <div class="detail-row"><span class="detail-label">Total Duration:</span><span class="detail-val mono">${formatDuration(span.total_duration_ms)}</span></div>
        <div class="detail-row"><span class="detail-label">Timeout Limit:</span><span class="detail-val mono">${span.timeout_ms ? (span.timeout_ms + ' ms') : 'None'}</span></div>
        <div class="detail-row"><span class="detail-label">Remaining:</span><span class="detail-val mono">${span.deadline_remaining_ms ? (span.deadline_remaining_ms.toFixed(1) + ' ms') : '-'}</span></div>
        <div class="detail-row"><span class="detail-label">Wire Traffic:</span><span class="detail-val mono">${span.request_bytes || 0} bytes in / ${span.response_bytes || 0} bytes out</span></div>
        <div class="detail-row"><span class="detail-label">Error Info:</span><span class="detail-val mono" style="color:var(--stage-failed)">${span.error_code ? (`[${span.error_code}] ` + escapeHtml(span.error_message)) : 'None'}</span></div>
    `;

    modal.classList.add('open');
}

function closeModal() {
    document.getElementById('detailModal').classList.remove('open');
}

window.onclick = function(event) {
    const modal = document.getElementById('detailModal');
    if (event.target === modal) {
        closeModal();
    }
};

function showToast(msg) {
    const toast = document.getElementById('toast');
    toast.innerText = msg;
    toast.classList.add('show');
    setTimeout(() => {
        toast.classList.remove('show');
    }, 2500);
}

function escapeHtml(str) {
    if (!str) return '';
    return String(str)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#039;');
}
)JS";

// ─────────────────────────────────────────────────────────────────────────────
// RpcTraceRegistry Implementation
// ─────────────────────────────────────────────────────────────────────────────

auto RpcTraceRegistry::instance() -> RpcTraceRegistry& {
    static RpcTraceRegistry sInstance;
    return sInstance;
}

auto RpcTraceRegistry::registerSpan(std::string_view methodName, std::string requestId, std::size_t requestBytes,
                                    bool isNotification, const RpcPeerInfo* peer, const void* session,
                                    std::optional<std::chrono::steady_clock::time_point> deadline,
                                    std::optional<std::chrono::nanoseconds> timeout)
    -> std::shared_ptr<RpcCallTraceSpan> {
    auto span = std::make_shared<RpcCallTraceSpan>();
    span->requestId = std::move(requestId);
    span->methodName = std::string(methodName);
    span->stage = RpcCallStage::Received;
    span->session = session;
    span->receivedTime = std::chrono::system_clock::now();
    span->receivedAt = std::chrono::steady_clock::now();
    span->deadline = deadline;
    span->timeout = timeout;
    span->requestBytes = requestBytes;
    span->isNotification = isNotification;

    if (peer != nullptr) {
        span->peerId = peer->id;
        span->peerAttributes = peer->attributes;
    }

    std::scoped_lock lock(mMutex);
    span->traceId = mNextTraceId++;
    mActive[span->traceId] = span;
    return span;
}

void RpcTraceRegistry::markQueued(const std::shared_ptr<RpcCallTraceSpan>& span) {
    if (!span || isTerminalStage(span->stage)) return;
    std::scoped_lock lock(mMutex);
    if (isTerminalStage(span->stage)) return;
    span->stage = RpcCallStage::Queued;
    span->queuedAt = std::chrono::steady_clock::now();
}

void RpcTraceRegistry::markExecuting(const std::shared_ptr<RpcCallTraceSpan>& span) {
    if (!span || isTerminalStage(span->stage)) return;
    std::scoped_lock lock(mMutex);
    if (isTerminalStage(span->stage)) return;
    span->stage = RpcCallStage::Executing;
    span->startedAt = std::chrono::steady_clock::now();
}

void RpcTraceRegistry::markExecuted(const std::shared_ptr<RpcCallTraceSpan>& span, std::size_t responseBytes,
                                    std::error_code ec) {
    if (!span || isTerminalStage(span->stage)) return;
    std::scoped_lock lock(mMutex);
    if (isTerminalStage(span->stage)) return;
    span->stage = RpcCallStage::Executed;
    span->executedAt = std::chrono::steady_clock::now();
    if (responseBytes > 0) {
        span->responseBytes = responseBytes;
    }
    if (ec) {
        span->errorCode = ec.value();
        span->errorMessage = ec.message();
    }
}

void RpcTraceRegistry::markSending(const std::shared_ptr<RpcCallTraceSpan>& span, std::size_t responseBytes) {
    if (!span || isTerminalStage(span->stage)) return;
    std::scoped_lock lock(mMutex);
    if (isTerminalStage(span->stage)) return;
    span->stage = RpcCallStage::Sending;
    if (responseBytes > 0) {
        span->responseBytes = responseBytes;
    }
}

void RpcTraceRegistry::markCompleted(const std::shared_ptr<RpcCallTraceSpan>& span, std::size_t responseBytes) {
    if (!span || isTerminalStage(span->stage)) return;
    std::scoped_lock lock(mMutex);
    if (isTerminalStage(span->stage)) return;
    span->stage = RpcCallStage::Completed;
    span->completedAt = std::chrono::steady_clock::now();
    if (responseBytes > 0) {
        span->responseBytes = responseBytes;
    }
    mActive.erase(span->traceId);

    mRecentCompleted.push_front(span);
    while (mRecentCompleted.size() > mMaxHistory) {
        mRecentCompleted.pop_back();
    }
    ++mTotalCompleted;
}

void RpcTraceRegistry::markCanceled(const std::shared_ptr<RpcCallTraceSpan>& span) {
    if (!span || isTerminalStage(span->stage)) return;
    std::scoped_lock lock(mMutex);
    if (isTerminalStage(span->stage)) return;
    span->stage = RpcCallStage::Canceled;
    span->completedAt = std::chrono::steady_clock::now();
    mActive.erase(span->traceId);

    mRecentFailed.push_front(span);
    while (mRecentFailed.size() > mMaxHistory) {
        mRecentFailed.pop_back();
    }
    ++mTotalCanceled;
}

void RpcTraceRegistry::markTimedOut(const std::shared_ptr<RpcCallTraceSpan>& span) {
    if (!span || isTerminalStage(span->stage)) return;
    std::scoped_lock lock(mMutex);
    if (isTerminalStage(span->stage)) return;
    span->stage = RpcCallStage::TimedOut;
    span->completedAt = std::chrono::steady_clock::now();
    mActive.erase(span->traceId);

    mRecentFailed.push_front(span);
    while (mRecentFailed.size() > mMaxHistory) {
        mRecentFailed.pop_back();
    }
    ++mTotalTimedOut;
}

void RpcTraceRegistry::markRejected(const std::shared_ptr<RpcCallTraceSpan>& span) {
    if (!span || isTerminalStage(span->stage)) return;
    std::scoped_lock lock(mMutex);
    if (isTerminalStage(span->stage)) return;
    span->stage = RpcCallStage::Rejected;
    span->completedAt = std::chrono::steady_clock::now();
    mActive.erase(span->traceId);

    mRecentFailed.push_front(span);
    while (mRecentFailed.size() > mMaxHistory) {
        mRecentFailed.pop_back();
    }
    ++mTotalRejected;
}

void RpcTraceRegistry::markFailed(const std::shared_ptr<RpcCallTraceSpan>& span, std::error_code ec,
                                  std::string_view msg) {
    if (!span || isTerminalStage(span->stage)) return;
    std::scoped_lock lock(mMutex);
    if (isTerminalStage(span->stage)) return;
    span->stage = RpcCallStage::Failed;
    span->completedAt = std::chrono::steady_clock::now();
    span->errorCode = ec.value();
    span->errorMessage = msg.empty() ? ec.message() : std::string(msg);
    mActive.erase(span->traceId);

    mRecentFailed.push_front(span);
    while (mRecentFailed.size() > mMaxHistory) {
        mRecentFailed.pop_back();
    }
    ++mTotalFailed;
}

void RpcTraceRegistry::registerCancelHook(CancelHook hook) {
    std::scoped_lock lock(mMutex);
    mCancelHook = std::move(hook);
}

void RpcTraceRegistry::unregisterCancelHook() {
    std::scoped_lock lock(mMutex);
    mCancelHook = nullptr;
}

auto RpcTraceRegistry::requestCancel(std::string_view requestId, const void* session) -> bool {
    CancelHook hookCopy;
    {
        std::scoped_lock lock(mMutex);
        hookCopy = mCancelHook;
    }
    if (hookCopy) {
        return hookCopy(session, requestId);
    }
    return false;
}

auto RpcTraceRegistry::requestCancelByTraceId(std::uint64_t traceId) -> bool {
    std::string reqId;
    const void* session = nullptr;
    {
        std::scoped_lock lock(mMutex);
        auto it = mActive.find(traceId);
        if (it == mActive.end()) {
            return false;
        }
        reqId = it->second->requestId;
        session = it->second->session;
    }
    return requestCancel(reqId, session);
}

void RpcTraceRegistry::registerServerInfoProvider(ServerInfoProvider provider) {
    std::scoped_lock lock(mMutex);
    mServerInfoProvider = std::move(provider);
}

void RpcTraceRegistry::unregisterServerInfoProvider() {
    std::scoped_lock lock(mMutex);
    mServerInfoProvider = nullptr;
}

auto RpcTraceRegistry::serverInfoJson() -> std::string {
    ServerInfoProvider providerCopy;
    {
        std::scoped_lock lock(mMutex);
        providerCopy = mServerInfoProvider;
    }
    if (!providerCopy) {
        return "{\"server\":{\"backend\":\"Unknown\",\"methods_count\":0},\"methods\":[]}";
    }

    auto info = providerCopy();
    std::string json;
    json.reserve(4096);

    const auto now = std::chrono::system_clock::now();
    const auto uptimeSec = std::chrono::duration_cast<std::chrono::seconds>(now - info.startTime).count();

    json += "{\"server\":{";
    json += "\"backend\":\"" + escapeJson(info.backendName) + "\",";
    json += "\"max_concurrent\":" + std::to_string(info.maxConcurrent) + ',';
    json += "\"max_queue\":" + std::to_string(info.maxQueue) + ',';
    if (info.defaultTimeout.has_value()) {
        const auto ms = std::chrono::duration<double, std::milli>(*info.defaultTimeout).count();
        json += "\"default_timeout_ms\":" + std::to_string(ms) + ',';
    } else {
        json += "\"default_timeout_ms\":null,";
    }
    json += "\"start_time\":\"" + formatTimePoint(info.startTime) + "\",";
    json += "\"uptime_seconds\":" + std::to_string(uptimeSec) + ',';
    json += "\"methods_count\":" + std::to_string(info.methods.size());
    json += "},\"methods\":[";

    bool first = true;
    for (const auto& m : info.methods) {
        if (!first) json += ',';
        first = false;
        json += '{';
        json += "\"name\":\"" + escapeJson(m.name) + "\",";
        json += "\"signature\":\"" + escapeJson(m.signature) + "\",";
        json += "\"description\":\"" + escapeJson(m.description) + "\",";
        json += "\"version\":\"" + escapeJson(m.rpcVersion) + "\",";
        json += "\"is_notification\":" + std::string(m.isNotification ? "true" : "false") + ',';
        json += "\"is_bind\":" + std::string(m.isBind ? "true" : "false") + ',';
        json += "\"uses_context\":" + std::string(m.usesContext ? "true" : "false") + ',';
        json += "\"args\":[";
        bool firstArg = true;
        for (const auto& a : m.argNames) {
            if (!firstArg) json += ',';
            firstArg = false;
            json += "\"" + escapeJson(a) + "\"";
        }
        json += "]}";
    }
    json += "]}";
    return json;
}

auto RpcTraceRegistry::snapshotJson() -> std::string {
    std::scoped_lock lock(mMutex);
    std::string json;
    json.reserve(4096);

    std::size_t activeCount = 0;
    std::size_t queuedCount = 0;
    for (const auto& [_, span] : mActive) {
        if (span->stage == RpcCallStage::Queued) {
            ++queuedCount;
        } else if (span->stage == RpcCallStage::Executing || span->stage == RpcCallStage::Received ||
                   span->stage == RpcCallStage::Sending) {
            ++activeCount;
        }
    }

    json += "{\"metrics\":{";
    json += "\"active\":" + std::to_string(activeCount) + ',';
    json += "\"queued\":" + std::to_string(queuedCount) + ',';
    json += "\"completed\":" + std::to_string(mTotalCompleted) + ',';
    json += "\"timed_out\":" + std::to_string(mTotalTimedOut) + ',';
    json += "\"canceled\":" + std::to_string(mTotalCanceled) + ',';
    json += "\"rejected\":" + std::to_string(mTotalRejected) + ',';
    json += "\"failed\":" + std::to_string(mTotalFailed);
    json += "},";

    json += "\"active\":[";
    bool first = true;
    for (const auto& [_, span] : mActive) {
        if (!first) json += ',';
        first = false;
        serializeSpanJson(json, *span);
    }
    json += "],";

    json += "\"recent_completed\":[";
    first = true;
    for (const auto& span : mRecentCompleted) {
        if (!first) json += ',';
        first = false;
        serializeSpanJson(json, *span);
    }
    json += "],";

    json += "\"recent_failed\":[";
    first = true;
    for (const auto& span : mRecentFailed) {
        if (!first) json += ',';
        first = false;
        serializeSpanJson(json, *span);
    }
    json += "]}";

    return json;
}

void RpcTraceRegistry::clear() {
    std::scoped_lock lock(mMutex);
    mActive.clear();
    mRecentCompleted.clear();
    mRecentFailed.clear();
    mTotalCompleted = 0;
    mTotalTimedOut = 0;
    mTotalCanceled = 0;
    mTotalRejected = 0;
    mTotalFailed = 0;
    mNextTraceId = 1;
}

// ─────────────────────────────────────────────────────────────────────────────
// RpcTracingWebUi Implementation
// ─────────────────────────────────────────────────────────────────────────────

struct RpcTracingWebUi::Impl {
    std::string mBind;
    RpcTraceRegistry* mRegistry = nullptr;
    ilias::WaitHandle<void> mServeHandle;

    auto serve() -> ilias::Task<void>;
    auto handleConnection(ilias::BufStream<ilias::TcpStream> stream) -> ilias::Task<void>;
    auto handleRequest(ilias::BufStream<ilias::TcpStream>& stream, std::string_view method,
                       std::string_view path, std::string_view body) -> ilias::IoTask<void>;
    auto sendReply(ilias::BufStream<ilias::TcpStream>& stream, int status, std::string_view contentType,
                   std::span<const std::string_view> bodyChunks) -> ilias::IoTask<void>;
    auto sendReply(ilias::BufStream<ilias::TcpStream>& stream, int status, std::string_view contentType,
                   std::string_view body) -> ilias::IoTask<void>;
};

auto RpcTracingWebUi::Impl::serve() -> ilias::Task<void> {
    co_await ilias::this_coro::setName("RpcWebUiServer");
    auto listener = co_await ilias::TcpListener::bind(mBind);
    if (!listener) {
        std::fprintf(stderr, "[RpcTracingWebUi] Failed to bind %s: %s\n", mBind.c_str(),
                     listener.error().message().c_str());
        co_return;
    }
    if (auto ep = listener->localEndpoint()) {
        std::fprintf(stderr, "[RpcTracingWebUi] Dashboard running at http://%s\n", ep->toString().c_str());
    }

    co_await ilias::TaskScope::enter([&](ilias::TaskScope& scope) -> ilias::Task<void> {
        while (true) {
            auto accepted = co_await listener->accept();
            if (!accepted) {
                break;
            }
            auto& [stream, ep] = *accepted;
            scope.spawn(handleConnection(std::move(stream)));
        }
    });
}

auto RpcTracingWebUi::Impl::handleConnection(ilias::BufStream<ilias::TcpStream> stream) -> ilias::Task<void> {
    co_await ilias::this_coro::setName("RpcWebUiConnection");
    auto splitRequestLine = [](std::string_view line) -> std::tuple<std::string_view, std::string_view, std::string_view> {
        auto sp1 = line.find(' ');
        if (sp1 == std::string_view::npos) return {};
        auto sp2 = line.find(' ', sp1 + 1);
        if (sp2 == std::string_view::npos) return {};
        return {line.substr(0, sp1), line.substr(sp1 + 1, sp2 - sp1 - 1), line.substr(sp2 + 1)};
    };

    std::string line;
    std::string header;
    while (true) {
        line.clear();
        if (!co_await stream.readline(line, "\r\n")) {
            co_return;
        }
        if (line.ends_with("\r\n")) {
            line.resize(line.size() - 2);
        }
        auto [method, path, version] = splitRequestLine(line);
        if (method.empty()) {
            co_return;
        }

        std::size_t contentLength = 0;
        while (true) {
            header.clear();
            if (!co_await stream.readline(header, "\r\n")) {
                co_return;
            }
            if (header.starts_with("Content-Length:") || header.starts_with("content-length:")) {
                std::string_view valStr = std::string_view(header).substr(15);
                while (!valStr.empty() && (valStr.front() == ' ' || valStr.front() == '\t')) {
                    valStr.remove_prefix(1);
                }
                while (!valStr.empty() && (valStr.back() == '\r' || valStr.back() == '\n' || valStr.back() == ' ')) {
                    valStr.remove_suffix(1);
                }
                std::from_chars(valStr.data(), valStr.data() + valStr.size(), contentLength);
            }
            if (header == "\r\n") {
                break;
            }
        }

        std::string body;
        if (contentLength > 0 && contentLength < 1024 * 1024) {
            body.resize(contentLength);
            auto readRes = co_await stream.readAll(std::span<std::byte>{reinterpret_cast<std::byte*>(body.data()), contentLength});
            if (!readRes || readRes.value() != contentLength) {
                co_return;
            }
        }

        if (auto ret = co_await handleRequest(stream, method, path, body); !ret) {
            co_return;
        }
        if (auto ret = co_await stream.flush(); !ret) {
            co_return;
        }
    }
}

auto RpcTracingWebUi::Impl::handleRequest(ilias::BufStream<ilias::TcpStream>& stream, std::string_view method,
                                         std::string_view path, std::string_view body) -> ilias::IoTask<void> {
    if (auto q = path.find('?'); q != std::string_view::npos) {
        path = path.substr(0, q);
    }

    if (method == "GET") {
        if (path == "/" || path == "/index.html") {
            co_return co_await sendReply(stream, 200, "text/html; charset=utf-8", kIndexHtml);
        }
        if (path == "/styles.css") {
            std::string_view cssChunks[] = {kStylesCss1, kStylesCss2};
            co_return co_await sendReply(stream, 200, "text/css; charset=utf-8", std::span<const std::string_view>{cssChunks});
        }
        if (path == "/script.js") {
            std::string_view jsChunks[] = {kScriptJs1, kScriptJs2, kScriptJs3};
            co_return co_await sendReply(stream, 200, "application/javascript; charset=utf-8", std::span<const std::string_view>{jsChunks});
        }
        if (path == "/api/calls" || path == "/api/tasks") {
            auto reg = mRegistry ? mRegistry : &RpcTraceRegistry::instance();
            auto json = reg->snapshotJson();
            co_return co_await sendReply(stream, 200, "application/json; charset=utf-8", json);
        }
        if (path == "/api/server" || path == "/api/methods" || path == "/api/info") {
            auto reg = mRegistry ? mRegistry : &RpcTraceRegistry::instance();
            auto json = reg->serverInfoJson();
            co_return co_await sendReply(stream, 200, "application/json; charset=utf-8", json);
        }
        if (path == "/favicon.ico") {
            co_return co_await sendReply(stream, 204, "text/plain", "");
        }
    } else if (method == "POST") {
        if (path == "/api/cancel") {
            auto reg = mRegistry ? mRegistry : &RpcTraceRegistry::instance();
            bool ok = false;

            // Check trace_id in JSON body
            if (auto pos = body.find("\"trace_id\":"); pos != std::string_view::npos) {
                std::uint64_t tid = 0;
                auto numStart = pos + 11;
                while (numStart < body.size() && (body[numStart] == ' ' || body[numStart] == ':')) {
                    ++numStart;
                }
                std::from_chars(body.data() + numStart, body.data() + body.size(), tid);
                if (tid > 0) {
                    ok = reg->requestCancelByTraceId(tid);
                }
            }
            // Check request id
            if (!ok) {
                if (auto pos = body.find("\"id\":\""); pos != std::string_view::npos) {
                    auto start = pos + 6;
                    auto end = body.find('"', start);
                    if (end != std::string_view::npos) {
                        auto reqId = body.substr(start, end - start);
                        ok = reg->requestCancel(reqId);
                    }
                }
            }

            std::string resp = ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"Call not found or already finished\"}";
            co_return co_await sendReply(stream, 200, "application/json; charset=utf-8", resp);
        }
    }

    co_return co_await sendReply(stream, 404, "text/plain; charset=utf-8", "Not Found");
}

auto RpcTracingWebUi::Impl::sendReply(ilias::BufStream<ilias::TcpStream>& stream, int status,
                                      std::string_view contentType,
                                      std::span<const std::string_view> bodyChunks) -> ilias::IoTask<void> {
    const auto statusText = [](int code) -> std::string_view {
        switch (code) {
        case 200: return "OK";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        default:  return "Unknown";
        }
    };

    std::size_t totalLen = 0;
    for (const auto& chunk : bodyChunks) {
        totalLen += chunk.size();
    }

    std::string header;
    header.reserve(256);
    header += "HTTP/1.1 " + std::to_string(status) + " " + std::string(statusText(status)) + "\r\n";
    header += "Content-Type: " + std::string(contentType) + "\r\n";
    header += "Content-Length: " + std::to_string(totalLen) + "\r\n";
    header += "Cache-Control: no-store\r\n";
    header += "Connection: keep-alive\r\n";
    header += "Access-Control-Allow-Origin: *\r\n";
    header += "\r\n";

    ILIAS_CO_TRYV(co_await stream.writeAll(std::span<const std::byte>{reinterpret_cast<const std::byte*>(header.data()), header.size()}));
    for (const auto& chunk : bodyChunks) {
        if (!chunk.empty()) {
            ILIAS_CO_TRYV(co_await stream.writeAll(std::span<const std::byte>{reinterpret_cast<const std::byte*>(chunk.data()), chunk.size()}));
        }
    }
    co_return {};
}

auto RpcTracingWebUi::Impl::sendReply(ilias::BufStream<ilias::TcpStream>& stream, int status,
                                      std::string_view contentType, std::string_view body) -> ilias::IoTask<void> {
    std::string_view chunks[] = {body};
    co_return co_await sendReply(stream, status, contentType, std::span<const std::string_view>{chunks});
}

// ─────────────────────────────────────────────────────────────────────────────
// RpcTracingWebUi Public Members
// ─────────────────────────────────────────────────────────────────────────────

RpcTracingWebUi::RpcTracingWebUi(std::string_view bind) : d(std::make_unique<Impl>()) {
    if (const auto env = std::getenv("NEKO_RPC_TRACE_WEBUI_BIND"); env != nullptr) {
        d->mBind.assign(env);
    } else {
        d->mBind.assign(bind);
    }
}

RpcTracingWebUi::~RpcTracingWebUi() {
    if (d && d->mServeHandle) {
        d->mServeHandle.stop();
        d->mServeHandle.wait();
    }
}

RpcTracingWebUi::RpcTracingWebUi(RpcTracingWebUi&&) noexcept = default;
auto RpcTracingWebUi::operator=(RpcTracingWebUi&&) noexcept -> RpcTracingWebUi& = default;

auto RpcTracingWebUi::install(RpcTraceRegistry& registry) -> bool {
    if (!d) return false;
    d->mRegistry = &registry;
    if (!d->mServeHandle) {
        d->mServeHandle = ilias::spawn(d->serve());
    }
    return true;
}

auto RpcTracingWebUi::endpoint() const noexcept -> std::string_view {
    return d ? d->mBind : std::string_view{};
}

} // namespace nekoproto

#endif // defined(NEKO_PROTO_RPC_TRACE)

