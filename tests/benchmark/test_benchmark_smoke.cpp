// ============================================================================
//  Aurora Motion Player — benchmark smoke test (Session 10)
//  Compiles standalone (no Qt, no Vulkan) to verify headers + basic function
// ============================================================================

#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>

// We include the headers to verify they compile
#include "core/benchmark/BenchmarkSystem.h"
#include "core/gpu_monitor/GPUMonitor.h"
#include "core/gpu_monitor/CPUMonitor.h"

using namespace aurora::benchmark;
using namespace std::chrono_literals;

static int passed = 0;
static int failed = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("  ✓ %s\n", msg); ++passed; } \
    else       { printf("  ✗ FAIL: %s\n", msg); ++failed; } \
} while (0)

// ── CPUMonitor tests ─────────────────────────────────────────────────────────
void test_cpu_monitor() {
    printf("\n[CPUMonitor]\n");

    CPUMonitor cpu;
    CHECK(cpu.init(), "init() returns true");

    // First sample primes delta state
    auto s0 = cpu.sample();
    CHECK(s0.totalUsagePct >= 0.0 && s0.totalUsagePct <= 100.0,
          "first sample in [0,100]");

    // Wait a tick so deltas accumulate
    std::this_thread::sleep_for(200ms);
    auto s1 = cpu.sample();
    CHECK(s1.totalUsagePct >= 0.0 && s1.totalUsagePct <= 100.0,
          "second sample in [0,100]");

    printf("    CPU usage: %.1f%%", s1.totalUsagePct);
    if (s1.temperatureC > 0) printf(" | Temp: %.1f°C", s1.temperatureC);
    printf("\n");

    // Test polling
    int callbackCount = 0;
    cpu.startPolling(100, [&](const CPUSample&) { ++callbackCount; });
    std::this_thread::sleep_for(350ms);
    cpu.stopPolling();
    CHECK(callbackCount >= 2, "polling callback fires at least 2x in 350ms");

    cpu.shutdown();
    CHECK(true, "shutdown() called without crash");
}

// ── GPUMonitor tests ─────────────────────────────────────────────────────────
void test_gpu_monitor() {
    printf("\n[GPUMonitor]\n");

    GPUMonitor gpu;
    // On CI (no GPU), init() may return false — that's OK
    bool hasGPU = gpu.init();
    printf("    GPU backend available: %s\n", hasGPU ? "yes" : "no (CI stub)");
    CHECK(true, "init() does not crash");

    auto s = gpu.sample();
    CHECK(s.gpuUsagePct >= 0.0 && s.gpuUsagePct <= 100.0,
          "gpuUsagePct in [0,100]");
    CHECK(s.vramUsedMB <= s.vramTotalMB || s.vramTotalMB == 0,
          "vramUsed <= vramTotal");

    printf("    GPU: %.1f%% | VRAM: %zu/%zu MB | Temp: %.1f°C\n",
           s.gpuUsagePct, s.vramUsedMB, s.vramTotalMB, s.temperatureC);

    // Polling test (stubs should still fire)
    int cbCount = 0;
    gpu.startPolling(100, [&](const GPUSample&) { ++cbCount; });
    std::this_thread::sleep_for(350ms);
    gpu.stopPolling();
    CHECK(cbCount >= 2, "polling fires >= 2 callbacks in 350ms");

    CHECK(GPUMonitor::formatSample(s).size() > 5, "formatSample returns non-empty string");
    gpu.shutdown();
    CHECK(true, "shutdown() without crash");
}

// ── BenchmarkSystem tests ─────────────────────────────────────────────────────
void test_benchmark_system() {
    printf("\n[BenchmarkSystem]\n");

    BenchmarkSystem bench;
    bench.start();
    CHECK(true, "start() without crash");

    // Simulate frame events
    for (int i = 0; i < 30; ++i) {
        bench.onRenderBegin();
        std::this_thread::sleep_for(2ms);
        bench.onRenderEnd();
        bench.onFrameRendered();
        bench.onFrameDecoded();
        if (i % 3 == 0) bench.onFrameInterpolated();
        if (i == 15) bench.onFrameDropped();
    }

    auto snap = bench.snapshot();
    CHECK(snap.droppedFrames == 1, "droppedFrames == 1");
    CHECK(snap.renderFPS > 0.0, "renderFPS > 0");
    CHECK(snap.avgFrameMs >= 0.0, "avgFrameMs >= 0");
    CHECK(snap.frameVarianceMs >= 0.0, "frameVarianceMs >= 0");

    printf("    renderFPS=%.1f | decodeFPS=%.1f | interpFPS=%.1f\n",
           snap.renderFPS, snap.decodeFPS, snap.interpolateFPS);
    printf("    avgFrameMs=%.2f ± %.2f\n", snap.avgFrameMs, snap.frameVarianceMs);
    printf("    CPU=%.1f%% | GPU=%.1f%%\n", snap.cpuUsage, snap.gpuUsage);

    // Callback test
    int cbCount = 0;
    bench.setCallback([&](const BenchmarkSnapshot&) { ++cbCount; }, 100);
    std::this_thread::sleep_for(350ms);
    CHECK(cbCount >= 2, "setCallback fires >= 2 in 350ms");

    auto fmtStr = BenchmarkSystem::format(snap);
    CHECK(fmtStr.find("fps") != std::string::npos, "format() contains 'fps'");
    CHECK(fmtStr.find("CPU") != std::string::npos, "format() contains 'CPU'");
    CHECK(fmtStr.find("GPU") != std::string::npos, "format() contains 'GPU'");
    printf("    format: %s\n", fmtStr.c_str());

    bench.stop();
    CHECK(true, "stop() without crash");
}

// ── BenchmarkSnapshot format test ────────────────────────────────────────────
void test_snapshot_format() {
    printf("\n[BenchmarkSnapshot format]\n");

    BenchmarkSnapshot s;
    s.renderFPS      = 60.0;
    s.decodeFPS      = 60.0;
    s.interpolateFPS = 120.0;
    s.droppedFrames  = 3;
    s.cpuUsage       = 45.2;
    s.gpuUsage       = 72.8;
    s.vramUsedMB     = 2048;
    s.vramTotalMB    = 4096;
    s.avgFrameMs     = 16.67;
    s.frameVarianceMs = 0.5;
    s.gpuTemperatureC = 75.0;
    s.gpuPowerWatts  = 85.5;
    s.gpuClockMHz    = 1875.0;
    s.gpuName        = "GeForce RTX 3080";
    s.gpuVendor      = "NVIDIA";

    std::string fmt = BenchmarkSystem::format(s);
    CHECK(!fmt.empty(), "format() non-empty for filled snapshot");
    CHECK(fmt.find("60.0") != std::string::npos, "renderFPS 60.0 in format");
    CHECK(fmt.find("GeForce RTX 3080") != std::string::npos, "GPU name in format");
    printf("    %s\n", fmt.c_str());
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main() {
    printf("══════════════════════════════════════════════════\n");
    printf("  Aurora Motion Player — Session 10 Smoke Tests  \n");
    printf("══════════════════════════════════════════════════\n");

    test_cpu_monitor();
    test_gpu_monitor();
    test_benchmark_system();
    test_snapshot_format();

    printf("\n══════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed\n", passed, failed);
    printf("══════════════════════════════════════════════════\n");

    return failed == 0 ? 0 : 1;
}
