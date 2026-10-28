#pragma once

#include "StudioModels.h"
#include "DockLayout.h"
#include "core/audio/AudioInputManager.h"
#include "core/capture/VisualSourceManager.h"
#include "core/project/StudioRepository.h"
#include "core/recorder/LocalRecorder.h"
#include "core/render/ProgramRenderEngine.h"
#include "core/streaming/StreamingOutput.h"
#include "core/platform/CredentialStore.h"
#include "core/platform/OAuthLoopback.h"
#include "core/platform/PlatformHttp.h"
#include "core/platform/YouTubeClient.h"
#include "core/platform/TwitchClient.h"
#include "core/platform/KickClient.h"

#include <QObject>
#include <QJsonObject>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>
#include <memory>

class StudioController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *scenesModel READ scenesModel CONSTANT FINAL)
    Q_PROPERTY(QAbstractItemModel *sceneItemsModel READ sceneItemsModel CONSTANT FINAL)
    Q_PROPERTY(QAbstractItemModel *mixerModel READ mixerModel CONSTANT FINAL)
    Q_PROPERTY(QString activeSceneName READ activeSceneName NOTIFY projectChanged FINAL)
    Q_PROPERTY(int activeSceneIndex READ activeSceneIndex NOTIFY projectChanged FINAL)
    Q_PROPERTY(QString selectedItemId READ selectedItemId NOTIFY selectedItemChanged FINAL)
    Q_PROPERTY(QString profileName READ profileName NOTIFY projectChanged FINAL)
    Q_PROPERTY(QString transitionType READ transitionType NOTIFY projectChanged FINAL)
    Q_PROPERTY(int transitionDurationMs READ transitionDurationMs NOTIFY projectChanged FINAL)
    Q_PROPERTY(int programWidth READ programWidth NOTIFY projectChanged FINAL)
    Q_PROPERTY(int programHeight READ programHeight NOTIFY projectChanged FINAL)
    Q_PROPERTY(QVariantList compositorLayers READ compositorLayers NOTIFY projectChanged FINAL)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged FINAL)
    Q_PROPERTY(QObject *recorder READ recorder CONSTANT FINAL)
    Q_PROPERTY(QObject *streamer READ streamer CONSTANT FINAL)
    Q_PROPERTY(QObject *programEngine READ programEngine CONSTANT FINAL)
    Q_PROPERTY(QString streamServerUrl READ streamServerUrl NOTIFY projectChanged FINAL)
    Q_PROPERTY(QString streamKey READ streamKey NOTIFY streamKeyChanged FINAL)
    Q_PROPERTY(int streamVideoBitrateKbps READ streamVideoBitrateKbps NOTIFY projectChanged FINAL)
    Q_PROPERTY(int streamAudioBitrateKbps READ streamAudioBitrateKbps NOTIFY projectChanged FINAL)
    Q_PROPERTY(int streamFrameRate READ streamFrameRate NOTIFY projectChanged FINAL)
    Q_PROPERTY(QString activeStreamProvider READ activeStreamProvider NOTIFY projectChanged FINAL)
    Q_PROPERTY(QString platformState READ platformState NOTIFY platformStateChanged FINAL)
    Q_PROPERTY(bool detailedRecordingDiagnostics READ detailedRecordingDiagnostics NOTIFY detailedRecordingDiagnosticsChanged FINAL)
    Q_PROPERTY(QVariantList youtubeBroadcasts READ youtubeBroadcasts NOTIFY projectChanged FINAL)
    Q_PROPERTY(QVariantList youtubeCategories READ youtubeCategories NOTIFY projectChanged FINAL)
    Q_PROPERTY(QVariantMap activeStreamMetadata READ activeStreamMetadata NOTIFY projectChanged FINAL)
    Q_PROPERTY(QString selectedYouTubeBroadcastId READ selectedYouTubeBroadcastId NOTIFY projectChanged FINAL)
    Q_PROPERTY(bool youtubeStreamActive READ youtubeStreamActive NOTIFY platformStateChanged FINAL)
    Q_PROPERTY(QString twitchUserCode READ twitchUserCode NOTIFY platformStateChanged FINAL)
    Q_PROPERTY(bool twitchStreamActive READ twitchStreamActive NOTIFY platformStateChanged FINAL)
    Q_PROPERTY(QVariantList twitchCategories READ twitchCategories NOTIFY projectChanged FINAL)
    Q_PROPERTY(QVariantList kickCategories READ kickCategories NOTIFY projectChanged FINAL)
    Q_PROPERTY(DockLayout *dockLayout READ dockLayout CONSTANT FINAL)
public:
    explicit StudioController(QObject *parent = nullptr);
    explicit StudioController(const QString &storageDirectory, QObject *parent = nullptr);
    ~StudioController() override;
    QAbstractItemModel *scenesModel();
    QAbstractItemModel *sceneItemsModel();
    QAbstractItemModel *mixerModel();
    QString activeSceneName() const;
    int activeSceneIndex() const;
    QString selectedItemId() const;
    QString profileName() const;
    QString transitionType() const;
    int transitionDurationMs() const;
    int programWidth() const;
    int programHeight() const;
    QVariantList compositorLayers() const;
    QString statusMessage() const;
    QObject *recorder();
    QObject *streamer();
    QObject *programEngine();
    QString streamServerUrl() const;
    QString streamKey() const;
    int streamVideoBitrateKbps() const;
    int streamAudioBitrateKbps() const;
    int streamFrameRate() const;
    QString activeStreamProvider() const;
    QString platformState() const { return platformState_; }
    bool detailedRecordingDiagnostics() const { return detailedRecordingDiagnostics_; }
    QVariantList youtubeBroadcasts() const { return youtubeBroadcasts_; }
    QVariantList youtubeCategories() const { return youtubeCategories_; }
    QVariantMap activeStreamMetadata() const;
    QString selectedYouTubeBroadcastId() const;
    bool youtubeStreamActive() const { return !youtubeBroadcastId_.isEmpty(); }
    QString twitchUserCode() const { return twitchUserCode_; }
    bool twitchStreamActive() const { return twitchStreamActive_; }
    QVariantList twitchCategories() const { return twitchCategories_; }
    QVariantList kickCategories() const { return kickCategories_; }
    DockLayout *dockLayout() const { return dockLayout_; }
    Q_INVOKABLE QRectF previewCanvasRect(double width, double height, double inset) const;

    Q_INVOKABLE void addScene(const QString &name);
    Q_INVOKABLE void renameScene(const QString &sceneId, const QString &name);
    Q_INVOKABLE void deleteScene(const QString &sceneId);
    Q_INVOKABLE void selectScene(const QString &sceneId);
    Q_INVOKABLE void moveScene(const QString &sceneId, int direction);
    Q_INVOKABLE void addSource(const QString &typeName, const QString &name = {});
    Q_INVOKABLE QVariantList captureTargets(const QString &typeName) const;
    Q_INVOKABLE QVariantList cameraFormats(const QString &targetId) const;
    Q_INVOKABLE QVariantMap sourceConfiguration(const QString &sourceId) const;
    Q_INVOKABLE void configureCaptureSource(const QString &sourceId, const QString &targetId, const QString &formatId = QString{}, bool captureCursor = true);
    Q_INVOKABLE QString sourceRuntimeState(const QString &sourceId) const;
    Q_INVOKABLE QString sourceRuntimeMessage(const QString &sourceId) const;
    Q_INVOKABLE void renameSource(const QString &sourceId, const QString &name);
    Q_INVOKABLE void removeSceneItem(const QString &itemId);
    Q_INVOKABLE void moveSceneItem(const QString &itemId, int direction);
    Q_INVOKABLE void moveSceneItemTo(const QString &itemId, int targetIndex);
    Q_INVOKABLE void setItemVisible(const QString &itemId, bool visible);
    Q_INVOKABLE void setItemLocked(const QString &itemId, bool locked);
    Q_INVOKABLE void selectItem(const QString &itemId);
    Q_INVOKABLE QVariantMap itemTransform(const QString &itemId) const;
    Q_INVOKABLE void setItemTransform(const QString &itemId, const QVariantMap &values);
    Q_INVOKABLE void previewItemTransform(const QString &itemId, const QVariantMap &values);
    Q_INVOKABLE void commitPreviewTransform();
    Q_INVOKABLE void applyTransformAction(const QString &itemId, const QString &action);
    Q_INVOKABLE void removeSourceFromActiveScene(const QString &sourceId);
    Q_INVOKABLE void setMixerVolume(const QString &channelId, double volume);
    Q_INVOKABLE void setMixerMuted(const QString &channelId, bool muted);
    Q_INVOKABLE void setTransitionType(const QString &type);
    Q_INVOKABLE void setTransitionDurationMs(int durationMs);
    Q_INVOKABLE void setProfileName(const QString &name);
    Q_INVOKABLE void showUnavailableAction(const QString &action);
    Q_INVOKABLE void toggleRecording();
    Q_INVOKABLE void setDetailedRecordingDiagnostics(bool enabled);
    Q_INVOKABLE bool exportRecordingDiagnostics(const QString &fileUrl);
    Q_INVOKABLE QString providerConnectionState(const QString &providerName) const;
    Q_INVOKABLE QString providerSetupMessage(const QString &providerName) const;
    Q_INVOKABLE void openProviderSetupHelp(const QString &providerName) const;
    Q_INVOKABLE QVariantList streamingProviders() const;
    Q_INVOKABLE QVariantMap providerCapabilities(const QString &providerName) const;
    Q_INVOKABLE void selectStreamDestination(const QString &destinationId);
    Q_INVOKABLE void setStreamConfiguration(const QString &serverUrl, const QString &streamKey, int videoBitrateKbps, int audioBitrateKbps, int frameRate);
    Q_INVOKABLE void toggleStreaming();
    Q_INVOKABLE void connectYouTube();
    Q_INVOKABLE void cancelYouTubeConnection();
    Q_INVOKABLE void disconnectYouTube();
    Q_INVOKABLE void refreshYouTubeOptions();
    Q_INVOKABLE void setActiveStreamMetadata(const QVariantMap &values);
    Q_INVOKABLE void selectYouTubeBroadcast(const QString &broadcastId);
    Q_INVOKABLE void goLiveYouTube(bool recordWhileStreaming);
    Q_INVOKABLE void stopYouTubeStream();
    Q_INVOKABLE void connectTwitch();
    Q_INVOKABLE void cancelTwitchConnection();
    Q_INVOKABLE void disconnectTwitch();
    Q_INVOKABLE void searchTwitchCategories(const QString &query);
    Q_INVOKABLE void goLiveTwitch(bool recordWhileStreaming);
    Q_INVOKABLE void stopTwitchStream();
    Q_INVOKABLE void connectKick();
    Q_INVOKABLE void cancelKickConnection();
    Q_INVOKABLE void disconnectKick();
    Q_INVOKABLE void searchKickCategories(const QString &query);
    Q_INVOKABLE void updateKickChannel();

signals:
    void projectChanged();
    void selectedItemChanged();
    void statusMessageChanged();
    void streamKeyChanged();
    void platformStateChanged();
    void detailedRecordingDiagnosticsChanged();

private:
    DockLayout *dockLayout_ = nullptr;
    void refresh();
    void save();
    void updateProgramAudioSink();
    void setPlatformState(const QString &state, const QString &message = {});
    void loadYouTubeChannel();
    void loadYouTubeBroadcastPage(const QString &pageToken, quint64 generation);
    void prepareYouTubeStream(const QString &broadcastId);
    void startYouTubeIngest(const QString &broadcastId, const QJsonObject &stream);
    void checkYouTubeReady();
    StreamDestination *youtubeDestination();
    StreamDestination *twitchDestination();
    void loadTwitchAccount();
    void pollTwitchConnection();
    void checkTwitchReady();
    StreamDestination *kickDestination();
    void loadKickAccount();
    StudioRepository repository_;
    StudioProject project_;
    SceneListModel scenesModel_;
    QString selectedItemId_;
    SceneItemListModel sceneItemsModel_;
    MixerListModel mixerModel_;
    VisualSourceManager visualSources_;
    AudioInputManager audioSources_;
    LocalRecorder recorder_;
    StreamingOutput streamer_;
    ProgramRenderEngine programEngine_;
    QString statusMessage_;
    QString preferencesPath_;
    bool detailedRecordingDiagnostics_ = false;
    QString streamKey_;
    QtPlatformHttpTransport platformHttp_;
    WindowsCredentialStore credentials_;
    OAuthLoopback oauth_;
    OAuthLoopback kickOauth_;
    std::unique_ptr<YouTubeClient> youtube_;
    std::unique_ptr<TwitchClient> twitch_;
    std::unique_ptr<KickClient> kick_;
    QTimer youtubePoll_;
    QString platformState_ = QStringLiteral("Idle");
    QString youtubeBroadcastId_;
    QString youtubeStreamId_;
    QVariantList youtubeBroadcasts_;
    QSet<QString> youtubeBroadcastPageTokens_;
    quint64 youtubeBroadcastRefreshGeneration_ = 0;
    QVariantList youtubeCategories_;
    bool youtubeChecking_ = false;
    bool youtubeTransitionRequested_ = false;
    bool recordWithYouTube_ = false;
    bool recordingStartedForStream_ = false;
    QTimer twitchAuthPoll_;
    QTimer twitchLivePoll_;
    QString twitchDeviceCode_;
    QString twitchUserCode_;
    qint64 twitchAuthorizationDeadlineMs_ = 0;
    QVariantList twitchCategories_;
    bool twitchStreamActive_ = false;
    bool twitchChecking_ = false;
    bool recordWithTwitch_ = false;
    bool twitchRecordingStarted_ = false;
    QVariantList kickCategories_;
};
