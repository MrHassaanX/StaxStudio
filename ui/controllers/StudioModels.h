#pragma once

#include "core/project/StudioProject.h"

#include <QAbstractListModel>

class SceneListModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role { IdRole = Qt::UserRole + 1, NameRole, ActiveRole };
    explicit SceneListModel(StudioProject *project, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QString addScene(const QString &name);
    bool renameScene(const QString &id, const QString &name);
    bool removeScene(const QString &id);
    bool moveScene(const QString &id, int direction);
    void notifyActiveSceneChanged();
private:
    StudioProject *project_;
};

class SceneItemListModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role { IdRole = Qt::UserRole + 1, SourceIdRole, NameRole, TypeRole, ItemVisibleRole, ItemLockedRole, ZOrderRole, SelectedRole, VisualRole, XRole, YRole, WidthRole, HeightRole, ScaleXRole, ScaleYRole, RotationRole, CropLeftRole, CropTopRole, CropRightRole, CropBottomRole, FlipHorizontalRole, FlipVerticalRole };
    explicit SceneItemListModel(StudioProject *project, QString *selectedItemId, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QString addSource(SourceType type, const QString &name);
    bool renameSource(const QString &sourceId, const QString &name);
    bool removeItem(const QString &itemId);
    bool moveItem(const QString &itemId, int direction);
    bool moveItemTo(const QString &itemId, int targetIndex);
    bool setItemVisible(const QString &itemId, bool visible);
    bool setItemLocked(const QString &itemId, bool locked);
    bool setItemTransform(const QString &itemId, const Transform &transform);
    bool applyTransformAction(const QString &itemId, const QString &action);
    void beginSceneChange();
    void endSceneChange();
    void notifySelectionChanged(const QString &previousId, const QString &currentId);
private:
    int itemRow(const QString &itemId) const;
    QVector<int> itemRowsForSource(const QString &sourceId) const;
    StudioProject *project_;
    QString *selectedItemId_;
};

class MixerListModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role { IdRole = Qt::UserRole + 1, NameRole, VolumeRole, MutedRole };
    explicit MixerListModel(StudioProject *project, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    bool setVolume(const QString &id, double volume);
    bool setMuted(const QString &id, bool muted);
    void synchronize();
private:
    StudioProject *project_;
    QVector<MixerChannel> channels_;
};
