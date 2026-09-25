#include "AudioInputManager.h"
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#endif
#include <QScopeGuard>

#include "AudioProcessing.h"

namespace {
#ifdef Q_OS_WIN
template <typename T> void release(T *&value) { if (value) { value->Release(); value = nullptr; } }
const PROPERTYKEY deviceFriendlyName = {{0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14};
QVariantList endpoints(const EDataFlow flow)
{
    QVariantList result; result.append(QVariantMap{{"id", QString{}}, {"name", flow == eRender ? QStringLiteral("Default output device") : QStringLiteral("Default microphone")}, {"detail", QStringLiteral("Windows default")}}); IMMDeviceEnumerator *enumerator = nullptr; IMMDeviceCollection *collection = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void **>(&enumerator))) || FAILED(enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &collection))) { release(collection); release(enumerator); return result; }
    auto cleanup = qScopeGuard([&] { release(collection); release(enumerator); });
    UINT count = 0; collection->GetCount(&count);
    for (UINT index = 0; index < count; ++index) { IMMDevice *device = nullptr; LPWSTR id = nullptr; IPropertyStore *store = nullptr; PROPVARIANT name; PropVariantInit(&name);
        if (SUCCEEDED(collection->Item(index, &device)) && SUCCEEDED(device->GetId(&id)) && SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store)) && SUCCEEDED(store->GetValue(deviceFriendlyName, &name))) result.append(QVariantMap{{"id", QString::fromWCharArray(id)}, {"name", QString::fromWCharArray(name.pwszVal)}, {"detail", QStringLiteral("Windows audio endpoint")}});
        PropVariantClear(&name); if (id) CoTaskMemFree(id); release(store); release(device);
    }
    return result;
}
#endif
}
AudioInputManager::AudioInputManager(QObject *parent) : QObject(parent)
{
    meterTimer_.setInterval(40);
    connect(&meterTimer_, &QTimer::timeout, this, &AudioInputManager::metersChanged);
    meterTimer_.start();
    mixerThread_ = std::jthread([this] { mixerLoop(); });
}
AudioInputManager::~AudioInputManager()
{
    mixerRunning_ = false;
    mixerWake_.wakeAll();
    if (mixerThread_.joinable()) mixerThread_.join();
}
void AudioInputManager::synchronizeSources(const QVector<Source> &sources)
{
    QMutexLocker lock(&entriesMutex_);
    QSet<QString> wanted;
    for (const Source &value : sources) if (value.type == SourceType::Microphone || value.type == SourceType::DesktopAudio) { wanted.insert(value.id); const QString endpoint = value.configuration.value("targetId").toString(); auto it = entries_.find(value.id); if (it == entries_.end() || it->endpointId != endpoint) { if (it != entries_.end()) entries_.erase(it); Entry entry; entry.endpointId = endpoint; entry.source = std::make_shared<WasapiAudioSource>(endpoint, value.type == SourceType::DesktopAudio, [this] { wakeMixer(); }); entry.source->start(); entries_.insert(value.id, std::move(entry)); } }
    for (auto it = entries_.begin(); it != entries_.end();) { if (!wanted.contains(it.key())) it = entries_.erase(it); else ++it; }
}
QVariantList AudioInputManager::targets(SourceType type) const {
#ifdef Q_OS_WIN
    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED); const QVariantList values = endpoints(type == SourceType::DesktopAudio ? eRender : eCapture); if (SUCCEEDED(result)) CoUninitialize(); return values;
#else
    Q_UNUSED(type); return {};
#endif
}
QVector<AudioInputManager::Entry> AudioInputManager::entriesSnapshot() const
{
    QMutexLocker lock(&entriesMutex_);
    QVector<Entry> snapshot;
    snapshot.reserve(entries_.size());
    for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) snapshot.append(it.value());
    return snapshot;
}
AudioRuntime AudioInputManager::runtime(const QString &id) const { QMutexLocker lock(&entriesMutex_); const auto it = entries_.constFind(id); return it == entries_.cend() ? AudioRuntime{} : it->source->runtime(); }
float AudioInputManager::levelDb(const QString &id) const { return runtime(id).peakDb; }
QHash<QString, AudioBlock> AudioInputManager::latestBlocks() const
{
    QHash<QString, AudioBlock> blocks;
    QMutexLocker lock(&entriesMutex_);
    for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) blocks.insert(it.key(), it->source->latestBlock());
    return blocks;
}
QHash<QString, QVector<AudioBlock>> AudioInputManager::takePendingBlocks()
{
    QHash<QString, QVector<AudioBlock>> blocks;
    QMutexLocker lock(&entriesMutex_);
    for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) blocks.insert(it.key(), it->source->takePendingBlocks());
    return blocks;
}
void AudioInputManager::discardPendingBlocks()
{
    for (const Entry &entry : entriesSnapshot()) entry.source->discardPendingBlocks();
}
QVariantMap AudioInputManager::diagnostics() const
{
    QVariantMap result;
    QMutexLocker lock(&entriesMutex_);
    for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) result.insert(it.key(), it->source->diagnostics());
    return result;
}
void AudioInputManager::setMixControls(const QString &id, const double gain, const bool muted)
{
    QMutexLocker lock(&entriesMutex_);
    const auto it = entries_.find(id);
    if (it != entries_.end()) it->source->setMixControls(gain, muted);
}
void AudioInputManager::setRecordingSink(std::function<void(AudioBlock)> sink)
{
    { QMutexLocker lock(&mixerMutex_); recordingSink_ = std::move(sink); mixerDirty_ = true; }
    mixerWake_.wakeAll();
}
void AudioInputManager::flushRecordingSink()
{
    std::function<void(AudioBlock)> sink;
    { QMutexLocker lock(&mixerMutex_); sink = recordingSink_; }
    if (sink) drainToSink(sink);
}
void AudioInputManager::wakeMixer()
{
    QMutexLocker lock(&mixerMutex_);
    mixerDirty_ = true;
    mixerWake_.wakeOne();
}
void AudioInputManager::mixerLoop()
{
    while (mixerRunning_) {
        std::function<void(AudioBlock)> sink;
        {
            QMutexLocker lock(&mixerMutex_);
            while (mixerRunning_ && (!recordingSink_ || !mixerDirty_)) mixerWake_.wait(&mixerMutex_);
            if (!mixerRunning_) break;
            sink = recordingSink_;
            mixerDirty_ = false;
        }
        if (sink) drainToSink(sink);
    }
}
void AudioInputManager::drainToSink(const std::function<void(AudioBlock)> &sink)
{
    QVector<QVector<AudioBlock>> perSource;
    for (const Entry &entry : entriesSnapshot()) perSource.append(entry.source->takePendingBlocks());
    int batches = 0;
    for (const QVector<AudioBlock> &blocks : perSource) batches = qMax(batches, blocks.size());
    for (int batch = 0; batch < batches; ++batch) {
        QVector<AudioBlock> sources;
        for (const QVector<AudioBlock> &blocks : perSource) if (batch < blocks.size()) sources.append(blocks.at(batch));
        AudioBlock mixed = AudioProcessing::mixToStereo(sources);
        if (!mixed.samples.isEmpty()) sink(std::move(mixed));
    }
}
