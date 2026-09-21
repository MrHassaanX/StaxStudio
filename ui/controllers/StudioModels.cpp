#include "StudioModels.h"

namespace {
const SceneItem *itemAt(const StudioProject *project, int row)
{
    const Scene *scene = project->activeScene();
    return scene && row >= 0 && row < scene->items.size() ? &scene->items[row] : nullptr;
}
}

SceneListModel::SceneListModel(StudioProject *project, QObject *parent) : QAbstractListModel(parent), project_(project) {}
int SceneListModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : project_->scenes.size(); }
QVariant SceneListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= project_->scenes.size()) return {};
    const Scene &scene = project_->scenes[index.row()];
    if (role == IdRole) return scene.id;
    if (role == NameRole) return scene.name;
    if (role == ActiveRole) return scene.id == project_->activeSceneId;
    return {};
}
QHash<int, QByteArray> SceneListModel::roleNames() const { return {{IdRole, "sceneId"}, {NameRole, "name"}, {ActiveRole, "active"}}; }
QString SceneListModel::addScene(const QString &name)
{
    const int row = project_->scenes.size();
    beginInsertRows({}, row, row);
    const QString id = project_->addScene(name).id;
    endInsertRows();
    return id;
}
bool SceneListModel::renameScene(const QString &id, const QString &name)
{
    const int row = project_->sceneIndex(id);
    if (row < 0 || !project_->renameScene(id, name)) return false;
    emit dataChanged(index(row), index(row), {NameRole});
    return true;
}
bool SceneListModel::removeScene(const QString &id)
{
    const int row = project_->sceneIndex(id);
    if (row < 0 || project_->scenes.size() <= 1) return false;
    beginRemoveRows({}, row, row);
    const bool removed = project_->removeScene(id);
    endRemoveRows();
    return removed;
}
bool SceneListModel::moveScene(const QString &id, int direction)
{
    const int from = project_->sceneIndex(id);
    const int to = from + direction;
    if (from < 0 || to < 0 || to >= project_->scenes.size()) return false;
    const int destination = to > from ? to + 1 : to;
    beginMoveRows({}, from, from, {}, destination);
    const bool moved = project_->moveScene(id, direction);
    endMoveRows();
    return moved;
}
void SceneListModel::notifyActiveSceneChanged()
{
    if (rowCount() > 0) emit dataChanged(index(0), index(rowCount() - 1), {ActiveRole});
}

SceneItemListModel::SceneItemListModel(StudioProject *project, QString *selectedItemId, QObject *parent) : QAbstractListModel(parent), project_(project), selectedItemId_(selectedItemId) {}
int SceneItemListModel::rowCount(const QModelIndex &parent) const { const Scene *scene = project_->activeScene(); return parent.isValid() || !scene ? 0 : scene->items.size(); }
QVariant SceneItemListModel::data(const QModelIndex &index, int role) const
{
    const SceneItem *item = itemAt(project_, index.row());
    if (!index.isValid() || !item) return {};
    const Source *source = project_->source(item->sourceId);
    if (role == IdRole) return item->id;
    if (role == SourceIdRole) return item->sourceId;
    if (role == NameRole) return source ? source->name : QStringLiteral("Missing source");
    if (role == TypeRole) return source ? sourceTypeName(source->type) : QStringLiteral("Unavailable");
    if (role == ItemVisibleRole) return item->visible;
    if (role == ItemLockedRole) return item->locked;
    if (role == ZOrderRole) return item->zOrder;
    if (role == SelectedRole) return item->id == *selectedItemId_;
    if (role == VisualRole) return source && source->type != SourceType::Microphone && source->type != SourceType::DesktopAudio;
    if (role == XRole) return item->transform.x;
    if (role == YRole) return item->transform.y;
    if (role == WidthRole) return item->transform.width;
    if (role == HeightRole) return item->transform.height;
    return {};
}
QHash<int, QByteArray> SceneItemListModel::roleNames() const { return {{IdRole, "itemId"}, {SourceIdRole, "sourceId"}, {NameRole, "name"}, {TypeRole, "type"}, {ItemVisibleRole, "itemVisible"}, {ItemLockedRole, "itemLocked"}, {ZOrderRole, "zOrder"}, {SelectedRole, "selected"}, {VisualRole, "visual"}, {XRole, "programX"}, {YRole, "programY"}, {WidthRole, "programWidth"}, {HeightRole, "programHeight"}}; }
int SceneItemListModel::itemRow(const QString &itemId) const
{
    const Scene *scene = project_->activeScene();
    if (!scene) return -1;
    for (qsizetype row = 0; row < scene->items.size(); ++row) {
        if (scene->items[row].id == itemId) return static_cast<int>(row);
    }
    return -1;
}
QVector<int> SceneItemListModel::itemRowsForSource(const QString &sourceId) const
{
    QVector<int> rows;
    const Scene *scene = project_->activeScene();
    if (!scene) return rows;
    for (qsizetype row = 0; row < scene->items.size(); ++row) {
        if (scene->items[row].sourceId == sourceId) rows.append(static_cast<int>(row));
    }
    return rows;
}
QString SceneItemListModel::addSource(SourceType type, const QString &name)
{
    const Scene *scene = project_->activeScene();
    if (!scene) return {};
    const int row = scene->items.size();
    beginInsertRows({}, row, row);
    Source &source = project_->addSource(type);
    if (!name.trimmed().isEmpty()) project_->renameSource(source.id, name);
    const QString itemId = project_->activeScene()->items.last().id;
    endInsertRows();
    return itemId;
}
bool SceneItemListModel::renameSource(const QString &sourceId, const QString &name)
{
    if (!project_->renameSource(sourceId, name)) return false;
    for (const int row : itemRowsForSource(sourceId)) emit dataChanged(index(row), index(row), {NameRole});
    return true;
}
bool SceneItemListModel::removeItem(const QString &itemId)
{
    const int row = itemRow(itemId);
    if (row < 0) return false;
    beginRemoveRows({}, row, row);
    const bool removed = project_->removeSceneItem(itemId);
    endRemoveRows();
    return removed;
}
bool SceneItemListModel::moveItem(const QString &itemId, int direction)
{
    const int from = itemRow(itemId);
    const int to = from + direction;
    if (from < 0 || to < 0 || to >= rowCount()) return false;
    const int destination = to > from ? to + 1 : to;
    beginMoveRows({}, from, from, {}, destination);
    const bool moved = project_->moveSceneItem(itemId, direction);
    endMoveRows();
    return moved;
}
bool SceneItemListModel::setItemVisible(const QString &itemId, bool visible)
{
    const int row = itemRow(itemId);
    if (row < 0 || !project_->setSceneItemVisible(itemId, visible)) return false;
    emit dataChanged(index(row), index(row), {ItemVisibleRole});
    return true;
}
bool SceneItemListModel::setItemLocked(const QString &itemId, bool locked)
{
    const int row = itemRow(itemId);
    if (row < 0 || !project_->setSceneItemLocked(itemId, locked)) return false;
    emit dataChanged(index(row), index(row), {ItemLockedRole});
    return true;
}
void SceneItemListModel::resetForActiveScene() { beginResetModel(); endResetModel(); }
void SceneItemListModel::notifySelectionChanged(const QString &previousId, const QString &currentId)
{
    for (const QString &id : {previousId, currentId}) {
        const int row = itemRow(id);
        if (row >= 0) emit dataChanged(index(row), index(row), {SelectedRole});
    }
}

MixerListModel::MixerListModel(StudioProject *project, QObject *parent) : QAbstractListModel(parent), project_(project) {}
int MixerListModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : project_->mixerChannels.size(); }
QVariant MixerListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= project_->mixerChannels.size()) return {};
    const MixerChannel &channel = project_->mixerChannels[index.row()];
    if (role == IdRole) return channel.id;
    if (role == NameRole) return channel.name;
    if (role == VolumeRole) return channel.volume;
    if (role == MutedRole) return channel.muted;
    return {};
}
QHash<int, QByteArray> MixerListModel::roleNames() const { return {{IdRole, "channelId"}, {NameRole, "name"}, {VolumeRole, "volume"}, {MutedRole, "muted"}}; }
bool MixerListModel::setVolume(const QString &id, double volume)
{
    for (qsizetype row = 0; row < project_->mixerChannels.size(); ++row) {
        if (project_->mixerChannels[row].id != id) continue;
        if (!project_->setMixerVolume(id, volume)) return false;
        emit dataChanged(index(static_cast<int>(row)), index(static_cast<int>(row)), {VolumeRole});
        return true;
    }
    return false;
}
bool MixerListModel::setMuted(const QString &id, bool muted)
{
    for (qsizetype row = 0; row < project_->mixerChannels.size(); ++row) {
        if (project_->mixerChannels[row].id != id) continue;
        if (!project_->setMixerMuted(id, muted)) return false;
        emit dataChanged(index(static_cast<int>(row)), index(static_cast<int>(row)), {MutedRole});
        return true;
    }
    return false;
}
