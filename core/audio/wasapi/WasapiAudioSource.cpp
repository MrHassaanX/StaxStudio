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

WasapiAudioSource::WasapiAudioSource(QString endpointId, bool loopback) : endpointId_(std::move(endpointId)), loopback_(loopback) {}
WasapiAudioSource::~WasapiAudioSource() { stop(); }
void WasapiAudioSource::start() { if (running_.exchange(true)) return; thread_ = std::jthread([this] { run(); }); }
void WasapiAudioSource::stop() { running_ = false; if (thread_.joinable()) thread_.join(); }
AudioRuntime WasapiAudioSource::runtime() const { QMutexLocker lock(&mutex_); return runtime_; }
AudioBlock WasapiAudioSource::latestBlock() const { QMutexLocker lock(&mutex_); return block_; }
void WasapiAudioSource::setMixControls(const double gain, const bool muted) { gain_ = qBound(0.0, gain, 1.0); muted_ = muted; }

void WasapiAudioSource::run()
{
#ifdef Q_OS_WIN
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
            const int channels = format->nChannels; const int samples = static_cast<int>(frames) * channels;
            QVector<float> values(samples); float peak = 0.0f;
            const bool isFloat = format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT || (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE && reinterpret_cast<WAVEFORMATEXTENSIBLE *>(format)->SubFormat.Data1 == WAVE_FORMAT_IEEE_FLOAT);
            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                if (isFloat && format->wBitsPerSample == 32) { const auto *input = reinterpret_cast<const float *>(data); for (int i = 0; i < samples; ++i) { values[i] = input[i]; peak = qMax(peak, qAbs(input[i])); } }
                else if (format->wBitsPerSample == 16) { const auto *input = reinterpret_cast<const qint16 *>(data); for (int i = 0; i < samples; ++i) { values[i] = input[i] / 32768.0f; peak = qMax(peak, qAbs(values[i])); } }
            }
            capture->ReleaseBuffer(frames);
            AudioBlock block{mediaTimestampNs(), static_cast<int>(format->nSamplesPerSec), channels, std::move(values)};
            block = AudioProcessing::normalizeToInternalFormat(std::move(block));
            AudioProcessing::applyGainAndMute(block, gain_.load(), muted_);
            { QMutexLocker lock(&mutex_); runtime_.peakDb = AudioProcessing::peakDb(block); block_ = std::move(block); runtime_.state = CaptureState::Active; }
            if (FAILED(capture->GetNextPacketSize(&packets))) { packets = 0; }
        }
    }
    client->Stop();
#else
    QMutexLocker lock(&mutex_); runtime_ = {CaptureState::Unavailable, QStringLiteral("WASAPI is only available on Windows"), -90};
#endif
    QMutexLocker lock(&mutex_); if (runtime_.state == CaptureState::Active) runtime_.state = CaptureState::Stopped;
}