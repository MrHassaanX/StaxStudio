#include "WasapiAudioSource.h"
#include "core/audio/AudioProcessing.h"

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <endpointvolume.h>
#endif
#include <QScopeGuard>
#include <QtMath>
#include <QUuid>
#include <cmath>

namespace {
#ifdef Q_OS_WIN
template <typename T> void release(T *&value) { if (value) { value->Release(); value = nullptr; } }
const PROPERTYKEY deviceFriendlyName = {{0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14};
// MinGW's endpointvolume.h only forward-declares this Windows COM interface.
struct EndpointMeter : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetPeakValue(float *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMeteringChannelCount(UINT *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetChannelsPeakValues(UINT, float *) = 0;
    virtual HRESULT STDMETHODCALLTYPE QueryHardwareSupport(DWORD *) = 0;
};
constexpr GUID meterIid{0xc02216f6,0x8c67,0x4b5b,{0x9d,0x00,0xd0,0x08,0xe7,0x3e,0x00,0x64}};
constexpr GUID floatSubtype{3,0,0x10,{0x80,0,0,0xaa,0,0x38,0x9b,0x71}};
constexpr GUID pcmSubtype{1,0,0x10,{0x80,0,0,0xaa,0,0x38,0x9b,0x71}};
float decibels(float peak) { return peak > 0 ? qMax(-90.0f, 20.0f * std::log10(peak)) : -90.0f; }
#endif
}

WasapiAudioSource::WasapiAudioSource(QString endpointId, bool loopback, std::function<void()> blockReady)
    : endpointId_(std::move(endpointId)), loopback_(loopback), blockReady_(std::move(blockReady)) {}
WasapiAudioSource::~WasapiAudioSource() { stop(); }
void WasapiAudioSource::start() { if (running_.exchange(true)) return; thread_ = std::jthread([this] { run(); }); }
void WasapiAudioSource::stop() { running_ = false; if (thread_.joinable()) thread_.join(); }
AudioRuntime WasapiAudioSource::runtime() const { QMutexLocker lock(&mutex_); return runtime_; }
AudioBlock WasapiAudioSource::latestBlock() const { QMutexLocker lock(&mutex_); return block_; }
QVector<AudioBlock> WasapiAudioSource::takePendingBlocks()
{
    QMutexLocker lock(&mutex_);
    QVector<AudioBlock> blocks;
    blocks.reserve(static_cast<qsizetype>(pendingBlocks_.size()));
    while (!pendingBlocks_.empty()) { blocks.append(std::move(pendingBlocks_.front())); pendingBlocks_.pop_front(); }
    return blocks;
}
void WasapiAudioSource::discardPendingBlocks() { QMutexLocker lock(&mutex_); pendingBlocks_.clear(); }
QVariantMap WasapiAudioSource::diagnostics() const
{
    QMutexLocker lock(&mutex_);
    QVariantMap values = {{"capturedBlocks", capturedBlocks_}, {"pendingBlocks", static_cast<qulonglong>(pendingBlocks_.size())},
            {"droppedPendingBlocks", droppedPendingBlocks_}, {"firstTimestampNs", firstTimestampNs_},
            {"lastTimestampNs", lastTimestampNs_}, {"format", captureFormat_},
            {"peakAfterConversionDb", convertedPeakDb_}, {"peakAfterSourceGainDb", gainedPeakDb_},
            {"endpointId", openedEndpointId_}, {"requestedEndpointId", endpointId_},
            {"usesDefaultEndpoint", endpointId_.isEmpty()}, {"endpointFriendlyName", endpointName_}, {"dataFlow", dataFlow_},
            {"loopbackEnabled", loopbackActive_}, {"endpointVolumeScalar", endpointVolume_}, {"endpointMuted", endpointMuted_},
            {"endpointMeterPeak", endpointMeter_}, {"silentPacketCount", silentPackets_}, {"totalPacketCount", totalPackets_}};
    for (auto it = nativeDiagnostics_.cbegin(); it != nativeDiagnostics_.cend(); ++it) values.insert(it.key(), it.value());
    return values;
}
void WasapiAudioSource::setMixControls(const double gain, const bool muted) { gain_ = qBound(0.0, gain, 1.0); muted_ = muted; }

void WasapiAudioSource::run()
{
#ifdef Q_OS_WIN
    AudioResampleState resampleState;
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) { QMutexLocker lock(&mutex_); runtime_ = {CaptureState::Error, QStringLiteral("WASAPI COM initialization failed"), -90}; return; }
    auto uninitialize = qScopeGuard([] { CoUninitialize(); });
    IMMDeviceEnumerator *enumerator = nullptr; IMMDevice *device = nullptr; IAudioClient *client = nullptr; IAudioCaptureClient *capture = nullptr; WAVEFORMATEX *format = nullptr; HANDLE eventHandle = nullptr;
    auto cleanup = qScopeGuard([&] { if (eventHandle) CloseHandle(eventHandle); if (format) CoTaskMemFree(format); release(capture); release(client); release(device); release(enumerator); });
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void **>(&enumerator)))) { QMutexLocker lock(&mutex_); runtime_ = {CaptureState::Error, QStringLiteral("WASAPI device enumerator failed"), -90}; return; }
    const EDataFlow flow = loopback_ ? eRender : eCapture;
    HRESULT result = endpointId_.isEmpty() ? enumerator->GetDefaultAudioEndpoint(flow, eConsole, &device) : enumerator->GetDevice(reinterpret_cast<LPCWSTR>(endpointId_.utf16()), &device);
    if (FAILED(result)) { QMutexLocker lock(&mutex_); runtime_ = {CaptureState::Unavailable, QStringLiteral("Audio endpoint unavailable"), -90}; return; }
    LPWSTR openedId = nullptr; IPropertyStore *store = nullptr; PROPVARIANT friendly; PropVariantInit(&friendly);
    device->GetId(&openedId); device->OpenPropertyStore(STGM_READ, &store); if (store) store->GetValue(deviceFriendlyName, &friendly);
    IAudioEndpointVolume *endpointVolume = nullptr;
    EndpointMeter *endpointMeter = nullptr;
    IMMEndpoint *endpoint = nullptr;
    auto cleanupEndpoint = qScopeGuard([&] {
        if (openedId) CoTaskMemFree(openedId);
        PropVariantClear(&friendly); release(store); release(endpointVolume); release(endpointMeter); release(endpoint);
    });
    device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(&endpointVolume));
    const HRESULT meterResult = device->Activate(meterIid, CLSCTX_ALL, nullptr, reinterpret_cast<void **>(&endpointMeter));
    EDataFlow actualFlow = eAll;
    if (SUCCEEDED(device->QueryInterface(__uuidof(IMMEndpoint), reinterpret_cast<void **>(&endpoint)))) endpoint->GetDataFlow(&actualFlow);
    { QMutexLocker lock(&mutex_); openedEndpointId_ = openedId ? QString::fromWCharArray(openedId) : QString{}; endpointName_ = friendly.vt == VT_LPWSTR ? QString::fromWCharArray(friendly.pwszVal) : QString{}; dataFlow_ = actualFlow == eRender ? QStringLiteral("eRender") : actualFlow == eCapture ? QStringLiteral("eCapture") : QStringLiteral("unknown"); loopbackActive_ = loopback_; }
    if (actualFlow != flow || (!endpointId_.isEmpty() && endpointId_ != openedEndpointId_)) {
        QMutexLocker lock(&mutex_); runtime_ = {CaptureState::Error, QStringLiteral("Selected audio endpoint does not match capture direction/identity"), -90}; return;
    }
    if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(&client))) || FAILED(client->GetMixFormat(&format))) { QMutexLocker lock(&mutex_); runtime_ = {CaptureState::Error, QStringLiteral("WASAPI initialization failed"), -90}; return; }
    eventHandle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | (loopback_ ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0);
    const bool extensible = format->wFormatTag == WAVE_FORMAT_EXTENSIBLE && format->cbSize >= 22;
    const auto *extended = extensible ? reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(format) : nullptr;
    const bool isFloat = format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT || (extended && IsEqualGUID(extended->SubFormat, floatSubtype));
    const bool isPcm = format->wFormatTag == WAVE_FORMAT_PCM || (extended && IsEqualGUID(extended->SubFormat, pcmSubtype));
    if ((!isFloat && !isPcm) || (isFloat && format->wBitsPerSample != 32 && format->wBitsPerSample != 64)
        || format->nBlockAlign != format->nChannels * format->wBitsPerSample / 8) {
        QMutexLocker lock(&mutex_); runtime_ = {CaptureState::Error, QStringLiteral("Unsupported WASAPI sample layout"), -90}; return;
    }
    if (!eventHandle || FAILED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 0, 0, format, nullptr)) || FAILED(client->SetEventHandle(eventHandle)) || FAILED(client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void **>(&capture))) || FAILED(client->Start())) { QMutexLocker lock(&mutex_); runtime_ = {CaptureState::Error, QStringLiteral("WASAPI capture start failed"), -90}; return; }
    ISimpleAudioVolume *sessionVolume = nullptr;
    IAudioStreamVolume *streamVolume = nullptr;
    client->GetService(__uuidof(ISimpleAudioVolume), reinterpret_cast<void **>(&sessionVolume));
    client->GetService(__uuidof(IAudioStreamVolume), reinterpret_cast<void **>(&streamVolume));
    auto cleanupVolumes = qScopeGuard([&] { release(sessionVolume); release(streamVolume); });
    { QMutexLocker lock(&mutex_); runtime_ = {CaptureState::Active, {}, -90}; }
    { QMutexLocker lock(&mutex_); captureFormat_ = QStringLiteral("%1 Hz, %2 ch, %3-bit %4")
        .arg(format->nSamplesPerSec).arg(format->nChannels).arg(format->wBitsPerSample)
        .arg(isFloat ? QStringLiteral("float") : QStringLiteral("PCM")); }
    UINT32 bufferFrames = 0; client->GetBufferSize(&bufferFrames);
    { QMutexLocker lock(&mutex_); nativeDiagnostics_ = {
        {"wFormatTag", format->wFormatTag}, {"subFormat", extended ? QUuid(extended->SubFormat).toString() : QString{}},
        {"inputSampleRate", quint32(format->nSamplesPerSec)}, {"inputChannels", format->nChannels},
        {"inputBitsPerSample", format->wBitsPerSample}, {"validBitsPerSample", extended ? extended->Samples.wValidBitsPerSample : format->wBitsPerSample},
        {"blockAlign", format->nBlockAlign}, {"sharedModeFlags", quint32(flags)}, {"bufferFrames", bufferFrames},
        {"endpointMeterHresult", quint32(meterResult)}}; }
    qint64 summaryStart = mediaTimestampNs(), nextMeter = 0;
    float rawMaximum = 0, meterMaximum = -1, convertedMaximum = -90, gainedMaximum = -90;
    quint64 packetFlags = 0;
    // GetBuffer's QPC position describes the FIRST sample, not callback arrival.
    // Anchor to the process clock once, then advance on the device sample clock.
    LARGE_INTEGER frequency{}, counter{};
    QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&counter);
    const qint64 qpcNowNs = (counter.QuadPart / frequency.QuadPart)*1'000'000'000LL
        + (counter.QuadPart % frequency.QuadPart)*1'000'000'000LL/frequency.QuadPart;
    const qint64 qpcOffset = mediaTimestampNs() - qpcNowNs;
    qint64 sourceOriginNs = -1;
    bool sourceAnchored = false;
    UINT64 sourceOriginFrame = 0, nextDeviceFrame = 0;
    while (running_) {
        if (WaitForSingleObject(eventHandle, 250) != WAIT_OBJECT_0) continue;
        UINT32 packets = 0; if (FAILED(capture->GetNextPacketSize(&packets))) break;
        while (packets) {
            BYTE *data = nullptr; UINT32 frames = 0; DWORD flags = 0;
            UINT64 deviceFrame = 0, packetQpc = 0;
            if (FAILED(capture->GetBuffer(&data, &frames, &flags, &deviceFrame, &packetQpc))) break;
            if (!frames) break;
            const bool timestampValid = !(flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR);
            if (!timestampValid && sourceAnchored) deviceFrame = nextDeviceFrame;
            if (!sourceAnchored || deviceFrame != nextDeviceFrame) {
                sourceOriginNs = timestampValid ? qint64(packetQpc)*100 + qpcOffset
                    : mediaTimestampNs()-qint64(frames)*1'000'000'000LL/format->nSamplesPerSec;
                sourceOriginFrame = deviceFrame;
                sourceAnchored = true;
                resampleState = {}; // Real source discontinuity, not callback jitter.
            }
            const qint64 sampleTimestamp = sourceOriginNs + qint64(deviceFrame-sourceOriginFrame)*1'000'000'000LL/format->nSamplesPerSec;
            nextDeviceFrame = deviceFrame + frames;
            const int channels = format->nChannels;
            { QMutexLocker lock(&mutex_); ++totalPackets_; if (flags & AUDCLNT_BUFFERFLAGS_SILENT) ++silentPackets_; }
            packetFlags |= flags;
            // Independently inspect native float bytes before conversion. No gain or resampling.
            if (isFloat && format->wBitsPerSample == 32 && data && !(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                for (size_t i = 0; i < size_t(frames) * channels; ++i) {
                    float sample; memcpy(&sample, data + i * sizeof(float), sizeof(float));
                    if (std::isfinite(sample)) rawMaximum = qMax(rawMaximum, std::abs(sample));
                }
            }
            const quint64 mask=extended ? extended->dwChannelMask : 0;
            AudioBlock block;
            try {
                block=AudioProcessing::convertNativePcm(flags&AUDCLNT_BUFFERFLAGS_SILENT ? nullptr : data,
                    frames,format->nSamplesPerSec,channels,format->wBitsPerSample,isFloat,mask,sampleTimestamp,resampleState);
            } catch(const std::exception &error) {
                capture->ReleaseBuffer(frames);
                QMutexLocker lock(&mutex_);
                runtime_={CaptureState::Error,QString::fromUtf8(error.what()),-90}; running_=false; break;
            }
            capture->ReleaseBuffer(frames);
            if (!block.isValid()) continue;
            const float convertedPeak = AudioProcessing::peakDb(block);
            AudioProcessing::applyGainAndMute(block, gain_.load(), muted_);
            convertedMaximum = qMax(convertedMaximum, convertedPeak);
            gainedMaximum = qMax(gainedMaximum, AudioProcessing::peakDb(block));
            const qint64 now = mediaTimestampNs();
            if (endpointMeter && now >= nextMeter) {
                float peak = 0;
                if (SUCCEEDED(endpointMeter->GetPeakValue(&peak))) meterMaximum = qMax(meterMaximum, peak);
                nextMeter = now + 10'000'000LL;
            }
            if (now - summaryStart >= 1'000'000'000LL) {
                float volume = -1; BOOL muted = FALSE;
                if (endpointVolume) { endpointVolume->GetMasterVolumeLevelScalar(&volume); endpointVolume->GetMute(&muted); }
                float sessionGain = -1; BOOL sessionMuted = FALSE;
                if (sessionVolume) { sessionVolume->GetMasterVolume(&sessionGain); sessionVolume->GetMute(&sessionMuted); }
                QVariantList channelGains;
                if (streamVolume) {
                    UINT count = 0;
                    if (SUCCEEDED(streamVolume->GetChannelCount(&count))) for (UINT i=0; i<count; ++i) {
                        float gain = -1;
                        if (SUCCEEDED(streamVolume->GetChannelVolume(i,&gain))) channelGains.append(gain);
                    }
                }
                QMutexLocker lock(&mutex_);
                nativeDiagnostics_.insert("sessionVolumeScalar", sessionGain >= 0 ? QVariant(sessionGain) : QVariant{});
                nativeDiagnostics_.insert("sessionMuted", sessionGain >= 0 ? QVariant(bool(sessionMuted)) : QVariant{});
                nativeDiagnostics_.insert("streamChannelGains", channelGains);
                endpointVolume_ = volume; endpointMuted_ = muted; endpointMeter_ = meterMaximum;
                nativeDiagnostics_.insert("WindowsEndpointPeakDbfs", meterMaximum >= 0 ? QVariant(decibels(meterMaximum)) : QVariant{});
                nativeDiagnostics_.insert("CapturedSamplePeakDbfs", isFloat && format->wBitsPerSample == 32 ? QVariant(decibels(rawMaximum)) : QVariant{});
                nativeDiagnostics_.insert("convertedWindowPeakDbfs", convertedMaximum);
                nativeDiagnostics_.insert("postGainWindowPeakDbfs", gainedMaximum);
                nativeDiagnostics_.insert("measurementStartNs", summaryStart);
                nativeDiagnostics_.insert("measurementEndNs", now);
                nativeDiagnostics_.insert("lastPacketFrames", frames);
                nativeDiagnostics_.insert("packetFlags", packetFlags);
                summaryStart = now; rawMaximum = 0; meterMaximum = -1; convertedMaximum = gainedMaximum = -90; packetFlags = 0;
            }
            {
                QMutexLocker lock(&mutex_);
                runtime_.peakDb = AudioProcessing::peakDb(block);
                convertedPeakDb_ = convertedPeak;
                gainedPeakDb_ = runtime_.peakDb;
                block_ = block;
                // The recorder drains this bounded queue; a UI meter may still
                // read block_ without ever owning the capture stream.
                if (pendingBlocks_.size() >= 512) { pendingBlocks_.pop_front(); ++droppedPendingBlocks_; }
                if (capturedBlocks_ == 0) firstTimestampNs_ = block.timestampNs;
                lastTimestampNs_ = block.timestampNs;
                pendingBlocks_.push_back(std::move(block));
                ++capturedBlocks_;
                runtime_.state = CaptureState::Active;
            }
            if (blockReady_) blockReady_();
            if (FAILED(capture->GetNextPacketSize(&packets))) { packets = 0; }
        }
    }
    client->Stop();
#else
    QMutexLocker lock(&mutex_); runtime_ = {CaptureState::Unavailable, QStringLiteral("WASAPI is only available on Windows"), -90};
#endif
    QMutexLocker lock(&mutex_); if (runtime_.state == CaptureState::Active) runtime_.state = CaptureState::Stopped;
}
