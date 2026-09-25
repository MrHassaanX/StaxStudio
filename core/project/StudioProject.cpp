#include "StudioProject.h"

#include <QUuid>
#include <QSet>
#include <QtMath>

#include <cmath>

namespace {
QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString uniqueName(const QString &requested, const QString &fallback, const auto &items, const auto &nameFor)
{
    const QString base = requested.trimmed().isEmpty() ? fallback : requested.trimmed();
    QString candidate = base;
    int suffix = 2;
    const auto contains = [&]() {
        for (const auto &item : items) {
            if (nameFor(item).compare(candidate, Qt::CaseInsensitive) == 0) return true;
        }
        return false;
    };
    while (contains()) candidate = QStringLiteral("%1 (%2)").arg(base).arg(suffix++);
    return candidate;
}

SceneItem *activeItem(StudioProject &project, const QString &itemId)
{
    Scene *scene = project.activeScene();
    if (!scene) return nullptr;
    for (SceneItem &item : scene->items) if (item.id == itemId) return &item;
    return nullptr;
}

Transform sanitizedTransform(Transform value)
{
    value.width = qMax(1.0, value.width);
    value.height = qMax(1.0, value.height);
    value.scaleX = qBound(0.01, value.scaleX, 100.0);
    value.scaleY = qBound(0.01, value.scaleY, 100.0);
    value.rotation = std::fmod(value.rotation, 360.0);
    if (value.rotation < 0.0) value.rotation += 360.0;
    value.cropLeft = qBound(0.0, value.cropLeft, value.width - 1.0);
    value.cropRight = qBound(0.0, value.cropRight, value.width - value.cropLeft - 1.0);
    value.cropTop = qBound(0.0, value.cropTop, value.height - 1.0);
    value.cropBottom = qBound(0.0, value.cropBottom, value.height - value.cropTop - 1.0);
    return value;
}
}

StudioProject StudioProject::createDefault()
{
    StudioProject project;
    project.profileId = newId();
    project.profileName = QStringLiteral("My Studio");
    project.addScene(QStringLiteral("Gameplay"));
    project.addScene(QStringLiteral("Starting Soon"));
    project.addScene(QStringLiteral("BRB"));
    project.activeSceneId = project.scenes.first().id;
    return project;
}

const Scene *StudioProject::activeScene() const { return const_cast<StudioProject *>(this)->activeScene(); }
Scene *StudioProject::activeScene()
{
    const int index = sceneIndex(activeSceneId);
    return index >= 0 ? &scenes[index] : nullptr;
}

QString StudioProject::uniqueSceneName(const QString &name) const
{
    return uniqueName(name, QStringLiteral("New Scene"), scenes, [](const Scene &scene) { return scene.name; });
}

QString StudioProject::uniqueSourceName(const QString &name) const
{
    const QString base = name.trimmed().isEmpty() ? QStringLiteral("New Source") : name.trimmed();
    auto available = [&](const QString &candidate) {
        for (const Source &source : sources)
            if (source.name.compare(candidate, Qt::CaseInsensitive) == 0) return false;
        return true;
    };
    if (available(base)) return base;
    for (int suffix = 2;; ++suffix) {
        const QString candidate = QStringLiteral("%1 %2").arg(base).arg(suffix);
        if (available(candidate)) return candidate;
    }
}

int StudioProject::sceneIndex(const QString &sceneId) const
{
    for (qsizetype i = 0; i < scenes.size(); ++i) if (scenes[i].id == sceneId) return static_cast<int>(i);
    return -1;
}

int StudioProject::sourceIndex(const QString &sourceId) const
{
    for (qsizetype i = 0; i < sources.size(); ++i) if (sources[i].id == sourceId) return static_cast<int>(i);
    return -1;
}

Source *StudioProject::source(const QString &sourceId)
{
    const int index = sourceIndex(sourceId);
    return index >= 0 ? &sources[index] : nullptr;
}

const Source *StudioProject::source(const QString &sourceId) const { return const_cast<StudioProject *>(this)->source(sourceId); }

Scene &StudioProject::addScene(const QString &requestedName)
{
    scenes.append({newId(), uniqueSceneName(requestedName), {}});
    if (activeSceneId.isEmpty()) activeSceneId = scenes.last().id;
    return scenes.last();
}

bool StudioProject::removeScene(const QString &sceneId)
{
    const QString removedId = sceneId;
    const int index = sceneIndex(sceneId);
    if (index < 0 || scenes.size() <= 1) return false;
    scenes.removeAt(index);
    if (activeSceneId == removedId) activeSceneId = scenes[qMin(index, scenes.size() - 1)].id;
    normalize();
    return true;
}

bool StudioProject::moveScene(const QString &sceneId, int delta)
{
    const int from = sceneIndex(sceneId);
    const int to = from + delta;
    if (from < 0 || to < 0 || to >= scenes.size()) return false;
    scenes.move(from, to);
    return true;
}

bool StudioProject::renameScene(const QString &sceneId, const QString &requestedName)
{
    const int index = sceneIndex(sceneId);
    const QString trimmed = requestedName.trimmed();
    if (index < 0 || trimmed.isEmpty()) return false;
    const QString previous = scenes[index].name;
    scenes[index].name.clear();
    scenes[index].name = uniqueSceneName(trimmed);
    if (scenes[index].name.isEmpty()) scenes[index].name = previous;
    return true;
}

Source &StudioProject::addSource(SourceType type)
{
    const QString name = uniqueSourceName(sourceTypeName(type));
    sources.append({newId(), name, type, true, {}});
    if (type == SourceType::Color) sources.last().configuration.insert(QStringLiteral("color"), QStringLiteral("#496E78"));
    Scene *scene = activeScene();
    if (scene) scene->items.append({newId(), sources.last().id, {}, true, false, static_cast<int>(scene->items.size())});
    if (type == SourceType::Microphone || type == SourceType::DesktopAudio)
        mixerChannels.append({sources.last().id, name, 1.0, false});
    return sources.last();
}

bool StudioProject::removeSceneItem(const QString &sceneItemId)
{
    Scene *scene = activeScene();
    if (!scene) return false;
    for (qsizetype i = 0; i < scene->items.size(); ++i) {
        if (scene->items[i].id == sceneItemId) {
            const QString sourceId = scene->items[i].sourceId;
            scene->items.removeAt(i);
            bool sourceIsStillUsed = false;
            for (const Scene &candidate : scenes) {
                for (const SceneItem &item : candidate.items) {
                    if (item.sourceId == sourceId) { sourceIsStillUsed = true; break; }
                }
                if (sourceIsStillUsed) break;
            }
            if (!sourceIsStillUsed) {
                const int sourceToRemove = sourceIndex(sourceId);
                if (sourceToRemove >= 0) sources.removeAt(sourceToRemove);
            }
            normalize();
            return true;
        }
    }
    return false;
}

bool StudioProject::moveSceneItem(const QString &sceneItemId, int delta)
{
    Scene *scene = activeScene();
    if (!scene) return false;
    for (qsizetype i = 0; i < scene->items.size(); ++i) {
        if (scene->items[i].id == sceneItemId) {
            const int target = static_cast<int>(i) + delta;
            if (target < 0 || target >= scene->items.size()) return false;
            scene->items.move(i, target); normalize(); return true;
        }
    }
    return false;
}

bool StudioProject::moveSceneItemTo(const QString &sceneItemId, int targetIndex)
{
    Scene *scene = activeScene();
    if (!scene) return false;
    for (qsizetype i = 0; i < scene->items.size(); ++i) {
        if (scene->items[i].id != sceneItemId) continue;
        if (targetIndex < 0 || targetIndex >= scene->items.size() || targetIndex == i) return false;
        scene->items.move(i, targetIndex);
        normalize();
        return true;
    }
    return false;
}

bool StudioProject::setSceneItemVisible(const QString &id, bool visible)
{
    if (Scene *scene = activeScene()) for (auto &item : scene->items) if (item.id == id) { item.visible = visible; return true; }
    return false;
}

bool StudioProject::setSceneItemLocked(const QString &id, bool locked)
{
    if (Scene *scene = activeScene()) for (auto &item : scene->items) if (item.id == id) { item.locked = locked; return true; }
    return false;
}

bool StudioProject::setSceneItemTransform(const QString &sceneItemId, const Transform &transform)
{
    SceneItem *item = activeItem(*this, sceneItemId);
    if (!item) return false;
    item->transform = sanitizedTransform(transform);
    return true;
}

bool StudioProject::resetSceneItemTransform(const QString &sceneItemId)
{
    Transform transform;
    transform.width = programResolution.width;
    transform.height = programResolution.height;
    return setSceneItemTransform(sceneItemId, transform);
}

bool StudioProject::fitSceneItemToCanvas(const QString &sceneItemId)
{
    SceneItem *item = activeItem(*this, sceneItemId);
    if (!item) return false;
    Transform value = item->transform;
    const double sourceWidth = qMax(1.0, (value.width - value.cropLeft - value.cropRight) * value.scaleX);
    const double sourceHeight = qMax(1.0, (value.height - value.cropTop - value.cropBottom) * value.scaleY);
    const double scale = qMin(programResolution.width / sourceWidth, programResolution.height / sourceHeight);
    value.width = sourceWidth * scale;
    value.height = sourceHeight * scale;
    value.scaleX = 1.0;
    value.scaleY = 1.0;
    value.x = (programResolution.width - value.width) / 2.0;
    value.y = (programResolution.height - value.height) / 2.0;
    value.cropLeft = value.cropTop = value.cropRight = value.cropBottom = 0.0;
    return setSceneItemTransform(sceneItemId, value);
}

bool StudioProject::stretchSceneItemToCanvas(const QString &sceneItemId)
{
    Transform value;
    value.width = programResolution.width;
    value.height = programResolution.height;
    return setSceneItemTransform(sceneItemId, value);
}

bool StudioProject::centerSceneItem(const QString &sceneItemId, bool horizontal, bool vertical)
{
    SceneItem *item = activeItem(*this, sceneItemId);
    if (!item || (!horizontal && !vertical)) return false;
    Transform value = item->transform;
    const double renderedWidth = (value.width - value.cropLeft - value.cropRight) * value.scaleX;
    const double renderedHeight = (value.height - value.cropTop - value.cropBottom) * value.scaleY;
    if (horizontal) value.x = (programResolution.width - renderedWidth) / 2.0;
    if (vertical) value.y = (programResolution.height - renderedHeight) / 2.0;
    return setSceneItemTransform(sceneItemId, value);
}

bool StudioProject::rotateSceneItem(const QString &sceneItemId, double degrees)
{
    SceneItem *item = activeItem(*this, sceneItemId);
    if (!item) return false;
    Transform value = item->transform;
    value.rotation += degrees;
    return setSceneItemTransform(sceneItemId, value);
}

bool StudioProject::flipSceneItem(const QString &sceneItemId, bool horizontal)
{
    SceneItem *item = activeItem(*this, sceneItemId);
    if (!item) return false;
    if (horizontal) item->transform.flipHorizontal = !item->transform.flipHorizontal;
    else item->transform.flipVertical = !item->transform.flipVertical;
    return true;
}

bool StudioProject::renameSource(const QString &sourceId, const QString &requestedName)
{
    Source *value = source(sourceId);
    const QString trimmed = requestedName.trimmed();
    if (!value || trimmed.isEmpty()) return false;
    const QString old = value->name; value->name.clear(); value->name = uniqueSourceName(trimmed);
    if (value->name.isEmpty()) value->name = old;
    for (auto &channel : mixerChannels) if (channel.id == value->id) channel.name = value->name;
    return true;
}

bool StudioProject::setMixerVolume(const QString &id, double volume)
{
    for (auto &channel : mixerChannels) if (channel.id == id) { channel.volume = qBound(0.0, volume, 10.0); return true; }
    return false;
}

bool StudioProject::setMixerMuted(const QString &id, bool muted)
{
    for (auto &channel : mixerChannels) if (channel.id == id) { channel.muted = muted; return true; }
    return false;
}

void StudioProject::normalize()
{
    if (scenes.isEmpty()) addScene(QStringLiteral("Gameplay"));
    if (sceneIndex(activeSceneId) < 0) activeSceneId = scenes.first().id;
    for (auto &scene : scenes) for (qsizetype i = 0; i < scene.items.size(); ++i) scene.items[i].zOrder = static_cast<int>(i);
    QSet<QString> referencedSources;
    for (const auto &scene : scenes) for (const auto &item : scene.items) referencedSources.insert(item.sourceId);
    sources.removeIf([&](const Source &source) { return !referencedSources.contains(source.id); });
    // Mixer IDs are source IDs: legacy standalone demo channels have no owner.
    QVector<MixerChannel> channels;
    for (const Source &source : sources) {
        if (source.type != SourceType::Microphone && source.type != SourceType::DesktopAudio) continue;
        MixerChannel channel{source.id, source.name, 1.0, false};
        for (const auto &saved : mixerChannels) if (saved.id == source.id) { channel = saved; break; }
        channel.name = source.name;
        channels.append(channel);
    }
    mixerChannels = std::move(channels);
}
