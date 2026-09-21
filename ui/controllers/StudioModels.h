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
    void refresh();
private:
    StudioProject *project_;
};

class SceneItemListModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role { IdRole = Qt::UserRole + 1, SourceIdRole, NameRole, TypeRole, ItemVisibleRole, ItemLockedRole, ZOrderRole, SelectedRole };
    explicit SceneItemListModel(StudioProject *project, QString *selectedItemId, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void refresh();
private:
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
    void refresh();
private:
    StudioProject *project_;
};
