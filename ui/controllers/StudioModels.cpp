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
void SceneListModel::refresh() { beginResetModel(); endResetModel(); }

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
void SceneItemListModel::refresh() { beginResetModel(); endResetModel(); }

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
void MixerListModel::refresh() { beginResetModel(); endResetModel(); }
