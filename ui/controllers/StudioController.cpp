#include "StudioController.h"

#include "core/render/CompositorScene.h"

namespace {
QVariantMap transformValues(const Transform &value)
{
    return {{"x", value.x}, {"y", value.y}, {"width", value.width}, {"height", value.height},
            {"scaleX", value.scaleX}, {"scaleY", value.scaleY}, {"rotation", value.rotation},
            {"cropLeft", value.cropLeft}, {"cropTop", value.cropTop}, {"cropRight", value.cropRight},
            {"cropBottom", value.cropBottom}, {"flipHorizontal", value.flipHorizontal}, {"flipVertical", value.flipVertical}};
}
}

StudioController::StudioController(QObject *parent)
    : StudioController(QString{}, parent)
{
}

StudioController::StudioController(const QString &storageDirectory, QObject *parent)
    : QObject(parent), repository_(storageDirectory), project_(repository_.load()), scenesModel_(&project_, this),
      sceneItemsModel_(&project_, &selectedItemId_, this), mixerModel_(&project_, this),
      statusMessage_(QStringLiteral("Studio setup is saved locally. Media capture is not connected yet."))
{
}

QAbstractItemModel *StudioController::scenesModel() { return &scenesModel_; }
QAbstractItemModel *StudioController::sceneItemsModel() { return &sceneItemsModel_; }
QAbstractItemModel *StudioController::mixerModel() { return &mixerModel_; }
QString StudioController::activeSceneName() const { const Scene *scene = project_.activeScene(); return scene ? scene->name : QStringLiteral("Untitled Scene"); }
int StudioController::activeSceneIndex() const { return project_.sceneIndex(project_.activeSceneId); }
QString StudioController::selectedItemId() const { return selectedItemId_; }
QString StudioController::profileName() const { return project_.profileName; }
QString StudioController::transitionType() const { return project_.transition.type == TransitionType::Cut ? QStringLiteral("Cut") : QStringLiteral("Fade"); }
int StudioController::transitionDurationMs() const { return project_.transition.durationMs; }
int StudioController::programWidth() const { return project_.programResolution.width; }
int StudioController::programHeight() const { return project_.programResolution.height; }
QVariantList StudioController::compositorLayers() const
{
    QVariantList values;
    for (const CompositorLayer &layer : CompositorScene::activeLayers(project_)) {
        values.append(QVariantMap{{"itemId", layer.sceneItemId}, {"x", layer.transform.x}, {"y", layer.transform.y},
                                  {"width", layer.transform.width}, {"height", layer.transform.height},
                                  {"scaleX", layer.transform.scaleX}, {"scaleY", layer.transform.scaleY},
                                  {"rotation", layer.transform.rotation}, {"cropLeft", layer.transform.cropLeft},
                                  {"cropTop", layer.transform.cropTop}, {"cropRight", layer.transform.cropRight},
                                  {"cropBottom", layer.transform.cropBottom}, {"flipHorizontal", layer.transform.flipHorizontal},
                                  {"flipVertical", layer.transform.flipVertical}, {"color", layer.frame.color}, {"zOrder", layer.zOrder}});
    }
    return values;
}
QString StudioController::statusMessage() const { return statusMessage_; }

void StudioController::addScene(const QString &name)
{
    if (name.trimmed().isEmpty()) return;
    const QString id = scenesModel_.addScene(name);
    selectScene(id);
}
void StudioController::renameScene(const QString &id, const QString &name) { if (scenesModel_.renameScene(id, name)) refresh(); }
void StudioController::deleteScene(const QString &id)
{
    if (project_.sceneIndex(id) < 0 || project_.scenes.size() <= 1) return;
    const bool active = project_.activeSceneId == id;
    if (active) sceneItemsModel_.beginSceneChange();
    scenesModel_.removeScene(id);
    if (active) {
        selectedItemId_.clear();
        sceneItemsModel_.endSceneChange();
        emit selectedItemChanged();
    }
    scenesModel_.notifyActiveSceneChanged();
    refresh();
}
void StudioController::selectScene(const QString &id)
{
    if (project_.sceneIndex(id) < 0 || project_.activeSceneId == id) return;
    sceneItemsModel_.beginSceneChange();
    project_.activeSceneId = id;
    selectedItemId_.clear();
    sceneItemsModel_.endSceneChange();
    emit selectedItemChanged();
    scenesModel_.notifyActiveSceneChanged();
    refresh();
}
void StudioController::moveScene(const QString &id, int direction) { if (scenesModel_.moveScene(id, direction)) refresh(); }
void StudioController::addSource(const QString &typeName, const QString &name)
{
    bool ok = false;
    const SourceType type = sourceTypeFromName(typeName, &ok);
    if (!ok) return;
    sceneItemsModel_.addSource(type, name);
    refresh();
}
void StudioController::renameSource(const QString &id, const QString &name) { if (sceneItemsModel_.renameSource(id, name)) refresh(); }
void StudioController::removeSceneItem(const QString &id)
{
    if (!sceneItemsModel_.removeItem(id)) return;
    if (selectedItemId_ == id) { selectedItemId_.clear(); emit selectedItemChanged(); }
    refresh();
}
void StudioController::moveSceneItem(const QString &id, int direction) { if (sceneItemsModel_.moveItem(id, direction)) refresh(); }
void StudioController::moveSceneItemTo(const QString &id, int targetIndex) { if (sceneItemsModel_.moveItemTo(id, targetIndex)) refresh(); }
void StudioController::setItemVisible(const QString &id, bool visible) { if (sceneItemsModel_.setItemVisible(id, visible)) refresh(); }
void StudioController::setItemLocked(const QString &id, bool locked) { if (sceneItemsModel_.setItemLocked(id, locked)) refresh(); }
void StudioController::selectItem(const QString &id)
{
    if (selectedItemId_ == id) return;
    const Scene *scene = project_.activeScene();
    if (!scene) return;
    bool found = id.isEmpty();
    for (const auto &item : scene->items) if (item.id == id) found = true;
    if (!found) return;
    const QString previousId = selectedItemId_;
    selectedItemId_ = id;
    sceneItemsModel_.notifySelectionChanged(previousId, selectedItemId_);
    emit selectedItemChanged();
}
QVariantMap StudioController::itemTransform(const QString &itemId) const
{
    const Scene *scene = project_.activeScene();
    if (!scene) return {};
    for (const SceneItem &item : scene->items) if (item.id == itemId) return transformValues(item.transform);
    return {};
}
void StudioController::setItemTransform(const QString &itemId, const QVariantMap &values)
{
    Transform value;
    const QVariantMap existing = itemTransform(itemId);
    if (existing.isEmpty()) return;
    value.x = values.value("x", existing.value("x")).toDouble();
    value.y = values.value("y", existing.value("y")).toDouble();
    value.width = values.value("width", existing.value("width")).toDouble();
    value.height = values.value("height", existing.value("height")).toDouble();
    value.scaleX = values.value("scaleX", existing.value("scaleX")).toDouble();
    value.scaleY = values.value("scaleY", existing.value("scaleY")).toDouble();
    value.rotation = values.value("rotation", existing.value("rotation")).toDouble();
    value.cropLeft = values.value("cropLeft", existing.value("cropLeft")).toDouble();
    value.cropTop = values.value("cropTop", existing.value("cropTop")).toDouble();
    value.cropRight = values.value("cropRight", existing.value("cropRight")).toDouble();
    value.cropBottom = values.value("cropBottom", existing.value("cropBottom")).toDouble();
    value.flipHorizontal = values.value("flipHorizontal", existing.value("flipHorizontal")).toBool();
    value.flipVertical = values.value("flipVertical", existing.value("flipVertical")).toBool();
    if (sceneItemsModel_.setItemTransform(itemId, value)) refresh();
}
void StudioController::previewItemTransform(const QString &itemId, const QVariantMap &values)
{
    Transform value;
    const QVariantMap existing = itemTransform(itemId);
    if (existing.isEmpty()) return;
    value.x = values.value("x", existing.value("x")).toDouble();
    value.y = values.value("y", existing.value("y")).toDouble();
    value.width = values.value("width", existing.value("width")).toDouble();
    value.height = values.value("height", existing.value("height")).toDouble();
    value.scaleX = values.value("scaleX", existing.value("scaleX")).toDouble();
    value.scaleY = values.value("scaleY", existing.value("scaleY")).toDouble();
    value.rotation = values.value("rotation", existing.value("rotation")).toDouble();
    value.cropLeft = values.value("cropLeft", existing.value("cropLeft")).toDouble();
    value.cropTop = values.value("cropTop", existing.value("cropTop")).toDouble();
    value.cropRight = values.value("cropRight", existing.value("cropRight")).toDouble();
    value.cropBottom = values.value("cropBottom", existing.value("cropBottom")).toDouble();
    value.flipHorizontal = values.value("flipHorizontal", existing.value("flipHorizontal")).toBool();
    value.flipVertical = values.value("flipVertical", existing.value("flipVertical")).toBool();
    if (sceneItemsModel_.setItemTransform(itemId, value)) emit projectChanged();
}
void StudioController::commitPreviewTransform() { save(); emit projectChanged(); }
void StudioController::applyTransformAction(const QString &itemId, const QString &action) { if (sceneItemsModel_.applyTransformAction(itemId, action)) refresh(); }
void StudioController::removeSourceFromActiveScene(const QString &sourceId)
{
    const Scene *scene = project_.activeScene();
    if (!scene) return;
    for (const SceneItem &item : scene->items) if (item.sourceId == sourceId) { removeSceneItem(item.id); return; }
}
void StudioController::setMixerVolume(const QString &id, double volume) { if (mixerModel_.setVolume(id, volume)) refresh(); }
void StudioController::setMixerMuted(const QString &id, bool muted) { if (mixerModel_.setMuted(id, muted)) refresh(); }
void StudioController::setTransitionType(const QString &type) { project_.transition.type = type == QStringLiteral("Cut") ? TransitionType::Cut : TransitionType::Fade; refresh(); }
void StudioController::setTransitionDurationMs(int value) { project_.transition.durationMs = qBound(50, value, 10000); refresh(); }
void StudioController::setProfileName(const QString &name) { const QString trimmed = name.trimmed(); if (!trimmed.isEmpty()) { project_.profileName = trimmed; refresh(); } }
void StudioController::showUnavailableAction(const QString &action) { statusMessage_ = QStringLiteral("%1 is planned for the media milestone. Your studio configuration is safe.").arg(action); emit statusMessageChanged(); }

void StudioController::refresh()
{
    mixerModel_.synchronize();
    save();
    emit projectChanged();
}

void StudioController::save()
{
    QString error;
    if (!repository_.save(project_, &error)) { statusMessage_ = QStringLiteral("Unable to save studio changes: %1").arg(error); emit statusMessageChanged(); }
}
