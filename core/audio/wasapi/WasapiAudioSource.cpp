#include "WasapiAudioSource.h"
#include "core/audio/AudioProcessing.h"

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#endif
#include <QScopeGuard>
#include <QtMath>

namespace {
#ifdef Q_OS_WIN
template <typename T> void release(T *&value) { if (value) { value->Release(); value = nullptr; } }
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
    return {{"capturedBlocks", capturedBlocks_}, {"pendingBlocks", static_cast<qulonglong>(pendingBlocks_.size())},
            {"droppedPendingBlocks", droppedPendingBlocks_}, {"firstTimestampNs", firstTimestampNs_},
            {"lastTimestampNs", lastTimestampNs_}};
}
void WasapiAudioSource::setMixControls(const double gain, const bool muted) { gain_ = qBound(0.0, gain, 10.0); muted_ = muted; }

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
    if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(&client))) || FAILED(client->GetMixFormat(&format))) { QMutexLocker lock(&mutex_); runtime_ = {CaptureState::Error, QStringLiteral("WASAPI initialization failed"), -90}; return; }
    eventHandle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | (loopback_ ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0);
    if (!eventHandle || FAILED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 0, 0, format, nullptr)) || FAILED(client->SetEventHandle(eventHandle)) || FAILED(client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void **>(&capture))) || FAILED(client->Start())) { QMutexLocker lock(&mutex_); runtime_ = {CaptureState::Error, QStringLiteral("WASAPI capture start failed"), -90}; return; }
    { QMutexLocker lock(&mutex_); runtime_ = {CaptureState::Active, {}, -90}; }
    while (running_) {
        if (WaitForSingleObject(eventHandle, 250) != WAIT_OBJECT_0) continue;
        UINT32 packets = 0; if (FAILED(capture->GetNextPacketSize(&packets))) break;
        while (packets) {
            BYTE *data = nullptr; UINT32 frames = 0; DWORD flags = 0;
            if (FAILED(capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) break;
            const int channels = format->nChannels;
            const bool extensible=format->wFormatTag==WAVE_FORMAT_EXTENSIBLE && format->cbSize>=22;
            const auto *extended=extensible ? reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(format) : nullptr;
            const bool isFloat=format->wFormatTag==WAVE_FORMAT_IEEE_FLOAT
                || (extended && extended->SubFormat.Data1==WAVE_FORMAT_IEEE_FLOAT);
            const quint64 mask=extended ? extended->dwChannelMask : 0;
            AudioBlock block;
            try {
                block=AudioProcessing::convertNativePcm(flags&AUDCLNT_BUFFERFLAGS_SILENT ? nullptr : data,
                    frames,format->nSamplesPerSec,channels,format->wBitsPerSample,isFloat,mask,mediaTimestampNs(),resampleState);
            } catch(const std::exception &error) {
                capture->ReleaseBuffer(frames);
                QMutexLocker lock(&mutex_);
                runtime_={CaptureState::Error,QString::fromUtf8(error.what()),-90}; running_=false; break;
            }
            capture->ReleaseBuffer(frames);
            AudioProcessing::applyGainAndMute(block, gain_.load(), muted_);
            {
                QMutexLocker lock(&mutex_);
                runtime_.peakDb = AudioProcessing::peakDb(block);
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
