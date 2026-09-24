#include "AudioInputManager.h"
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#endif
#include <QScopeGuard>

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
AudioInputManager::AudioInputManager(QObject *parent) : QObject(parent) { meterTimer_.setInterval(40); connect(&meterTimer_, &QTimer::timeout, this, &AudioInputManager::metersChanged); meterTimer_.start(); }
void AudioInputManager::synchronizeSources(const QVector<Source> &sources)
{
    QSet<QString> wanted;
    for (const Source &value : sources) if (value.type == SourceType::Microphone || value.type == SourceType::DesktopAudio) { wanted.insert(value.id); const QString endpoint = value.configuration.value("targetId").toString(); auto it = entries_.find(value.id); if (it == entries_.end() || it->endpointId != endpoint) { if (it != entries_.end()) entries_.erase(it); Entry entry; entry.endpointId = endpoint; entry.source = std::make_shared<WasapiAudioSource>(endpoint, value.type == SourceType::DesktopAudio); entry.source->start(); entries_.insert(value.id, std::move(entry)); } }
    for (auto it = entries_.begin(); it != entries_.end();) { if (!wanted.contains(it.key())) it = entries_.erase(it); else ++it; }
}
QVariantList AudioInputManager::targets(SourceType type) const {
#ifdef Q_OS_WIN
    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED); const QVariantList values = endpoints(type == SourceType::DesktopAudio ? eRender : eCapture); if (SUCCEEDED(result)) CoUninitialize(); return values;
#else
    Q_UNUSED(type); return {};
#endif
}
AudioRuntime AudioInputManager::runtime(const QString &id) const { const auto it = entries_.constFind(id); return it == entries_.cend() ? AudioRuntime{} : it->source->runtime(); }
float AudioInputManager::levelDb(const QString &id) const { return runtime(id).peakDb; }
void AudioInputManager::setMixControls(const QString &id, const double gain, const bool muted)
{
    const auto it = entries_.find(id);
    if (it != entries_.end()) it->source->setMixControls(gain, muted);
}