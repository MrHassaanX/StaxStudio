#pragma once

#include "StudioModels.h"
#include "core/project/StudioRepository.h"

#include <QObject>

class StudioController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *scenesModel READ scenesModel CONSTANT FINAL)
    Q_PROPERTY(QAbstractItemModel *sceneItemsModel READ sceneItemsModel CONSTANT FINAL)
    Q_PROPERTY(QAbstractItemModel *mixerModel READ mixerModel CONSTANT FINAL)
    Q_PROPERTY(QString activeSceneName READ activeSceneName NOTIFY projectChanged FINAL)
    Q_PROPERTY(QString selectedItemId READ selectedItemId NOTIFY selectedItemChanged FINAL)
    Q_PROPERTY(QString profileName READ profileName NOTIFY projectChanged FINAL)
    Q_PROPERTY(QString transitionType READ transitionType NOTIFY projectChanged FINAL)
    Q_PROPERTY(int transitionDurationMs READ transitionDurationMs NOTIFY projectChanged FINAL)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged FINAL)
public:
    explicit StudioController(QObject *parent = nullptr);
    QAbstractItemModel *scenesModel();
    QAbstractItemModel *sceneItemsModel();
    QAbstractItemModel *mixerModel();
    QString activeSceneName() const;
    QString selectedItemId() const;
    QString profileName() const;
    QString transitionType() const;
    int transitionDurationMs() const;
    QString statusMessage() const;

    Q_INVOKABLE void addScene(const QString &name);
    Q_INVOKABLE void renameScene(const QString &sceneId, const QString &name);
    Q_INVOKABLE void deleteScene(const QString &sceneId);
    Q_INVOKABLE void selectScene(const QString &sceneId);
    Q_INVOKABLE void moveScene(const QString &sceneId, int direction);
    Q_INVOKABLE void addSource(const QString &typeName);
    Q_INVOKABLE void renameSource(const QString &sourceId, const QString &name);
    Q_INVOKABLE void removeSceneItem(const QString &itemId);
    Q_INVOKABLE void moveSceneItem(const QString &itemId, int direction);
    Q_INVOKABLE void setItemVisible(const QString &itemId, bool visible);
    Q_INVOKABLE void setItemLocked(const QString &itemId, bool locked);
    Q_INVOKABLE void selectItem(const QString &itemId);
    Q_INVOKABLE void setMixerVolume(const QString &channelId, double volume);
    Q_INVOKABLE void setMixerMuted(const QString &channelId, bool muted);
    Q_INVOKABLE void setTransitionType(const QString &type);
    Q_INVOKABLE void setTransitionDurationMs(int durationMs);
    Q_INVOKABLE void setProfileName(const QString &name);
    Q_INVOKABLE void showUnavailableAction(const QString &action);

signals:
    void projectChanged();
    void selectedItemChanged();
    void statusMessageChanged();

private:
    void refresh(bool sceneChanged = false);
    void save();
    StudioRepository repository_;
    StudioProject project_;
    SceneListModel scenesModel_;
    QString selectedItemId_;
    SceneItemListModel sceneItemsModel_;
    MixerListModel mixerModel_;
    QString statusMessage_;
};
