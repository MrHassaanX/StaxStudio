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

void StudioController::addScene(const QString &name) { project_.addScene(name); refresh(true); }
void StudioController::renameScene(const QString &id, const QString &name) { if (project_.renameScene(id, name)) refresh(); }
void StudioController::deleteScene(const QString &id) { if (project_.removeScene(id)) { selectedItemId_.clear(); refresh(true); emit selectedItemChanged(); } else showUnavailableAction(QStringLiteral("Keep at least one scene in the studio.")); }
void StudioController::selectScene(const QString &id) { if (project_.sceneIndex(id) >= 0 && project_.activeSceneId != id) { project_.activeSceneId = id; selectedItemId_.clear(); refresh(true); emit selectedItemChanged(); } }
void StudioController::moveScene(const QString &id, int direction) { if (project_.moveScene(id, direction)) refresh(); }
void StudioController::addSource(const QString &typeName) { bool ok = false; const SourceType type = sourceTypeFromName(typeName, &ok); if (!ok) return; project_.addSource(type); refresh(true); }
void StudioController::renameSource(const QString &id, const QString &name) { if (project_.renameSource(id, name)) refresh(); }
void StudioController::removeSceneItem(const QString &id) { if (project_.removeSceneItem(id)) { if (selectedItemId_ == id) { selectedItemId_.clear(); emit selectedItemChanged(); } refresh(true); } }
void StudioController::moveSceneItem(const QString &id, int direction) { if (project_.moveSceneItem(id, direction)) refresh(true); }
void StudioController::setItemVisible(const QString &id, bool visible) { if (project_.setSceneItemVisible(id, visible)) refresh(true); }
void StudioController::setItemLocked(const QString &id, bool locked) { if (project_.setSceneItemLocked(id, locked)) refresh(true); }
void StudioController::selectItem(const QString &id) { if (selectedItemId_ == id) return; selectedItemId_ = id; sceneItemsModel_.refresh(); emit selectedItemChanged(); }
void StudioController::setMixerVolume(const QString &id, double volume) { if (project_.setMixerVolume(id, volume)) refresh(); }
void StudioController::setMixerMuted(const QString &id, bool muted) { if (project_.setMixerMuted(id, muted)) refresh(); }
void StudioController::setTransitionType(const QString &type) { project_.transition.type = type == QStringLiteral("Cut") ? TransitionType::Cut : TransitionType::Fade; refresh(); }
void StudioController::setTransitionDurationMs(int value) { project_.transition.durationMs = qBound(0, value, 10000); refresh(); }
void StudioController::setProfileName(const QString &name) { const QString trimmed = name.trimmed(); if (!trimmed.isEmpty()) { project_.profileName = trimmed; refresh(); } }
void StudioController::showUnavailableAction(const QString &action) { statusMessage_ = QStringLiteral("%1 is planned for the media milestone. Your studio configuration is safe.").arg(action); emit statusMessageChanged(); }

void StudioController::refresh(bool sceneChanged)
{
    project_.normalize(); save(); scenesModel_.refresh(); mixerModel_.refresh();
    if (sceneChanged) sceneItemsModel_.refresh(); else sceneItemsModel_.refresh();
    emit projectChanged();
}

void StudioController::save()
{
    QString error;
    if (!repository_.save(project_, &error)) { statusMessage_ = QStringLiteral("Unable to save studio changes: %1").arg(error); emit statusMessageChanged(); }
}
