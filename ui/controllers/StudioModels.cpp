#include "StudioModels.h"
#include <algorithm>

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
    if (!index.isValid() || index.model() != this || index.column() != 0 || index.row() < 0 || index.row() >= project_->scenes.size()) return {};
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
    if (direction != -1 && direction != 1) return false;
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
    if (!index.isValid() || index.model() != this || index.column() != 0 || !item) return {};
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
    if (role == ScaleXRole) return item->transform.scaleX;
    if (role == ScaleYRole) return item->transform.scaleY;
    if (role == RotationRole) return item->transform.rotation;
    if (role == CropLeftRole) return item->transform.cropLeft;
    if (role == CropTopRole) return item->transform.cropTop;
    if (role == CropRightRole) return item->transform.cropRight;
    if (role == CropBottomRole) return item->transform.cropBottom;
    if (role == FlipHorizontalRole) return item->transform.flipHorizontal;
    if (role == FlipVerticalRole) return item->transform.flipVertical;
    return {};
}
QHash<int, QByteArray> SceneItemListModel::roleNames() const { return {{IdRole, "itemId"}, {SourceIdRole, "sourceId"}, {NameRole, "name"}, {TypeRole, "type"}, {ItemVisibleRole, "itemVisible"}, {ItemLockedRole, "itemLocked"}, {ZOrderRole, "zOrder"}, {SelectedRole, "selected"}, {VisualRole, "visual"}, {XRole, "programX"}, {YRole, "programY"}, {WidthRole, "programWidth"}, {HeightRole, "programHeight"}, {ScaleXRole, "programScaleX"}, {ScaleYRole, "programScaleY"}, {RotationRole, "programRotation"}, {CropLeftRole, "cropLeft"}, {CropTopRole, "cropTop"}, {CropRightRole, "cropRight"}, {CropBottomRole, "cropBottom"}, {FlipHorizontalRole, "flipHorizontal"}, {FlipVerticalRole, "flipVertical"}}; }
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
    if (row < rowCount()) emit dataChanged(index(row), index(rowCount() - 1), {ZOrderRole});
    return removed;
}
bool SceneItemListModel::moveItem(const QString &itemId, int direction)
{
    if (direction != -1 && direction != 1) return false;
    const int from = itemRow(itemId);
    const int to = from + direction;
    if (from < 0 || to < 0 || to >= rowCount()) return false;
    const int destination = to > from ? to + 1 : to;
    beginMoveRows({}, from, from, {}, destination);
    const bool moved = project_->moveSceneItem(itemId, direction);
    endMoveRows();
    emit dataChanged(index(qMin(from, to)), index(qMax(from, to)), {ZOrderRole});
    return moved;
}
bool SceneItemListModel::moveItemTo(const QString &itemId, int targetIndex)
{
    const int from = itemRow(itemId);
    if (from < 0 || targetIndex < 0 || targetIndex >= rowCount() || targetIndex == from) return false;
    const int destination = targetIndex > from ? targetIndex + 1 : targetIndex;
    beginMoveRows({}, from, from, {}, destination);
    const bool moved = project_->moveSceneItemTo(itemId, targetIndex);
    endMoveRows();
    if (moved) emit dataChanged(index(qMin(from, targetIndex)), index(qMax(from, targetIndex)), {ZOrderRole});
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
bool SceneItemListModel::setItemTransform(const QString &itemId, const Transform &transform)
{
    const int row = itemRow(itemId);
    if (row < 0 || !project_->setSceneItemTransform(itemId, transform)) return false;
    emit dataChanged(index(row), index(row), {XRole, YRole, WidthRole, HeightRole, ScaleXRole, ScaleYRole, RotationRole, CropLeftRole, CropTopRole, CropRightRole, CropBottomRole, FlipHorizontalRole, FlipVerticalRole});
    return true;
}
bool SceneItemListModel::applyTransformAction(const QString &itemId, const QString &action)
{
    bool changed = false;
    if (action == QStringLiteral("reset")) changed = project_->resetSceneItemTransform(itemId);
    else if (action == QStringLiteral("fit")) changed = project_->fitSceneItemToCanvas(itemId);
    else if (action == QStringLiteral("stretch")) changed = project_->stretchSceneItemToCanvas(itemId);
    else if (action == QStringLiteral("center")) changed = project_->centerSceneItem(itemId, true, true);
    else if (action == QStringLiteral("centerHorizontal")) changed = project_->centerSceneItem(itemId, true, false);
    else if (action == QStringLiteral("centerVertical")) changed = project_->centerSceneItem(itemId, false, true);
    else if (action == QStringLiteral("rotate90Clockwise")) changed = project_->rotateSceneItem(itemId, 90.0);
    else if (action == QStringLiteral("rotate90CounterClockwise")) changed = project_->rotateSceneItem(itemId, -90.0);
    else if (action == QStringLiteral("flipHorizontal")) changed = project_->flipSceneItem(itemId, true);
    else if (action == QStringLiteral("flipVertical")) changed = project_->flipSceneItem(itemId, false);
    const int row = itemRow(itemId);
    if (changed && row >= 0) emit dataChanged(index(row), index(row), {XRole, YRole, WidthRole, HeightRole, ScaleXRole, ScaleYRole, RotationRole, CropLeftRole, CropTopRole, CropRightRole, CropBottomRole, FlipHorizontalRole, FlipVerticalRole});
    return changed;
}
void SceneItemListModel::beginSceneChange() { beginResetModel(); }
void SceneItemListModel::endSceneChange() { endResetModel(); }
void SceneItemListModel::notifySelectionChanged(const QString &previousId, const QString &currentId)
{
    for (const QString &id : {previousId, currentId}) {
        const int row = itemRow(id);
        if (row >= 0) emit dataChanged(index(row), index(row), {SelectedRole});
    }
}

MixerListModel::MixerListModel(StudioProject *project, QObject *parent) : QAbstractListModel(parent), project_(project) { synchronize(); }
int MixerListModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : channels_.size(); }
QVariant MixerListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this || index.column() != 0 || index.row() < 0 || index.row() >= channels_.size()) return {};
    const MixerChannel &channel = channels_[index.row()];
    if (role == IdRole) return channel.id;
    if (role == NameRole) return channel.name;
    if (role == VolumeRole) return channel.volume;
    if (role == MutedRole) return channel.muted;
    if (role == LevelDbRole) return levels_.value(channel.id, -90.0f);
    return {};
}
QHash<int, QByteArray> MixerListModel::roleNames() const { return {{IdRole, "channelId"}, {NameRole, "name"}, {VolumeRole, "volume"}, {MutedRole, "muted"}, {LevelDbRole, "levelDb"}}; }
bool MixerListModel::setVolume(const QString &id, double volume)
{
    if (!project_->setMixerVolume(id, volume)) return false;
    synchronize();
    return true;
}
bool MixerListModel::setMuted(const QString &id, bool muted)
{
    if (!project_->setMixerMuted(id, muted)) return false;
    synchronize();
    return true;
}

void MixerListModel::synchronize()
{
    QVector<MixerChannel> wanted;
    if (const Scene *scene = project_->activeScene()) {
        for (const auto &channel : project_->mixerChannels) {
            for (const auto &item : scene->items) {
                if (item.sourceId == channel.id) { wanted.append(channel); break; }
            }
        }
    }
    for (int row = static_cast<int>(channels_.size()) - 1; row >= 0; --row) {
        const auto found = std::find_if(wanted.begin(), wanted.end(), [&](const auto &channel) { return channel.id == channels_[row].id; });
        if (found != wanted.end()) continue;
        beginRemoveRows({}, row, row);
        channels_.removeAt(row);
        endRemoveRows();
    }
    for (int row = 0; row < wanted.size(); ++row) {
        if (row >= channels_.size() || channels_[row].id != wanted[row].id) {
            beginInsertRows({}, row, row);
            channels_.insert(row, wanted[row]);
            endInsertRows();
        } else if (channels_[row].name != wanted[row].name || channels_[row].volume != wanted[row].volume || channels_[row].muted != wanted[row].muted) {
            channels_[row] = wanted[row];
            emit dataChanged(index(row), index(row), {NameRole, VolumeRole, MutedRole});
        }
    }
}

void MixerListModel::setLevel(const QString &id, const float levelDb)
{
    levels_.insert(id, levelDb);
    for (int row = 0; row < channels_.size(); ++row) if (channels_[row].id == id) { emit dataChanged(index(row), index(row), {LevelDbRole}); break; }
}