// ============================================================================
//  Aurora Motion Player — GPUMonitor.cpp
//  Session 10: GPU Benchmark System
// ============================================================================

#include "GPUMonitor.h"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <winternl.h>   // NTSTATUS
// D3DKMT types (available without d3d12.h)
typedef UINT D3DKMT_HANDLE;
typedef LONGLONG D3DKMT_ALIGN64;
struct D3DKMT_OPENADAPTERFROMLUID {
    LUID  AdapterLuid;
    D3DKMT_HANDLE hAdapter;
};
struct D3DKMT_QUERYADAPTERINFO {
    D3DKMT_HANDLE hAdapter;
    UINT          Type;
    VOID*         pPrivateDriverData;
    UINT          PrivateDriverDataSize;
};
struct D3DKMT_SEGMENTSIZEINFO {
    ULONGLONG DedicatedVideoMemorySize;
    ULONGLONG DedicatedSystemMemorySize;
    ULONGLONG SharedSystemMemorySize;
};
enum KMTQUERYADAPTERINFOTYPE { KMTQAITYPE_GETSEGMENTSIZE = 3 };
// Use LONG (= NTSTATUS compatible) to avoid winternl.h conflicts
typedef LONG (WINAPI* PFN_D3DKMTOpenAdapterFromLuid)(D3DKMT_OPENADAPTERFROMLUID*);
typedef LONG (WINAPI* PFN_D3DKMTQueryAdapterInfo)(D3DKMT_QUERYADAPTERINFO*);

// DXGI for adapter enumeration
#  include <dxgi1_4.h>
#  pragma comment(lib, "dxgi.lib")

// PDH for per-GPU usage counter (available on Windows 10+)
#  include <pdh.h>
#  pragma comment(lib, "pdh.lib")
#endif

#ifdef __ANDROID__
#  include <sys/stat.h>
#endif

// ── NVML dynamic binding stubs ───────────────────────────────────────────────
// We dlopen/LoadLibrary nvml.dll at runtime so the binary works without CUDA.
#ifdef _WIN32
#  define NVML_LIB "nvml.dll"
typedef void* nvmlDevice_t;
typedef int   nvmlReturn_t;
struct nvmlUtilization_st { unsigned int gpu; unsigned int memory; };
typedef struct nvmlUtilization_st nvmlUtilization_t;
struct nvmlMemory_st { unsigned long long total; unsigned long long free; unsigned long long used; };
typedef struct nvmlMemory_st nvmlMemory_t;
#elif defined(__linux__)
#  define NVML_LIB "libnvidia-ml.so.1"
typedef void* nvmlDevice_t;
typedef int   nvmlReturn_t;
struct nvmlUtilization_st { unsigned int gpu; unsigned int memory; };
typedef struct nvmlUtilization_st nvmlUtilization_t;
struct nvmlMemory_st { unsigned long long total; unsigned long long free; unsigned long long used; };
typedef struct nvmlMemory_st nvmlMemory_t;
#endif

#if defined(_WIN32) || defined(__linux__)
// Function pointer typedefs
typedef nvmlReturn_t (*PFN_nvmlInit)();
typedef nvmlReturn_t (*PFN_nvmlShutdown)();
typedef nvmlReturn_t (*PFN_nvmlDeviceGetHandleByIndex)(unsigned int, nvmlDevice_t*);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetUtilizationRates)(nvmlDevice_t, nvmlUtilization_t*);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetMemoryInfo)(nvmlDevice_t, nvmlMemory_t*);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetTemperature)(nvmlDevice_t, unsigned int, unsigned int*);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetPowerUsage)(nvmlDevice_t, unsigned int*);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetClockInfo)(nvmlDevice_t, unsigned int, unsigned int*);
typedef nvmlReturn_t (*PFN_nvmlDeviceFanGetSpeed)(nvmlDevice_t, unsigned int, unsigned int*);
typedef nvmlReturn_t (*PFN_nvmlDeviceGetName)(nvmlDevice_t, char*, unsigned int);

struct NVMLFuncs {
    PFN_nvmlInit                    init                = nullptr;
    PFN_nvmlShutdown                shutdown            = nullptr;
    PFN_nvmlDeviceGetHandleByIndex  getHandle           = nullptr;
    PFN_nvmlDeviceGetUtilizationRates getUtil           = nullptr;
    PFN_nvmlDeviceGetMemoryInfo     getMem              = nullptr;
    PFN_nvmlDeviceGetTemperature    getTemp             = nullptr;
    PFN_nvmlDeviceGetPowerUsage     getPower            = nullptr;
    PFN_nvmlDeviceGetClockInfo      getClock            = nullptr;
    PFN_nvmlDeviceFanGetSpeed       getFan              = nullptr;
    PFN_nvmlDeviceGetName           getName             = nullptr;
};

static NVMLFuncs   g_nvml;
static void*       g_nvmlLib = nullptr;

#ifdef _WIN32
static void* loadSym(HMODULE h, const char* name) {
    return (void*)GetProcAddress(h, name);
}
#else
#  include <dlfcn.h>
static void* loadSym(void* h, const char* name) {
    return dlsym(h, name);
}
#endif

static bool loadNVML() {
#ifdef _WIN32
    HMODULE h = LoadLibraryA(NVML_LIB);
    if (!h) return false;
    g_nvmlLib = (void*)h;
#define SYM(fn, name) g_nvml.fn = (decltype(g_nvml.fn))loadSym(h, name)
#else
    void* h = dlopen(NVML_LIB, RTLD_LAZY);
    if (!h) return false;
    g_nvmlLib = h;
#define SYM(fn, name) g_nvml.fn = (decltype(g_nvml.fn))loadSym(h, name)
#endif
    SYM(init,      "nvmlInit_v2");
    SYM(shutdown,  "nvmlShutdown");
    SYM(getHandle, "nvmlDeviceGetHandleByIndex");
    SYM(getUtil,   "nvmlDeviceGetUtilizationRates");
    SYM(getMem,    "nvmlDeviceGetMemoryInfo");
    SYM(getTemp,   "nvmlDeviceGetTemperature");
    SYM(getPower,  "nvmlDeviceGetPowerUsage");
    SYM(getClock,  "nvmlDeviceGetClockInfo");
    SYM(getFan,    "nvmlDeviceFanGetSpeed");
    SYM(getName,   "nvmlDeviceGetName");
#undef SYM
    if (!g_nvml.init || !g_nvml.getUtil || !g_nvml.getMem) {
#ifdef _WIN32
        FreeLibrary((HMODULE)g_nvmlLib);
#else
        dlclose(g_nvmlLib);
#endif
        g_nvmlLib = nullptr;
        return false;
    }
    return (g_nvml.init() == 0); // NVML_SUCCESS == 0
}
#endif // _WIN32 || __linux__

namespace aurora::benchmark {

// ── Constructor / Destructor ─────────────────────────────────────────────────
GPUMonitor::GPUMonitor()  = default;
GPUMonitor::~GPUMonitor() { shutdown(); }

// ── init ─────────────────────────────────────────────────────────────────────
bool GPUMonitor::init() {
    // Try backends in priority order
#if defined(_WIN32)
    if (initNVML())    { m_backend = Backend::NVML;    return true; }
    if (initD3DKMT())  { m_backend = Backend::D3DKMT;  return true; }
#elif defined(__ANDROID__)
    if (initAndroidKgsl()) { m_backend = Backend::AndroidKgsl; return true; }
    if (initAndroidMali()) { m_backend = Backend::AndroidMali; return true; }
#elif defined(__linux__)
    if (initNVML())    { m_backend = Backend::NVML;    return true; }
    if (initSysfs())   { m_backend = Backend::Sysfs;   return true; }
#endif
    return false;
}

// ── shutdown ─────────────────────────────────────────────────────────────────
void GPUMonitor::shutdown() {
    stopPolling();
#if defined(_WIN32) || defined(__linux__)
    if (m_nvmlLoaded && g_nvml.shutdown) {
        g_nvml.shutdown();
        m_nvmlLoaded = false;
    }
    if (g_nvmlLib) {
#ifdef _WIN32
        FreeLibrary((HMODULE)g_nvmlLib);
#else
        dlclose(g_nvmlLib);
#endif
        g_nvmlLib = nullptr;
    }
#endif
#ifdef _WIN32
    // Close D3DKMT adapter handle
    if (m_d3dAdapter) {
        // D3DKMTCloseAdapter — load dynamically
        HMODULE gdi = GetModuleHandleA("gdi32.dll");
        if (gdi) {
            typedef NTSTATUS (WINAPI* PFN_Close)(const D3DKMT_HANDLE*);
            auto fn = (PFN_Close)GetProcAddress(gdi, "D3DKMTCloseAdapter");
            if (fn) {
                auto h = (D3DKMT_HANDLE)(uintptr_t)m_d3dAdapter;
                fn(&h);
            }
        }
        m_d3dAdapter = nullptr;
    }
#endif
    m_backend = Backend::None;
}

// ── sample ───────────────────────────────────────────────────────────────────
GPUSample GPUMonitor::sample() const {
    switch (m_backend) {
#if defined(_WIN32) || defined(__linux__)
        case Backend::NVML:         return sampleNVML();
#endif
#ifdef _WIN32
        case Backend::D3DKMT:       return sampleD3DKMT();
#endif
#ifdef __ANDROID__
        case Backend::AndroidKgsl:  return sampleAndroidKgsl();
        case Backend::AndroidMali:  return sampleAndroidMali();
#endif
#ifdef __linux__
        case Backend::Sysfs:        return sampleSysfs();
#endif
        default:                    return {};
    }
}

GPUInfo GPUMonitor::info() const { return m_info; }

// ── Polling thread ───────────────────────────────────────────────────────────
void GPUMonitor::startPolling(int intervalMs, SampleCallback cb) {
    if (m_polling.exchange(true)) return; // already running
    m_pollThread = std::thread(&GPUMonitor::pollLoop, this, intervalMs, std::move(cb));
}

void GPUMonitor::stopPolling() {
    m_polling.store(false);
    if (m_pollThread.joinable()) m_pollThread.join();
}

GPUSample GPUMonitor::latestSample() const {
    std::lock_guard lock(m_sampleMutex);
    return m_latestSample;
}

void GPUMonitor::pollLoop(int intervalMs, SampleCallback cb) {
    while (m_polling.load()) {
        GPUSample s = sample();
        {
            std::lock_guard lock(m_sampleMutex);
            m_latestSample = s;
        }
        if (cb) cb(s);
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
    }
}

// ── Format ───────────────────────────────────────────────────────────────────
std::string GPUMonitor::formatSample(const GPUSample& s) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    oss << "GPU: "   << s.gpuUsagePct  << "% | ";
    oss << "VRAM: "  << s.vramUsedMB   << "/" << s.vramTotalMB << " MB | ";
    oss << "Temp: "  << s.temperatureC << "°C | ";
    oss << "Power: " << s.powerWatts   << " W | ";
    oss << "Clk: "   << s.clockMHz     << " MHz";
    return oss.str();
}

// ── Helpers: read sysfs file ──────────────────────────────────────────────────
static std::string readSysfsStr(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};
    std::string s;
    std::getline(f, s);
    return s;
}
static long long readSysfsLL(const std::string& path, long long def = 0) {
    auto s = readSysfsStr(path);
    if (s.empty()) return def;
    try { return std::stoll(s); } catch (...) { return def; }
}
static bool fileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

// ─────────────────────────────────────────────────────────────────────────────
//  NVML backend
// ─────────────────────────────────────────────────────────────────────────────
#if defined(_WIN32) || defined(__linux__)
bool GPUMonitor::initNVML() {
    if (!loadNVML()) return false;
    m_nvmlLoaded = true;

    nvmlDevice_t dev = nullptr;
    if (!g_nvml.getHandle || g_nvml.getHandle(0, &dev) != 0) return false;
    m_nvmlDevice = (void*)dev;

    // Fill info
    char nameBuf[256] = {};
    if (g_nvml.getName) g_nvml.getName(dev, nameBuf, sizeof(nameBuf));
    m_info.name   = nameBuf;
    m_info.vendor = "NVIDIA";
    m_info.nvmlAvailable = true;

    if (g_nvml.getMem) {
        nvmlMemory_t mem{};
        g_nvml.getMem(dev, &mem);
        m_info.vramTotalMB = static_cast<size_t>(mem.total / (1024*1024));
    }
    return true;
}

GPUSample GPUMonitor::sampleNVML() const {
    GPUSample s;
    auto dev = (nvmlDevice_t)m_nvmlDevice;

    if (g_nvml.getUtil) {
        nvmlUtilization_t u{};
        if (g_nvml.getUtil(dev, &u) == 0) {
            s.gpuUsagePct = u.gpu;
            s.memUsagePct = u.memory;
        }
    }
    if (g_nvml.getMem) {
        nvmlMemory_t m{};
        if (g_nvml.getMem(dev, &m) == 0) {
            s.vramUsedMB  = static_cast<size_t>(m.used  / (1024*1024));
            s.vramTotalMB = static_cast<size_t>(m.total / (1024*1024));
        }
    }
    if (g_nvml.getTemp) {
        unsigned int t = 0;
        if (g_nvml.getTemp(dev, 0 /*TEMPERATURE_GPU*/, &t) == 0) s.temperatureC = t;
    }
    if (g_nvml.getPower) {
        unsigned int mW = 0;
        if (g_nvml.getPower(dev, &mW) == 0) s.powerWatts = mW / 1000.0;
    }
    if (g_nvml.getClock) {
        unsigned int clk = 0;
        if (g_nvml.getClock(dev, 0 /*CLOCK_GRAPHICS*/, &clk) == 0) s.clockMHz = clk;
    }
    if (g_nvml.getFan) {
        unsigned int rpm = 0;
        if (g_nvml.getFan(dev, 0, &rpm) == 0) s.fanRPM = rpm;
    }
    return s;
}
#else
bool GPUMonitor::initNVML()        { return false; }
GPUSample GPUMonitor::sampleNVML() const { return {}; }
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  D3DKMT backend (Windows — all GPU vendors via DXGI + kernel driver query)
// ─────────────────────────────────────────────────────────────────────────────
#ifdef _WIN32
bool GPUMonitor::initD3DKMT() {
    // Enumerate DXGI adapters to get name/VRAM
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory)))
        return false;

    IDXGIAdapter* adapter = nullptr;
    if (FAILED(factory->EnumAdapters(0, &adapter))) {
        factory->Release();
        return false;
    }
    DXGI_ADAPTER_DESC desc{};
    adapter->GetDesc(&desc);
    adapter->Release();
    factory->Release();

    // Convert name
    char nameBuf[256] = {};
    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, nameBuf, sizeof(nameBuf), nullptr, nullptr);
    m_info.name        = nameBuf;
    m_info.vendorId    = desc.VendorId;
    m_info.deviceId    = desc.DeviceId;
    m_info.vramTotalMB = static_cast<size_t>(desc.DedicatedVideoMemory / (1024*1024));

    switch (desc.VendorId) {
        case 0x10DE: m_info.vendor = "NVIDIA"; break;
        case 0x1002: m_info.vendor = "AMD";    break;
        case 0x8086: m_info.vendor = "Intel";  break;
        default:     m_info.vendor = "Unknown";
    }

    // Open D3DKMT adapter from LUID
    HMODULE gdi = GetModuleHandleA("gdi32.dll");
    if (!gdi) return false;
    auto openFn = (PFN_D3DKMTOpenAdapterFromLuid)GetProcAddress(gdi, "D3DKMTOpenAdapterFromLuid");
    if (!openFn) return false;

    m_d3dLuid = (uint64_t(desc.AdapterLuid.HighPart) << 32) | uint32_t(desc.AdapterLuid.LowPart);

    D3DKMT_OPENADAPTERFROMLUID oa{};
    oa.AdapterLuid = desc.AdapterLuid;
    if (openFn(&oa) != 0) return false;
    m_d3dAdapter = (void*)(uintptr_t)oa.hAdapter;
    m_info.d3dkmtAvailable = true;
    return true;
}

GPUSample GPUMonitor::sampleD3DKMT() const {
    GPUSample s;
    s.vramTotalMB = m_info.vramTotalMB;

    // Use PDH to query GPU engine utilisation counter
    // "\\GPU Engine(*_0_*engtype_3D)\\Utilization Percentage"
    static PDH_HQUERY   hQuery   = nullptr;
    static PDH_HCOUNTER hCounter = nullptr;
    static bool         pdhInit  = false;

    if (!pdhInit) {
        pdhInit = true;
        if (PdhOpenQuery(nullptr, 0, &hQuery) == ERROR_SUCCESS) {
            PdhAddEnglishCounterA(hQuery,
                "\\GPU Engine(*)\\Utilization Percentage",
                0, &hCounter);
            PdhCollectQueryData(hQuery);
        }
    }
    if (hQuery && hCounter) {
        PdhCollectQueryData(hQuery);
        PDH_FMT_COUNTERVALUE val{};
        if (PdhGetFormattedCounterValue(hCounter, PDH_FMT_DOUBLE, nullptr, &val) == ERROR_SUCCESS)
            s.gpuUsagePct = val.doubleValue;
    }

    // VRAM used via DXGI memory info
    IDXGIFactory4* factory4 = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory4), (void**)&factory4))) {
        IDXGIAdapter3* adapter3 = nullptr;
        IDXGIAdapter*  adp = nullptr;
        factory4->EnumAdapters(0, &adp);
        if (adp) {
            if (SUCCEEDED(adp->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&adapter3))) {
                DXGI_QUERY_VIDEO_MEMORY_INFO memInfo{};
                if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo))) {
                    s.vramUsedMB  = static_cast<size_t>(memInfo.CurrentUsage / (1024*1024));
                    s.vramTotalMB = static_cast<size_t>(memInfo.Budget       / (1024*1024));
                    s.memUsagePct = s.vramTotalMB > 0
                        ? 100.0 * double(s.vramUsedMB) / double(s.vramTotalMB)
                        : 0.0;
                }
                adapter3->Release();
            }
            adp->Release();
        }
        factory4->Release();
    }
    return s;
}
#else
bool GPUMonitor::initD3DKMT()          { return false; }
GPUSample GPUMonitor::sampleD3DKMT()  const { return {}; }
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Android Adreno kgsl sysfs backend
// ─────────────────────────────────────────────────────────────────────────────
#ifdef __ANDROID__
bool GPUMonitor::initAndroidKgsl() {
    // Try common kgsl paths (Adreno / Qualcomm)
    const char* busyPaths[] = {
        "/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage",
        "/sys/kernel/gpu/gpu_busy",
        "/sys/devices/soc/kgsl-3d0/gpu_busy_percentage",
    };
    for (auto p : busyPaths) {
        if (fileExists(p)) { m_kgslBusyPath = p; break; }
    }
    if (m_kgslBusyPath.empty()) return false;

    const char* freqPaths[] = {
        "/sys/class/kgsl/kgsl-3d0/gpuclk",
        "/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq",
        "/sys/kernel/gpu/gpu_clock",
    };
    for (auto p : freqPaths) {
        if (fileExists(p)) { m_kgslFreqPath = p; break; }
    }

    const char* memPaths[] = {
        "/sys/class/kgsl/kgsl-3d0/kgsl_memstats",
        "/sys/kernel/gpu/gpu_memtotal",
    };
    for (auto p : memPaths) {
        if (fileExists(p)) { m_kgslMemPath = p; break; }
    }

    m_info.name   = "Adreno GPU";
    m_info.vendor = "Qualcomm";
    return true;
}

GPUSample GPUMonitor::sampleAndroidKgsl() const {
    GPUSample s;
    // GPU busy %
    auto busy = readSysfsStr(m_kgslBusyPath);
    // Format: "45 %" or "45"
    if (!busy.empty()) {
        try { s.gpuUsagePct = std::stod(busy); } catch (...) {}
    }
    // Clock
    if (!m_kgslFreqPath.empty()) {
        long long hz = readSysfsLL(m_kgslFreqPath);
        s.clockMHz = hz / 1e6;
    }
    // Memory — read /proc/meminfo for approximation on Android
    {
        std::ifstream mi("/proc/meminfo");
        std::string line;
        long long total = 0, avail = 0;
        while (std::getline(mi, line)) {
            if (line.rfind("MemTotal:", 0) == 0)
                total = std::stoll(line.substr(9)) / 1024;
            if (line.rfind("MemAvailable:", 0) == 0)
                avail = std::stoll(line.substr(13)) / 1024;
        }
        // Rough: GPU shares system RAM on mobile
        s.vramTotalMB = static_cast<size_t>(total);
        s.vramUsedMB  = static_cast<size_t>(total - avail);
    }
    return s;
}

bool GPUMonitor::initAndroidMali() {
    const char* loadPaths[] = {
        "/sys/bus/platform/drivers/mali/gpu/utilization",
        "/sys/class/misc/mali0/device/utilization",
        "/sys/devices/platform/mali/utilization",
    };
    for (auto p : loadPaths) {
        if (fileExists(p)) { m_maliLoadPath = p; break; }
    }
    if (m_maliLoadPath.empty()) return false;

    const char* freqPaths[] = {
        "/sys/class/misc/mali0/device/clock",
        "/sys/devices/platform/mali/devfreq/cur_freq",
    };
    for (auto p : freqPaths) {
        if (fileExists(p)) { m_maliFreqPath = p; break; }
    }

    m_info.name   = "Mali GPU";
    m_info.vendor = "ARM";
    return true;
}

GPUSample GPUMonitor::sampleAndroidMali() const {
    GPUSample s;
    long long util = readSysfsLL(m_maliLoadPath);
    s.gpuUsagePct = static_cast<double>(util);
    if (!m_maliFreqPath.empty()) {
        long long hz = readSysfsLL(m_maliFreqPath);
        s.clockMHz = hz / 1e6;
    }
    return s;
}
#else
bool GPUMonitor::initAndroidKgsl()            { return false; }
bool GPUMonitor::initAndroidMali()            { return false; }
GPUSample GPUMonitor::sampleAndroidKgsl() const { return {}; }
GPUSample GPUMonitor::sampleAndroidMali() const { return {}; }
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Linux sysfs (AMD/Intel via /sys/class/drm)
// ─────────────────────────────────────────────────────────────────────────────
#if defined(__linux__) && !defined(__ANDROID__)
bool GPUMonitor::initSysfs() {
    // Check common DRM paths
    const char* busyPaths[] = {
        "/sys/class/drm/card0/device/gpu_busy_percent",  // AMD
        "/sys/class/drm/renderD128/device/gpu_busy_percent",
    };
    for (auto p : busyPaths) {
        if (fileExists(p)) { m_drmCardPath = p; break; }
    }
    if (m_drmCardPath.empty()) return false;
    m_info.vendor = "AMD";

    // Detect vendor
    auto vendorPath = std::string(
        m_drmCardPath.substr(0, m_drmCardPath.rfind('/') + 1)) + "vendor";
    long long vendorId = readSysfsLL(vendorPath);
    if (vendorId == 0x8086) m_info.vendor = "Intel";

    // VRAM
    std::string base = m_drmCardPath.substr(0, m_drmCardPath.rfind('/') + 1);
    long long vram = readSysfsLL(base + "mem_info_vram_total");
    m_info.vramTotalMB = static_cast<size_t>(vram / (1024*1024));

    return true;
}

GPUSample GPUMonitor::sampleSysfs() const {
    GPUSample s;
    s.gpuUsagePct = static_cast<double>(readSysfsLL(m_drmCardPath));
    s.vramTotalMB = m_info.vramTotalMB;

    std::string base = m_drmCardPath.substr(0, m_drmCardPath.rfind('/') + 1);
    long long used = readSysfsLL(base + "mem_info_vram_used");
    s.vramUsedMB = static_cast<size_t>(used / (1024*1024));
    if (s.vramTotalMB > 0)
        s.memUsagePct = 100.0 * double(s.vramUsedMB) / double(s.vramTotalMB);

    // Temperature
    long long temp = readSysfsLL(base + "hwmon/hwmon0/temp1_input");
    s.temperatureC = temp / 1000.0;

    // Clock
    long long clk = readSysfsLL(base + "pp_dpm_sclk");
    s.clockMHz = clk > 0 ? clk : readSysfsLL(base + "cur_sclk") / 1e6;

    return s;
}
#else
bool GPUMonitor::initSysfs()          { return false; }
GPUSample GPUMonitor::sampleSysfs()  const { return {}; }
#endif

} // namespace aurora::benchmark
