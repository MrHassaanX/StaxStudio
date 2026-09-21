#include "StudioController.h"

StudioController::StudioController(QObject *parent)
    : QObject(parent), project_(repository_.load()), scenesModel_(&project_, this),
      sceneItemsModel_(&project_, &selectedItemId_, this), mixerModel_(&project_, this),
      statusMessage_(QStringLiteral("Studio setup is saved locally. Media capture is not connected yet."))
{
}

QAbstractItemModel *StudioController::scenesModel() { return &scenesModel_; }
QAbstractItemModel *StudioController::sceneItemsModel() { return &sceneItemsModel_; }
QAbstractItemModel *StudioController::mixerModel() { return &mixerModel_; }
QString StudioController::activeSceneName() const { const Scene *scene = project_.activeScene(); return scene ? scene->name : QStringLiteral("Untitled Scene"); }
QString StudioController::selectedItemId() const { return selectedItemId_; }
QString StudioController::profileName() const { return project_.profileName; }
QString StudioController::transitionType() const { return project_.transition.type == TransitionType::Cut ? QStringLiteral("Cut") : QStringLiteral("Fade"); }
int StudioController::transitionDurationMs() const { return project_.transition.durationMs; }
QString StudioController::statusMessage() const { return statusMessage_; }

void StudioController::addScene(const QString &name) { scenesModel_.addScene(name); refresh(); }
void StudioController::renameScene(const QString &id, const QString &name) { if (scenesModel_.renameScene(id, name)) refresh(); }
void StudioController::deleteScene(const QString &id)
{
    const QString previousActiveSceneId = project_.activeSceneId;
    if (!scenesModel_.removeScene(id)) { showUnavailableAction(QStringLiteral("Keep at least one scene in the studio.")); return; }
    if (selectedItemId_.isEmpty() == false) { selectedItemId_.clear(); emit selectedItemChanged(); }
    scenesModel_.notifyActiveSceneChanged();
    if (project_.activeSceneId != previousActiveSceneId) sceneItemsModel_.resetForActiveScene();
    refresh();
}
void StudioController::selectScene(const QString &id)
{
    if (project_.sceneIndex(id) < 0 || project_.activeSceneId == id) return;
    project_.activeSceneId = id;
    if (selectedItemId_.isEmpty() == false) { selectedItemId_.clear(); emit selectedItemChanged(); }
    scenesModel_.notifyActiveSceneChanged();
    sceneItemsModel_.resetForActiveScene();
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
void StudioController::setItemVisible(const QString &id, bool visible) { if (sceneItemsModel_.setItemVisible(id, visible)) refresh(); }
void StudioController::setItemLocked(const QString &id, bool locked) { if (sceneItemsModel_.setItemLocked(id, locked)) refresh(); }
void StudioController::selectItem(const QString &id)
{
    if (selectedItemId_ == id) return;
    const QString previousId = selectedItemId_;
    selectedItemId_ = id;
    sceneItemsModel_.notifySelectionChanged(previousId, selectedItemId_);
    emit selectedItemChanged();
}
void StudioController::setMixerVolume(const QString &id, double volume) { if (mixerModel_.setVolume(id, volume)) refresh(); }
void StudioController::setMixerMuted(const QString &id, bool muted) { if (mixerModel_.setMuted(id, muted)) refresh(); }
void StudioController::setTransitionType(const QString &type) { project_.transition.type = type == QStringLiteral("Cut") ? TransitionType::Cut : TransitionType::Fade; refresh(); }
void StudioController::setTransitionDurationMs(int value) { project_.transition.durationMs = qBound(50, value, 10000); refresh(); }
void StudioController::setProfileName(const QString &name) { const QString trimmed = name.trimmed(); if (!trimmed.isEmpty()) { project_.profileName = trimmed; refresh(); } }
void StudioController::showUnavailableAction(const QString &action) { statusMessage_ = QStringLiteral("%1 is planned for the media milestone. Your studio configuration is safe.").arg(action); emit statusMessageChanged(); }

void StudioController::refresh(bool sceneChanged)
{
    Q_UNUSED(sceneChanged)
    save();
    emit projectChanged();
}

void StudioController::save()
{
    QString error;
    if (!repository_.save(project_, &error)) { statusMessage_ = QStringLiteral("Unable to save studio changes: %1").arg(error); emit statusMessageChanged(); }
}
