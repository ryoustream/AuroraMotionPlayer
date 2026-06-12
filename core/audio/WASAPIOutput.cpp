/**
 * Aurora Motion Player — WASAPI Audio Output Implementation
 */

#ifdef _WIN32

#include "WASAPIOutput.h"

#include <Functiondiscoverykeys_devpkey.h>
#include <avrt.h>
#include <iostream>

#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "ole32.lib")

// WASAPI passthrough format GUID
static const GUID KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL =
    {0x00000092, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

static const GUID KSDATAFORMAT_SUBTYPE_IEC61937_DTS =
    {0x00000008, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

namespace aurora::core {

// ── Ctor / Dtor ───────────────────────────────────────────────────────────────
WASAPIOutput::WASAPIOutput()  = default;
WASAPIOutput::~WASAPIOutput() { close(); }

// ── Open ──────────────────────────────────────────────────────────────────────
bool WASAPIOutput::open(const Config& cfg) {
    m_config = cfg;

    if (!initCOM())                   return false;
    if (!openDevice(cfg.deviceId))    return false;
    if (!createAudioClient(cfg.mode)) return false;

    AudioFormat fmt = cfg.autoFormat ? AudioFormat{} : cfg.format;
    if (!negotiateFormat(fmt))        return false;

    m_open.store(true);
    std::cout << "[WASAPI] Opened: "
              << m_wfx.Format.nSamplesPerSec << "Hz "
              << m_wfx.Format.nChannels << "ch "
              << m_wfx.Format.wBitsPerSample << "bit\n";
    return true;
}

bool WASAPIOutput::initCOM() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                           CLSCTX_ALL, IID_PPV_ARGS(&m_enumerator));
    return SUCCEEDED(hr);
}

bool WASAPIOutput::openDevice(const std::string& deviceId) {
    HRESULT hr;
    if (deviceId.empty()) {
        hr = m_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
    } else {
        std::wstring wid(deviceId.begin(), deviceId.end());
        hr = m_enumerator->GetDevice(wid.c_str(), &m_device);
    }
    if (FAILED(hr)) {
        std::cerr << "[WASAPI] Failed to get device\n";
        return false;
    }

    hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                              nullptr, reinterpret_cast<void**>(&m_audioClient));
    return SUCCEEDED(hr);
}

bool WASAPIOutput::createAudioClient(WASAPIMode mode) {
    // Get mix format as starting point
    WAVEFORMATEX* pwfx = nullptr;
    m_audioClient->GetMixFormat(&pwfx);

    if (pwfx) {
        if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
            m_wfx = *reinterpret_cast<WAVEFORMATEXTENSIBLE*>(pwfx);
        } else {
            m_wfx.Format = *pwfx;
        }
        CoTaskMemFree(pwfx);
    }

    AUDCLNT_SHAREMODE shareMode = (mode == WASAPIMode::Exclusive)
        ? AUDCLNT_SHAREMODE_EXCLUSIVE
        : AUDCLNT_SHAREMODE_SHARED;

    REFERENCE_TIME bufDuration =
        static_cast<REFERENCE_TIME>(m_config.bufferMs) * 10000LL;

    HRESULT hr = m_audioClient->Initialize(
        shareMode,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
        bufDuration, 0,
        &m_wfx.Format, nullptr);

    if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT && mode == WASAPIMode::Exclusive) {
        // Fallback to shared mode
        std::cout << "[WASAPI] Exclusive mode unsupported, falling back to shared\n";
        hr = m_audioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
            bufDuration, 0,
            &m_wfx.Format, nullptr);
    }

    if (FAILED(hr)) return false;

    m_bufferEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_bufferEvent) return false;

    m_audioClient->SetEventHandle(m_bufferEvent);
    m_audioClient->GetBufferSize(&m_bufferFrames);

    m_audioClient->GetService(IID_PPV_ARGS(&m_renderClient));
    m_audioClient->GetService(IID_PPV_ARGS(&m_volumeControl));

    return m_renderClient != nullptr;
}

bool WASAPIOutput::negotiateFormat(const AudioFormat& requested) {
    if (!m_config.autoFormat) {
        // Override with requested format
        m_wfx.Format.nSamplesPerSec      = requested.sampleRate;
        m_wfx.Format.nChannels           = requested.channels;
        m_wfx.Format.wBitsPerSample      = requested.bitsPerSample;
        m_wfx.Format.nBlockAlign         = (requested.channels * requested.bitsPerSample) / 8;
        m_wfx.Format.nAvgBytesPerSec     = m_wfx.Format.nSamplesPerSec * m_wfx.Format.nBlockAlign;
    }
    // Format already set from mix format query — no further negotiation needed
    return true;
}

// ── Start / Stop ──────────────────────────────────────────────────────────────
bool WASAPIOutput::start() {
    if (!m_open.load()) return false;
    m_playing.store(true);
    m_paused.store(false);
    m_audioClient->Start();

    m_renderThread = std::thread([this] {
        // Set MMCSS thread priority for audio
        DWORD taskIndex = 0;
        HANDLE hTask = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
        renderLoop();
        if (hTask) AvRevertMmThreadCharacteristics(hTask);
    });

    std::cout << "[WASAPI] Playback started\n";
    return true;
}

void WASAPIOutput::stop() {
    m_playing.store(false);
    if (m_bufferEvent) SetEvent(m_bufferEvent);
    if (m_renderThread.joinable()) m_renderThread.join();
    if (m_audioClient) m_audioClient->Stop();
    flush();
}

void WASAPIOutput::pause() {
    m_paused.store(true);
    if (m_audioClient) m_audioClient->Stop();
}

void WASAPIOutput::resume() {
    m_paused.store(false);
    if (m_audioClient) m_audioClient->Start();
    if (m_bufferEvent) SetEvent(m_bufferEvent);
}

void WASAPIOutput::flush() {
    std::lock_guard lock(m_queueMutex);
    while (!m_queue.empty()) m_queue.pop();
    if (m_audioClient) m_audioClient->Reset();
    {
        std::lock_guard pl(m_posMutex);
        m_positionUs = 0;
    }
}

// ── Render loop ───────────────────────────────────────────────────────────────
void WASAPIOutput::renderLoop() {
    std::vector<uint8_t> silenceBuf;

    while (m_playing.load()) {
        DWORD waitResult = WaitForSingleObject(m_bufferEvent, 200);
        if (waitResult == WAIT_TIMEOUT) continue;
        if (!m_playing.load()) break;
        if (m_paused.load())   continue;

        UINT32 padding = 0;
        m_audioClient->GetCurrentPadding(&padding);
        UINT32 available = m_bufferFrames - padding;
        if (available == 0) continue;

        BYTE* pData = nullptr;
        if (FAILED(m_renderClient->GetBuffer(available, &pData))) continue;

        int   bytesPerFrame = m_wfx.Format.nBlockAlign;
        DWORD bytes         = available * bytesPerFrame;

        std::unique_lock lock(m_queueMutex);
        if (m_queue.empty()) {
            lock.unlock();
            // Silence
            memset(pData, 0, bytes);
            m_renderClient->ReleaseBuffer(available, AUDCLNT_BUFFERFLAGS_SILENT);
            if (m_underrunCb) m_underrunCb();
        } else {
            auto& chunk = m_queue.front();
            DWORD copyBytes = min(bytes, static_cast<DWORD>(chunk.data.size()));
            memcpy(pData, chunk.data.data(), copyBytes);
            if (copyBytes < bytes) memset(pData + copyBytes, 0, bytes - copyBytes);

            // Update position
            {
                std::lock_guard pl(m_posMutex);
                m_positionUs = chunk.pts;
            }
            m_queue.pop();
            lock.unlock();

            // Apply software volume if < 1.0
            float vol = m_volume.load();
            if (vol < 0.999f && m_wfx.Format.wBitsPerSample == 32) {
                auto* samples = reinterpret_cast<float*>(pData);
                int   count   = copyBytes / sizeof(float);
                for (int i = 0; i < count; ++i) samples[i] *= vol;
            }

            m_renderClient->ReleaseBuffer(available, 0);
        }
    }
}

// ── Push chunk ────────────────────────────────────────────────────────────────
bool WASAPIOutput::pushChunk(AudioChunk chunk) {
    std::lock_guard lock(m_queueMutex);
    if (static_cast<int>(m_queue.size()) >= k_MaxQueueChunks) return false;
    m_queue.push(std::move(chunk));
    m_queueCV.notify_one();
    return true;
}

// ── Volume ────────────────────────────────────────────────────────────────────
void WASAPIOutput::setVolume(float vol) {
    m_volume.store(vol);
    if (m_volumeControl)
        m_volumeControl->SetMasterVolume(vol, nullptr);
}

// ── Position ─────────────────────────────────────────────────────────────────
int64_t WASAPIOutput::positionUs() const {
    std::lock_guard lock(m_posMutex);
    return m_positionUs;
}

int WASAPIOutput::latencyMs() const {
    if (!m_audioClient) return 0;
    REFERENCE_TIME latency = 0;
    m_audioClient->GetStreamLatency(&latency);
    return static_cast<int>(latency / 10000);
}

// ── Passthrough support ───────────────────────────────────────────────────────
bool WASAPIOutput::supportsPassthrough() const {
    // Check if device supports IEC60958 or IEC61937 formats
    if (!m_audioClient) return false;
    WAVEFORMATEXTENSIBLE wfx = {};
    wfx.Format.wFormatTag            = WAVE_FORMAT_EXTENSIBLE;
    wfx.Format.nChannels             = 2;
    wfx.Format.nSamplesPerSec        = 48000;
    wfx.Format.wBitsPerSample        = 16;
    wfx.Format.nBlockAlign           = 4;
    wfx.Format.nAvgBytesPerSec       = 96000;
    wfx.Format.cbSize                = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    wfx.Samples.wValidBitsPerSample  = 16;
    wfx.dwChannelMask                = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    wfx.SubFormat = KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL;

    WAVEFORMATEX* closest = nullptr;
    HRESULT hr = m_audioClient->IsFormatSupported(
        AUDCLNT_SHAREMODE_EXCLUSIVE, &wfx.Format, &closest);
    if (closest) CoTaskMemFree(closest);
    return SUCCEEDED(hr);
}

// ── Device enumeration ────────────────────────────────────────────────────────
std::vector<WASAPIOutput::DeviceInfo> WASAPIOutput::enumerateDevices() {
    std::vector<DeviceInfo> result;

    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                   CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    if (FAILED(hr)) return result;

    ComPtr<IMMDeviceCollection> collection;
    hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(hr)) return result;

    // Default device ID
    ComPtr<IMMDevice> defaultDev;
    std::wstring defaultId;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDev))) {
        LPWSTR id = nullptr;
        defaultDev->GetId(&id);
        if (id) { defaultId = id; CoTaskMemFree(id); }
    }

    UINT count = 0;
    collection->GetCount(&count);
    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> dev;
        if (FAILED(collection->Item(i, &dev))) continue;

        LPWSTR id = nullptr;
        dev->GetId(&id);
        if (!id) continue;

        ComPtr<IPropertyStore> props;
        dev->OpenPropertyStore(STGM_READ, &props);

        PROPVARIANT pv;
        PropVariantInit(&pv);

        std::wstring name = L"Unknown";
        if (props && SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &pv))) {
            if (pv.vt == VT_LPWSTR) name = pv.pwszVal;
            PropVariantClear(&pv);
        }

        DeviceInfo info;
        info.id        = std::string(id, id + wcslen(id));
        info.name      = name;
        info.isDefault = (std::wstring(id) == defaultId);
        result.push_back(std::move(info));

        CoTaskMemFree(id);
    }
    return result;
}

// ── Close ─────────────────────────────────────────────────────────────────────
void WASAPIOutput::close() {
    stop();
    m_open.store(false);
    if (m_bufferEvent) { CloseHandle(m_bufferEvent); m_bufferEvent = nullptr; }
    m_renderClient.Reset();
    m_volumeControl.Reset();
    m_audioClient.Reset();
    m_device.Reset();
    m_enumerator.Reset();
}

} // namespace aurora::core

#endif // _WIN32
