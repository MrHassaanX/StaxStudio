#pragma once

#include "core/audio/mixer/MixerChannel.h"
#include "core/scene/Scene.h"
#include "core/source/Source.h"
#include "core/transition/TransitionSettings.h"

#include <QString>
#include <QVector>

class StudioProject final
{
public:
    static constexpr int SchemaVersion = 1;

    static StudioProject createDefault();

    QString profileId;
    QString profileName;
    QString activeSceneId;
    QVector<Scene> scenes;
    QVector<Source> sources;
    QVector<MixerChannel> mixerChannels;
    TransitionSettings transition;

    [[nodiscard]] const Scene *activeScene() const;
    [[nodiscard]] Scene *activeScene();
    [[nodiscard]] QString uniqueSceneName(const QString &requestedName) const;
    [[nodiscard]] QString uniqueSourceName(const QString &requestedName) const;
    [[nodiscard]] int sceneIndex(const QString &sceneId) const;
    [[nodiscard]] int sourceIndex(const QString &sourceId) const;
    [[nodiscard]] Source *source(const QString &sourceId);
    [[nodiscard]] const Source *source(const QString &sourceId) const;

    Scene &addScene(const QString &requestedName);
    bool removeScene(const QString &sceneId);
    bool moveScene(const QString &sceneId, int delta);
    bool renameScene(const QString &sceneId, const QString &requestedName);
    Source &addSource(SourceType type);
    bool removeSceneItem(const QString &sceneItemId);
    bool moveSceneItem(const QString &sceneItemId, int delta);
    bool moveSceneItemTo(const QString &sceneItemId, int targetIndex);
    bool setSceneItemVisible(const QString &sceneItemId, bool visible);
    bool setSceneItemLocked(const QString &sceneItemId, bool locked);
    bool setSceneItemTransform(const QString &sceneItemId, const Transform &transform);
    bool resetSceneItemTransform(const QString &sceneItemId);
    bool fitSceneItemToCanvas(const QString &sceneItemId);
    bool stretchSceneItemToCanvas(const QString &sceneItemId);
    bool centerSceneItem(const QString &sceneItemId, bool horizontal, bool vertical);
    bool rotateSceneItem(const QString &sceneItemId, double degrees);
    bool flipSceneItem(const QString &sceneItemId, bool horizontal);
    bool renameSource(const QString &sourceId, const QString &requestedName);
    bool setMixerVolume(const QString &channelId, double volume);
    bool setMixerMuted(const QString &channelId, bool muted);
    void normalize();
};
