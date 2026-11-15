#include "AudioInputManager.h"
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#endif
#include <QScopeGuard>
#include <algorithm>
#include <cmath>

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
QString captureEndpointId(const QString &requestedId, EDataFlow flow)
{
    if (!requestedId.isEmpty()) return requestedId;
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IMMDeviceEnumerator *enumerator = nullptr;
    IMMDevice *device = nullptr;
    LPWSTR id = nullptr;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                   __uuidof(IMMDeviceEnumerator), reinterpret_cast<void **>(&enumerator)))
        && SUCCEEDED(enumerator->GetDefaultAudioEndpoint(flow, eConsole, &device)))
        device->GetId(&id);
    const QString resolved = id ? QString::fromWCharArray(id) : QString{};
    if (id) CoTaskMemFree(id);
    release(device);
    release(enumerator);
    if (SUCCEEDED(initialized)) CoUninitialize();
    return resolved;
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
    entries_.clear(); // Join source callbacks while wake/lock members still exist.
}
void AudioInputManager::synchronizeSources(const QVector<Source> &sources)
{
    struct Desired { QString id, key, endpoint; bool loopback, global; };
    QVector<Desired> desired;
    for (const Source &value : sources) {
        if (value.type != SourceType::Microphone && value.type != SourceType::DesktopAudio) continue;
        const bool loopback = value.type == SourceType::DesktopAudio;
        const QString requested = value.configuration.value(QStringLiteral("targetId")).toString();
#ifdef Q_OS_WIN
        const QString resolved = captureEndpointId(requested, loopback ? eRender : eCapture);
#else
        const QString resolved = requested;
#endif
        desired.append({value.id, (loopback ? QStringLiteral("output:") : QStringLiteral("input:")) + resolved,
                        resolved, loopback,
                        value.configuration.value(QStringLiteral("audioScope")).toString() == QStringLiteral("global")});
    }
    QVector<std::shared_ptr<WasapiAudioSource>> toStart;
    QHash<QString, Entry> retired;
    {
        QMutexLocker lock(&entriesMutex_);
        QHash<QString, std::shared_ptr<WasapiAudioSource>> sessions;
        for (auto it = entries_.cbegin(); it != entries_.cend(); ++it)
            sessions.insert(it->endpointKey, it->source);
        QHash<QString, Entry> next;
        for (const Desired &route : desired) {
            auto session = sessions.value(route.key);
            if (!session) {
                session = std::make_shared<WasapiAudioSource>(route.endpoint, route.loopback,
                                                               [this] { wakeMixer(); });
                sessions.insert(route.key, session);
                toStart.append(session);
            }
            Entry entry{route.key, session, route.global};
            const auto previous = entries_.constFind(route.id);
            if (previous != entries_.cend()) { entry.gain = previous->gain; entry.muted = previous->muted; }
            next.insert(route.id, std::move(entry));
        }
        retired.swap(entries_);
        entries_.swap(next);
    }
    retired.clear(); // A capture worker may join here, outside the manager lock.
    for (const auto &source : toStart) source->start();
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
AudioRuntime AudioInputManager::runtime(const QString &id) const
{
    QMutexLocker lock(&entriesMutex_);
    const auto it = entries_.constFind(id);
    if (it == entries_.cend()) return {};
    AudioRuntime result = it->source->runtime();
    result.peakDb = it->muted || it->gain <= 0 ? -90.0f
        : std::max(-90.0f, result.peakDb + 20.0f * std::log10(it->gain));
    return result;
}
float AudioInputManager::levelDb(const QString &id) const { return runtime(id).peakDb; }
QHash<QString, AudioBlock> AudioInputManager::latestBlocks() const
{
    QHash<QString, AudioBlock> blocks;
    QMutexLocker lock(&entriesMutex_);
    for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) {
        AudioBlock block = it->source->latestBlock();
        AudioProcessing::applyGainAndMute(block, it->gain, it->muted);
        blocks.insert(it.key(), std::move(block));
    }
    return blocks;
}
QHash<QString, QVector<AudioBlock>> AudioInputManager::takePendingBlocks()
{
    QHash<QString, QVector<AudioBlock>> blocks;
    QMutexLocker lock(&entriesMutex_);
    QVector<AudioRoute> routes;
    routes.reserve(entries_.size());
    for (auto it = entries_.cbegin(); it != entries_.cend(); ++it)
        routes.append({it.key(), it->endpointKey, it->global});
    const auto owners = selectAudioRouteOwners(routes, activeProgramSources_);
    for (auto owner = owners.cbegin(); owner != owners.cend(); ++owner) {
        const auto entry = entries_.constFind(owner.value());
        if (entry == entries_.cend()) continue;
        auto pending = entry->source->takePendingBlocks();
        for (AudioBlock &block : pending)
            AudioProcessing::applyGainAndMute(block, entry->gain, entry->muted);
        blocks.insert(owner.value(), std::move(pending));
    }
    // Drain inactive sessions as well, so their queues cannot grow while a
    // different scene is on air.
    QSet<QString> drained;
    for (auto owner = owners.cbegin(); owner != owners.cend(); ++owner) drained.insert(owner.key());
    for (auto it = entries_.cbegin(); it != entries_.cend(); ++it)
        if (!drained.contains(it->endpointKey)) {
            it->source->discardPendingBlocks();
            drained.insert(it->endpointKey);
        }
    return blocks;
}
void AudioInputManager::discardPendingBlocks()
{
    QSet<WasapiAudioSource *> drained;
    for (const Entry &entry : entriesSnapshot())
        if (!drained.contains(entry.source.get())) {
            entry.source->discardPendingBlocks();
            drained.insert(entry.source.get());
        }
}
QVariantMap AudioInputManager::diagnostics() const
{
    QVariantMap result;
    QMutexLocker processing(&processingMutex_);
    result.insert("programMix",programMixer_.diagnostics());
    QMutexLocker lock(&entriesMutex_);
    QVector<AudioRoute> routes;
    for (auto it = entries_.cbegin(); it != entries_.cend(); ++it)
        routes.append({it.key(), it->endpointKey, it->global});
    const auto owners = selectAudioRouteOwners(routes, activeProgramSources_);
    for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) {
        QVariantMap source = it->source->diagnostics();
        source.insert(QStringLiteral("sharedEndpointKey"), it->endpointKey);
        source.insert(QStringLiteral("sourceGain"), it->gain);
        source.insert(QStringLiteral("sourceMuted"), it->muted);
        source.insert(QStringLiteral("feedsProgramMix"), owners.value(it->endpointKey) == it.key());
        result.insert(it.key(), source);
    }
    return result;
}
void AudioInputManager::setMixControls(const QString &id, const double gain, const bool muted)
{
    QMutexLocker lock(&entriesMutex_);
    const auto it = entries_.find(id);
    if (it != entries_.end()) { it->gain = std::clamp(static_cast<float>(gain), 0.0f, 1.0f); it->muted = muted; }
}
void AudioInputManager::setActiveProgramSources(const QSet<QString> &sourceIds)
{
    QMutexLocker processing(&processingMutex_);
    for (const QString &id : activeProgramSources_ - sourceIds) programMixer_.removeSource(id);
    activeProgramSources_ = sourceIds;
}
QSet<QString> AudioInputManager::programSourceIds() const
{
    QMutexLocker processing(&processingMutex_);
    QMutexLocker lock(&entriesMutex_);
    QVector<AudioRoute> routes;
    for (auto it = entries_.cbegin(); it != entries_.cend(); ++it)
        routes.append({it.key(), it->endpointKey, it->global});
    const auto owners = selectAudioRouteOwners(routes, activeProgramSources_);
    QSet<QString> ids;
    for (auto it = owners.cbegin(); it != owners.cend(); ++it) ids.insert(it.value());
    return ids;
}
void AudioInputManager::setRecordingSink(std::function<void(ProgramMixedAudioBlock)> sink)
{
    { QMutexLocker lock(&processingMutex_);
      if(sink && !recordingSink_) { discardPendingBlocks(); programMixer_.reset(mediaTimestampNs()); }
      recordingSink_ = std::move(sink); }
    mixerWake_.wakeAll();
}
void AudioInputManager::flushRecordingSink()
{
    drainToSink(true);
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
        {
            QMutexLocker lock(&mixerMutex_);
            if (mixerRunning_ && !mixerDirty_) mixerWake_.wait(&mixerMutex_,10);
            if (!mixerRunning_) break;
            mixerDirty_ = false;
        }
        drainToSink();
    }
}
void AudioInputManager::drainToSink(bool flush)
{
    QMutexLocker processing(&processingMutex_);
    auto pending=takePendingBlocks();
    if(!recordingSink_) return;
    QSet<QString> routed;
    for (auto it = pending.cbegin(); it != pending.cend(); ++it) routed.insert(it.key());
    for (const QString &id : activeProgramSources_ - routed) programMixer_.removeSource(id);
    for(auto it=pending.begin();it!=pending.end();++it) {
        if (!activeProgramSources_.contains(it.key())) continue;
        for(auto &block:it.value()) programMixer_.push(it.key(),std::move(block));
    }
    programMixer_.produceUntil(mediaTimestampNs()-(flush?0:ProgramAudioMixer::LatencyNs),recordingSink_);
}
