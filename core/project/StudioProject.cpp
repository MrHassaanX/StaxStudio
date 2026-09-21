#include "StudioProject.h"

#include <QUuid>

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
    project.mixerChannels = {{newId(), QStringLiteral("Desktop Audio"), 0.8, false},
                             {newId(), QStringLiteral("Microphone"), 0.8, false}};
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
    return uniqueName(name, QStringLiteral("New Source"), sources, [](const Source &source) { return source.name; });
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
    const int index = sceneIndex(sceneId);
    if (index < 0 || scenes.size() <= 1) return false;
    scenes.removeAt(index);
    if (activeSceneId == sceneId) activeSceneId = scenes[qMin(index, scenes.size() - 1)].id;
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
    Scene *scene = activeScene();
    if (scene) scene->items.append({newId(), sources.last().id, {}, true, false, static_cast<int>(scene->items.size())});
    return sources.last();
}

bool StudioProject::removeSceneItem(const QString &sceneItemId)
{
    Scene *scene = activeScene();
    if (!scene) return false;
    for (qsizetype i = 0; i < scene->items.size(); ++i) {
        if (scene->items[i].id == sceneItemId) { scene->items.removeAt(i); normalize(); return true; }
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

bool StudioProject::renameSource(const QString &sourceId, const QString &requestedName)
{
    Source *value = source(sourceId);
    const QString trimmed = requestedName.trimmed();
    if (!value || trimmed.isEmpty()) return false;
    const QString old = value->name; value->name.clear(); value->name = uniqueSourceName(trimmed);
    if (value->name.isEmpty()) value->name = old;
    return true;
}

bool StudioProject::setMixerVolume(const QString &id, double volume)
{
    for (auto &channel : mixerChannels) if (channel.id == id) { channel.volume = qBound(0.0, volume, 1.0); return true; }
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
}
