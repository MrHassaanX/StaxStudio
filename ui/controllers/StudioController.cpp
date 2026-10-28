#include "StudioController.h"

#include "core/render/CompositorScene.h"
#include "core/render/ProgramFrameMath.h"
#include "core/platform/PlatformProviders.h"
#include "core/platform/GoogleOAuthConfiguration.h"
#include "core/recorder/RecordingDiagnostics.h"
#include "AppOAuthConfig.h"
#include <algorithm>
#include <QDesktopServices>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSettings>
#include <QUrlQuery>

namespace {
QVariantMap transformValues(const Transform &value)
{
    return {{"x", value.x}, {"y", value.y}, {"width", value.width}, {"height", value.height},
            {"scaleX", value.scaleX}, {"scaleY", value.scaleY}, {"rotation", value.rotation},
            {"cropLeft", value.cropLeft}, {"cropTop", value.cropTop}, {"cropRight", value.cropRight},
            {"cropBottom", value.cropBottom}, {"flipHorizontal", value.flipHorizontal}, {"flipVertical", value.flipVertical}};
}
}

StudioController::StudioController(QObject *parent)
    : StudioController(QString{}, parent)
{
}

StudioController::StudioController(const QString &storageDirectory, QObject *parent)
    : QObject(parent), repository_(storageDirectory), project_(repository_.load()), scenesModel_(&project_, this),
      sceneItemsModel_(&project_, &selectedItemId_, this), mixerModel_(&project_, this), visualSources_(this), audioSources_(this), recorder_(this), streamer_(this),
      statusMessage_(QStringLiteral("Studio setup is saved locally. Media capture is not connected yet."))
{
    dockLayout_ = new DockLayout(storageDirectory, this);
    preferencesPath_ = QFileInfo(repository_.filePath()).dir().filePath(QStringLiteral("preferences.ini"));
    detailedRecordingDiagnostics_ = QSettings(preferencesPath_, QSettings::IniFormat)
                                        .value(QStringLiteral("recording/detailedDiagnostics"), false).toBool();
    if (StreamDestination *destination = youtubeDestination()) {
        const QString clientId = GoogleOAuthConfiguration::resolveClientId(
            preferencesPath_, QString::fromUtf8(STAX_GOOGLE_OAUTH_CLIENT_ID),
            qEnvironmentVariable("STAX_GOOGLE_CLIENT_ID"));
        const QString clientSecret = GoogleOAuthConfiguration::resolveClientSecret(
            credentials_, clientId, QString::fromUtf8(STAX_GOOGLE_OAUTH_CLIENT_SECRET),
            qEnvironmentVariable("STAX_GOOGLE_CLIENT_SECRET"));
        youtube_ = std::make_unique<YouTubeClient>(&platformHttp_, &credentials_,
            QStringLiteral("youtube/") + destination->id,
            clientId, clientSecret);
        destination->connected = youtube_->hasCredentials();
    }
    if (StreamDestination *destination = twitchDestination()) {
        const QString clientId = qEnvironmentVariable("STAX_TWITCH_CLIENT_ID").trimmed();
        twitch_ = std::make_unique<TwitchClient>(&platformHttp_, &credentials_,
            QStringLiteral("twitch/") + destination->id,
            clientId.isEmpty() ? QString::fromUtf8(STAX_TWITCH_OAUTH_CLIENT_ID) : clientId);
        destination->connected = twitch_->hasCredentials();
    }
    if (StreamDestination *destination = kickDestination()) {
        kick_ = std::make_unique<KickClient>(&platformHttp_, &credentials_,
            QStringLiteral("kick/") + destination->id,
            qEnvironmentVariable("STAX_KICK_CLIENT_ID").trimmed(),
            qEnvironmentVariable("STAX_KICK_CLIENT_SECRET").trimmed());
        destination->connected = kick_->hasCredentials();
    }
    connect(&oauth_, &OAuthLoopback::authorized, this, [this](const QString &code, const QString &verifier, const QString &redirect) {
        setPlatformState(QStringLiteral("Exchanging authorization code"), QStringLiteral("Google authorization received. Securely exchanging the authorization code..."));
        youtube_->exchangeCode(code, verifier, redirect, [this](const QString &error) {
            if (!error.isEmpty()) { setPlatformState(QStringLiteral("Token exchange failed"), error); return; }
            loadYouTubeChannel();
        });
    });
    connect(&oauth_, &OAuthLoopback::failed, this, [this](const QString &message) {
        const QString testUserHelp = QStringLiteral(" If Google shows 403 access_denied while this app is in Testing, add your Google account under Google Auth Platform → Audience → Test users.");
        setPlatformState(QStringLiteral("Authorization failed"), message + testUserHelp);
    });
    connect(&kickOauth_, &OAuthLoopback::authorized, this, [this](const QString &code, const QString &verifier, const QString &redirect) {
        setPlatformState(QStringLiteral("Authenticating"), QStringLiteral("Connecting your Kick account..."));
        kick_->exchangeCode(code, verifier, redirect, [this](const QString &error) {
            if (!error.isEmpty()) { setPlatformState(QStringLiteral("Error"), error); return; }
            loadKickAccount();
        });
    });
    connect(&kickOauth_, &OAuthLoopback::failed, this, [this](const QString &message) {
        setPlatformState(QStringLiteral("Error"), message);
    });
    youtubePoll_.setInterval(3000);
    connect(&youtubePoll_, &QTimer::timeout, this, &StudioController::checkYouTubeReady);
    connect(&twitchAuthPoll_, &QTimer::timeout, this, &StudioController::pollTwitchConnection);
    twitchLivePoll_.setInterval(5000);
    connect(&twitchLivePoll_, &QTimer::timeout, this, &StudioController::checkTwitchReady);
    connect(&visualSources_, &VisualSourceManager::framesChanged, this, [this] {
        for (const Source &source : project_.sources) {
            const CapturedVideoFrame frame = visualSources_.frame(source.id);
            if (!frame.image.isNull()) programEngine_.updateSourceFrame(source.id, frame.image, frame.timestampNs);
        }
    });
    connect(&visualSources_, &VisualSourceManager::sourceStateChanged, this, [this](const QString &) { emit projectChanged(); });
    connect(&audioSources_, &AudioInputManager::metersChanged, this, [this] {
        for (const MixerChannel &channel : project_.mixerChannels) {
            const AudioRuntime runtime = audioSources_.runtime(channel.id);
            mixerModel_.setRuntime(channel.id, captureStateName(runtime.state), runtime.channelCount, runtime.peakDb);
        }
    });
    visualSources_.synchronizeSources(project_.sources);
    programEngine_.setRecorder(&recorder_);
    programEngine_.setStreamer(&streamer_);
    programEngine_.setAudioDiagnosticsProvider([this] { return audioSources_.diagnostics(); });
    programEngine_.setScene(compositorLayers(), project_.programResolution.size());
    audioSources_.synchronizeSources(project_.sources);
    for (const MixerChannel &channel : project_.mixerChannels) audioSources_.setMixControls(channel.id, channel.volume, channel.muted);
    connect(&recorder_, &LocalRecorder::stateChanged, this, [this] {
        const QString state = recorder_.stateName();
        statusMessage_ = recorder_.state() == RecordingState::Error
            ? QStringLiteral("Recording error: %1").arg(recorder_.errorMessage())
            : state == QStringLiteral("Recording") ? QStringLiteral("Recording to %1").arg(recorder_.outputPath())
            : QStringLiteral("Recorder %1").arg(state.toLower());
        updateProgramAudioSink();
        emit statusMessageChanged();
        emit projectChanged();
    });
    connect(&streamer_, &StreamingOutput::stateChanged, this, [this] {
        const QString state = streamer_.stateName();
        if (!youtubeBroadcastId_.isEmpty()) {
            if (streamer_.state() == StreamingState::Live) {
                setPlatformState(QStringLiteral("Waiting for YouTube"));
                youtubePoll_.start();
                checkYouTubeReady();
            } else if (streamer_.state() == StreamingState::Reconnecting) {
                setPlatformState(QStringLiteral("Reconnecting"));
            } else if (streamer_.state() == StreamingState::Error) {
                youtubePoll_.stop();
                setPlatformState(QStringLiteral("Error"), streamer_.errorMessage());
            }
        }
        if (twitchStreamActive_) {
            if (streamer_.state() == StreamingState::Live) {
                setPlatformState(QStringLiteral("Waiting for Twitch"), QStringLiteral("Waiting for Twitch to report the stream as live..."));
                twitchLivePoll_.start();
                checkTwitchReady();
            } else if (streamer_.state() == StreamingState::Reconnecting) {
                setPlatformState(QStringLiteral("Reconnecting"));
            } else if (streamer_.state() == StreamingState::Error) {
                twitchLivePoll_.stop();
                setPlatformState(QStringLiteral("Error"), streamer_.errorMessage());
            }
        }
        statusMessage_ = streamer_.state() == StreamingState::Error
            ? QStringLiteral("Streaming error: %1").arg(streamer_.errorMessage())
            : state == QStringLiteral("Live") ? QStringLiteral("Streaming live")
            : QStringLiteral("Streamer %1").arg(state.toLower());
        updateProgramAudioSink();
        emit statusMessageChanged();
        emit projectChanged();
    });
}

QRectF StudioController::previewCanvasRect(double width, double height, double inset) const
{
    auto rect=ProgramFrameMath::bestFitPreviewRect(project_.programResolution.size(),
        QSizeF(qMax(0.0,width-2*inset),qMax(0.0,height-2*inset)));
    rect.translate(inset,inset);
    return rect;
}

StudioController::~StudioController()
{
    // The recording sink is invoked on AudioInputManager's worker, so clear it
    // before LocalRecorder is destroyed (member destruction is reverse order).
    audioSources_.setRecordingSink({});
}

QAbstractItemModel *StudioController::scenesModel() { return &scenesModel_; }
QAbstractItemModel *StudioController::sceneItemsModel() { return &sceneItemsModel_; }
QAbstractItemModel *StudioController::mixerModel() { return &mixerModel_; }
QString StudioController::activeSceneName() const { const Scene *scene = project_.activeScene(); return scene ? scene->name : QStringLiteral("Untitled Scene"); }
int StudioController::activeSceneIndex() const { return project_.sceneIndex(project_.activeSceneId); }
QString StudioController::selectedItemId() const { return selectedItemId_; }
QString StudioController::profileName() const { return project_.profileName; }
QString StudioController::transitionType() const { return project_.transition.type == TransitionType::Cut ? QStringLiteral("Cut") : QStringLiteral("Fade"); }
int StudioController::transitionDurationMs() const { return project_.transition.durationMs; }
int StudioController::programWidth() const { return project_.programResolution.width; }
int StudioController::programHeight() const { return project_.programResolution.height; }
QVariantList StudioController::compositorLayers() const
{
    QVariantList values;
    for (const CompositorLayer &layer : CompositorScene::activeLayers(project_)) {
        const CapturedVideoFrame captured = visualSources_.frame(layer.sourceId);
        const Source *source = project_.source(layer.sourceId);
        values.append(QVariantMap{{"itemId", layer.sceneItemId}, {"sourceId", layer.sourceId}, {"x", layer.transform.x}, {"y", layer.transform.y},
                                  {"width", layer.transform.width}, {"height", layer.transform.height},
                                  {"scaleX", layer.transform.scaleX}, {"scaleY", layer.transform.scaleY},
                                  {"rotation", layer.transform.rotation}, {"cropLeft", layer.transform.cropLeft},
                                  {"cropTop", layer.transform.cropTop}, {"cropRight", layer.transform.cropRight},
                                  {"cropBottom", layer.transform.cropBottom}, {"flipHorizontal", layer.transform.flipHorizontal},
                                  {"flipVertical", layer.transform.flipVertical}, {"color", layer.frame.color}, {"image", captured.image},
                                  {"sourceType", source ? sourceTypeName(source->type) : QString{}},
                                  {"targetId", source ? source->configuration.value("targetId").toString() : QString{}},
                                  {"captureCursor", source ? source->configuration.value("captureCursor").toBool(true) : true},
                                  {"timestampNs", captured.timestampNs}, {"zOrder", layer.zOrder}});
    }
    return values;
}
QString StudioController::statusMessage() const { return statusMessage_; }
QObject *StudioController::recorder() { return &recorder_; }
QObject *StudioController::streamer() { return &streamer_; }
QObject *StudioController::programEngine() { return &programEngine_; }
QString StudioController::streamServerUrl() const { return project_.streamSettings.serverUrl; }
QString StudioController::streamKey() const { return streamKey_; }
int StudioController::streamVideoBitrateKbps() const { return project_.streamSettings.videoBitrateKbps; }
int StudioController::streamAudioBitrateKbps() const { return project_.streamSettings.audioBitrateKbps; }
int StudioController::streamFrameRate() const { return project_.streamSettings.frameRate; }
QString StudioController::activeStreamProvider() const
{
    for (const StreamDestination &destination : project_.streamDestinations)
        if (destination.id == project_.activeStreamDestinationId) return platformProviderName(destination.provider);
    return platformProviderName(PlatformProviderId::CustomRtmp);
}

void StudioController::addScene(const QString &name)
{
    if (name.trimmed().isEmpty()) return;
    const QString id = scenesModel_.addScene(name);
    selectScene(id);
}
void StudioController::renameScene(const QString &id, const QString &name) { if (scenesModel_.renameScene(id, name)) refresh(); }
void StudioController::deleteScene(const QString &id)
{
    if (project_.sceneIndex(id) < 0 || project_.scenes.size() <= 1) return;
    const bool active = project_.activeSceneId == id;
    if (active) sceneItemsModel_.beginSceneChange();
    scenesModel_.removeScene(id);
    if (active) {
        selectedItemId_.clear();
        sceneItemsModel_.endSceneChange();
        emit selectedItemChanged();
    }
    scenesModel_.notifyActiveSceneChanged();
    refresh();
}
void StudioController::selectScene(const QString &id)
{
    if (project_.sceneIndex(id) < 0 || project_.activeSceneId == id) return;
    sceneItemsModel_.beginSceneChange();
    project_.activeSceneId = id;
    selectedItemId_.clear();
    sceneItemsModel_.endSceneChange();
    emit selectedItemChanged();
    scenesModel_.notifyActiveSceneChanged();
    refresh();
}
void StudioController::moveScene(const QString &id, int direction) { if (scenesModel_.moveScene(id, direction)) refresh(); }
void StudioController::addSource(const QString &typeName, const QString &name)
{
    bool ok = false;
    const SourceType type = sourceTypeFromName(typeName, &ok);
    if (!ok) return;
    const QString itemId = sceneItemsModel_.addSource(type, name);
    Scene *scene = project_.activeScene();
    Source *source = scene && !scene->items.isEmpty() ? project_.source(scene->items.last().sourceId) : nullptr;
    if (source && (type == SourceType::DisplayCapture || type == SourceType::WindowCapture || type == SourceType::GameCapture || type == SourceType::Webcam)) {
        source->configuration.insert(QStringLiteral("targetId"), visualSources_.defaultTarget(type));
        QSize sourceSize;
        for (const QVariant &target : visualSources_.targets(type)) {
            const QVariantMap values = target.toMap();
            if (values.value("id").toString() == source->configuration.value("targetId").toString()) {
                sourceSize = {values.value("width").toInt(), values.value("height").toInt()};
                break;
            }
        }
        if (sourceSize.isValid()) {
            sceneItemsModel_.setItemTransform(itemId, ProgramFrameMath::fitToCanvas(sourceSize, project_.programResolution.size()));
        }
    }
    refresh();
}
QVariantList StudioController::captureTargets(const QString &typeName) const
{
    bool ok = false;
    const SourceType type = sourceTypeFromName(typeName, &ok);
    if (!ok) return {};
    if (type == SourceType::Microphone || type == SourceType::DesktopAudio) return audioSources_.targets(type);
    return visualSources_.targets(type);
}
QVariantList StudioController::cameraFormats(const QString &targetId) const { return visualSources_.cameraFormats(targetId); }
QVariantMap StudioController::sourceConfiguration(const QString &sourceId) const
{
    const Source *source = project_.source(sourceId);
    return source ? source->configuration.toVariantMap() : QVariantMap{};
}
void StudioController::configureCaptureSource(const QString &sourceId, const QString &targetId, const QString &formatId, const bool captureCursor)
{
    Source *source = project_.source(sourceId);
    if (!source) return;
    source->configuration.insert(QStringLiteral("targetId"), targetId);
    source->configuration.insert(QStringLiteral("formatId"), formatId);
    source->configuration.insert(QStringLiteral("captureCursor"), captureCursor);
    refresh();
}
QString StudioController::sourceRuntimeState(const QString &sourceId) const { const Source *source = project_.source(sourceId); return source && (source->type == SourceType::Microphone || source->type == SourceType::DesktopAudio) ? captureStateName(audioSources_.runtime(sourceId).state) : captureStateName(visualSources_.frame(sourceId).state); }
QString StudioController::sourceRuntimeMessage(const QString &sourceId) const { const Source *source = project_.source(sourceId); return source && (source->type == SourceType::Microphone || source->type == SourceType::DesktopAudio) ? audioSources_.runtime(sourceId).message : visualSources_.frame(sourceId).message; }
void StudioController::renameSource(const QString &id, const QString &name) { if (sceneItemsModel_.renameSource(id, name)) refresh(); }
void StudioController::removeSceneItem(const QString &id)
{
    if (!sceneItemsModel_.removeItem(id)) return;
    if (selectedItemId_ == id) { selectedItemId_.clear(); emit selectedItemChanged(); }
    refresh();
}
void StudioController::moveSceneItem(const QString &id, int direction) { if (sceneItemsModel_.moveItem(id, direction)) refresh(); }
void StudioController::moveSceneItemTo(const QString &id, int targetIndex) { if (sceneItemsModel_.moveItemTo(id, targetIndex)) refresh(); }
void StudioController::setItemVisible(const QString &id, bool visible) { if (sceneItemsModel_.setItemVisible(id, visible)) refresh(); }
void StudioController::setItemLocked(const QString &id, bool locked) { if (sceneItemsModel_.setItemLocked(id, locked)) refresh(); }
void StudioController::selectItem(const QString &id)
{
    if (selectedItemId_ == id) return;
    const Scene *scene = project_.activeScene();
    if (!scene) return;
    bool found = id.isEmpty();
    for (const auto &item : scene->items) if (item.id == id) found = true;
    if (!found) return;
    const QString previousId = selectedItemId_;
    selectedItemId_ = id;
    sceneItemsModel_.notifySelectionChanged(previousId, selectedItemId_);
    emit selectedItemChanged();
}
QVariantMap StudioController::itemTransform(const QString &itemId) const
{
    const Scene *scene = project_.activeScene();
    if (!scene) return {};
    for (const SceneItem &item : scene->items) if (item.id == itemId) return transformValues(item.transform);
    return {};
}
void StudioController::setItemTransform(const QString &itemId, const QVariantMap &values)
{
    Transform value;
    const QVariantMap existing = itemTransform(itemId);
    if (existing.isEmpty()) return;
    value.x = values.value("x", existing.value("x")).toDouble();
    value.y = values.value("y", existing.value("y")).toDouble();
    value.width = values.value("width", existing.value("width")).toDouble();
    value.height = values.value("height", existing.value("height")).toDouble();
    value.scaleX = values.value("scaleX", existing.value("scaleX")).toDouble();
    value.scaleY = values.value("scaleY", existing.value("scaleY")).toDouble();
    value.rotation = values.value("rotation", existing.value("rotation")).toDouble();
    value.cropLeft = values.value("cropLeft", existing.value("cropLeft")).toDouble();
    value.cropTop = values.value("cropTop", existing.value("cropTop")).toDouble();
    value.cropRight = values.value("cropRight", existing.value("cropRight")).toDouble();
    value.cropBottom = values.value("cropBottom", existing.value("cropBottom")).toDouble();
    value.flipHorizontal = values.value("flipHorizontal", existing.value("flipHorizontal")).toBool();
    value.flipVertical = values.value("flipVertical", existing.value("flipVertical")).toBool();
    if (sceneItemsModel_.setItemTransform(itemId, value)) refresh();
}
void StudioController::previewItemTransform(const QString &itemId, const QVariantMap &values)
{
    Transform value;
    const QVariantMap existing = itemTransform(itemId);
    if (existing.isEmpty()) return;
    value.x = values.value("x", existing.value("x")).toDouble();
    value.y = values.value("y", existing.value("y")).toDouble();
    value.width = values.value("width", existing.value("width")).toDouble();
    value.height = values.value("height", existing.value("height")).toDouble();
    value.scaleX = values.value("scaleX", existing.value("scaleX")).toDouble();
    value.scaleY = values.value("scaleY", existing.value("scaleY")).toDouble();
    value.rotation = values.value("rotation", existing.value("rotation")).toDouble();
    value.cropLeft = values.value("cropLeft", existing.value("cropLeft")).toDouble();
    value.cropTop = values.value("cropTop", existing.value("cropTop")).toDouble();
    value.cropRight = values.value("cropRight", existing.value("cropRight")).toDouble();
    value.cropBottom = values.value("cropBottom", existing.value("cropBottom")).toDouble();
    value.flipHorizontal = values.value("flipHorizontal", existing.value("flipHorizontal")).toBool();
    value.flipVertical = values.value("flipVertical", existing.value("flipVertical")).toBool();
    if (sceneItemsModel_.setItemTransform(itemId, value)) programEngine_.updateTransform(itemId, transformValues(value));
}
void StudioController::commitPreviewTransform() { save(); emit projectChanged(); }
void StudioController::applyTransformAction(const QString &itemId, const QString &action) { if (sceneItemsModel_.applyTransformAction(itemId, action)) refresh(); }
void StudioController::removeSourceFromActiveScene(const QString &sourceId)
{
    const Scene *scene = project_.activeScene();
    if (!scene) return;
    for (const SceneItem &item : scene->items) if (item.sourceId == sourceId) { removeSceneItem(item.id); return; }
}
void StudioController::setMixerVolume(const QString &id, double volume) { if (mixerModel_.setVolume(id, volume)) { const auto channel = std::find_if(project_.mixerChannels.cbegin(), project_.mixerChannels.cend(), [&](const MixerChannel &value) { return value.id == id; }); if (channel != project_.mixerChannels.cend()) audioSources_.setMixControls(id, channel->volume, channel->muted); refresh(); } }
void StudioController::setMixerMuted(const QString &id, bool muted) { if (mixerModel_.setMuted(id, muted)) { const auto channel = std::find_if(project_.mixerChannels.cbegin(), project_.mixerChannels.cend(), [&](const MixerChannel &value) { return value.id == id; }); if (channel != project_.mixerChannels.cend()) audioSources_.setMixControls(id, channel->volume, channel->muted); refresh(); } }
void StudioController::setTransitionType(const QString &type) { project_.transition.type = type == QStringLiteral("Cut") ? TransitionType::Cut : TransitionType::Fade; refresh(); }
void StudioController::setTransitionDurationMs(int value) { project_.transition.durationMs = qBound(50, value, 10000); refresh(); }
void StudioController::setProfileName(const QString &name) { const QString trimmed = name.trimmed(); if (!trimmed.isEmpty()) { project_.profileName = trimmed; refresh(); } }
void StudioController::showUnavailableAction(const QString &action) { statusMessage_ = QStringLiteral("%1 is planned for the media milestone. Your studio configuration is safe.").arg(action); emit statusMessageChanged(); }
void StudioController::toggleRecording()
{
    if (recorder_.state() == RecordingState::Recording || recorder_.state() == RecordingState::Starting || recorder_.state() == RecordingState::Stopping) {
        audioSources_.flushRecordingSink();
        recorder_.setProgramDiagnostics(programEngine_.diagnostics());
        recorder_.stop();
        updateProgramAudioSink();
        return;
    }
    RecordingSettings settings;
    settings.detailedDiagnostics = detailedRecordingDiagnostics_;
    audioSources_.discardPendingBlocks();
    updateProgramAudioSink();
    if (!recorder_.start(settings, {project_.programResolution.width, project_.programResolution.height})) {
        statusMessage_ = QStringLiteral("Unable to start the recorder.");
        emit statusMessageChanged();
    } else {
        updateProgramAudioSink();
    }
}

void StudioController::setDetailedRecordingDiagnostics(const bool enabled)
{
    if (detailedRecordingDiagnostics_ == enabled) return;
    QSettings preferences(preferencesPath_, QSettings::IniFormat);
    preferences.setValue(QStringLiteral("recording/detailedDiagnostics"), enabled);
    preferences.sync();
    if (preferences.status() != QSettings::NoError) {
        statusMessage_ = QStringLiteral("Unable to save diagnostics preference.");
        emit statusMessageChanged();
        return;
    }
    detailedRecordingDiagnostics_ = enabled;
    emit detailedRecordingDiagnosticsChanged();
}

bool StudioController::exportRecordingDiagnostics(const QString &fileUrl)
{
    const QUrl url(fileUrl);
    const QString path = url.isLocalFile() ? url.toLocalFile() : fileUrl;
    QString error;
    const bool saved = RecordingDiagnostics::exportLatest(path, {}, &error);
    statusMessage_ = saved ? QStringLiteral("Diagnostics exported to %1").arg(path) : error;
    emit statusMessageChanged();
    return saved;
}

QString StudioController::providerConnectionState(const QString &providerName) const
{
    if (providerName == QStringLiteral("custom-rtmp")) {
        if (streamer_.state() == StreamingState::Live) return QStringLiteral("Live");
        if (streamer_.state() == StreamingState::Connecting || streamer_.state() == StreamingState::Reconnecting)
            return QStringLiteral("Connecting");
        return QStringLiteral("Ready to stream");
    }
    bool configured = false;
    if (providerName == QStringLiteral("youtube")) configured = youtube_ && !youtube_->clientId().isEmpty();
    if (providerName == QStringLiteral("twitch")) configured = twitch_ && !twitch_->clientId().isEmpty();
    if (providerName == QStringLiteral("kick")) {
        bool validPort = false;
        const quint16 port = qEnvironmentVariable("STAX_KICK_REDIRECT_PORT").toUShort(&validPort);
        configured = kick_ && kick_->configured() && validPort && port != 0;
    }
    if (!configured) return QStringLiteral("Not configured");
    for (const StreamDestination &destination : project_.streamDestinations) {
        if (platformProviderName(destination.provider) != providerName) continue;
        if (destination.connected) {
            if ((providerName == QStringLiteral("youtube") && !youtubeBroadcastId_.isEmpty() && platformState_ == QStringLiteral("Live"))
                || (providerName == QStringLiteral("twitch") && twitchStreamActive_ && platformState_ == QStringLiteral("Live")))
                return QStringLiteral("Live");
            if (providerName == QStringLiteral("kick")) return QStringLiteral("Connected");
            return QStringLiteral("Ready to stream");
        }
        if (project_.activeStreamDestinationId == destination.id
            && (platformState_ == QStringLiteral("Authenticating")
                || platformState_ == QStringLiteral("Exchanging authorization code")
                || platformState_ == QStringLiteral("Loading YouTube account")))
            return platformState_ == QStringLiteral("Authenticating") ? QStringLiteral("Waiting for authorization") : platformState_;
        if (project_.activeStreamDestinationId == destination.id
            && (platformState_ == QStringLiteral("Authorization failed")
                || platformState_ == QStringLiteral("Token exchange failed")
                || platformState_ == QStringLiteral("Account loading failed")))
            return platformState_;
        if (project_.activeStreamDestinationId == destination.id && platformState_ == QStringLiteral("Error"))
            return QStringLiteral("Error");
        return QStringLiteral("Not connected");
    }
    return QStringLiteral("Not configured");
}

QString StudioController::providerSetupMessage(const QString &providerName) const
{
    if (providerName == QStringLiteral("youtube"))
        return QStringLiteral("This build needs a Google Desktop OAuth Client ID. For development, save oauth/googleClientId in the local preferences.ini file; a public release can provide it at build time.");
    if (providerName == QStringLiteral("twitch"))
        return QStringLiteral("This build needs a registered public Twitch application Client ID. Set STAX_TWITCH_CLIENT_ID for development or configure it at build time.");
    if (providerName == QStringLiteral("kick"))
        return QStringLiteral("Kick currently requires developer app credentials and a registered localhost callback. A consumer desktop release needs a secure token-exchange backend; no secret is bundled here.");
    return {};
}

void StudioController::openProviderSetupHelp(const QString &providerName) const
{
    QUrl url;
    if (providerName == QStringLiteral("youtube")) url = QUrl(QStringLiteral("https://developers.google.com/identity/protocols/oauth2/native-app"));
    if (providerName == QStringLiteral("twitch")) url = QUrl(QStringLiteral("https://dev.twitch.tv/docs/authentication/getting-tokens-oauth/"));
    if (providerName == QStringLiteral("kick")) url = QUrl(QStringLiteral("https://github.com/KickEngineering/KickDevDocs"));
    if (url.isValid()) QDesktopServices::openUrl(url);
}

QVariantList StudioController::streamingProviders() const
{
    QVariantList values;
    for (const StreamDestination &destination : project_.streamDestinations) {
        const PlatformCapabilities capabilities = platformCapabilities(destination.provider);
        values.append(QVariantMap{{"id", destination.id},
                                  {"provider", platformProviderName(destination.provider)},
                                  {"label", platformProviderLabel(destination.provider)},
                                  {"accountName", destination.accountName},
                                  {"enabled", destination.enabled},
                                  {"connected", destination.connected},
                                  {"connectionState", providerConnectionState(platformProviderName(destination.provider))},
                                  {"selected", destination.id == project_.activeStreamDestinationId},
                                  {"summary", redactedDestinationSummary(destination)},
                                  {"capabilities", capabilitiesToVariant(capabilities)}});
    }
    return values;
}

QVariantMap StudioController::providerCapabilities(const QString &providerName) const
{
    bool ok = false;
    const PlatformProviderId provider = platformProviderFromName(providerName, &ok);
    return capabilitiesToVariant(ok ? platformCapabilities(provider) : PlatformCapabilities{});
}

void StudioController::selectStreamDestination(const QString &destinationId)
{
    if (streamer_.isAcceptingInput() || !youtubeBroadcastId_.isEmpty() || twitchStreamActive_) {
        statusMessage_ = QStringLiteral("Stop the current stream before changing destinations.");
        emit statusMessageChanged();
        return;
    }
    for (const StreamDestination &destination : project_.streamDestinations) {
        if (destination.id != destinationId) continue;
        project_.activeStreamDestinationId = destinationId;
        save();
        emit projectChanged();
        const QString provider = platformProviderName(destination.provider);
        const QString connection = providerConnectionState(provider);
        setPlatformState(connection, connection == QStringLiteral("Not configured")
            ? providerSetupMessage(provider)
            : destination.connected ? QStringLiteral("Account connected.") : QStringLiteral("Connect an account to continue."));
        return;
    }
}

void StudioController::setStreamConfiguration(const QString &serverUrl, const QString &streamKey, const int videoBitrateKbps, const int audioBitrateKbps, const int frameRate)
{
    project_.streamSettings.serverUrl = serverUrl.trimmed();
    project_.streamSettings.videoBitrateKbps = qBound(300, videoBitrateKbps, 50000);
    project_.streamSettings.audioBitrateKbps = qBound(64, audioBitrateKbps, 512);
    project_.streamSettings.frameRate = qBound(1, frameRate, 60);
    streamKey_ = streamKey;
    for (StreamDestination &destination : project_.streamDestinations) {
        if (destination.provider == PlatformProviderId::CustomRtmp) {
            destination.configuration.insert(QStringLiteral("serverUrl"), project_.streamSettings.serverUrl);
            destination.connected = true;
        }
    }
    save();
    emit streamKeyChanged();
    emit projectChanged();
}

void StudioController::toggleStreaming()
{
    const StreamingState state = streamer_.state();
    if (state == StreamingState::Connecting || state == StreamingState::Live || state == StreamingState::Reconnecting || state == StreamingState::Stopping) {
        streamer_.stop();
        updateProgramAudioSink();
        return;
    }
    const StreamDestination *activeDestination = nullptr;
    for (const StreamDestination &destination : project_.streamDestinations)
        if (destination.id == project_.activeStreamDestinationId) activeDestination = &destination;
    if (activeDestination && activeDestination->provider != PlatformProviderId::CustomRtmp) {
        statusMessage_ = QStringLiteral("%1 account setup is prepared, but live API start is not enabled until OAuth credentials are configured. Custom RTMP remains available.").arg(platformProviderLabel(activeDestination->provider));
        emit statusMessageChanged();
        return;
    }
    StreamSettings settings = project_.streamSettings;
    settings.streamKey = streamKey_;
    audioSources_.discardPendingBlocks();
    updateProgramAudioSink();
    if (!streamer_.start(settings, {project_.programResolution.width, project_.programResolution.height})) {
        statusMessage_ = QStringLiteral("Unable to start streaming.");
        emit statusMessageChanged();
    } else {
        updateProgramAudioSink();
    }
}

StreamDestination *StudioController::youtubeDestination()
{
    for (StreamDestination &destination : project_.streamDestinations)
        if (destination.provider == PlatformProviderId::YouTube) return &destination;
    return nullptr;
}

QVariantMap StudioController::activeStreamMetadata() const
{
    for (const StreamDestination &destination : project_.streamDestinations)
        if (destination.id == project_.activeStreamDestinationId) return metadataToVariant(destination.metadata);
    return {};
}

QString StudioController::selectedYouTubeBroadcastId() const
{
    for (const StreamDestination &destination : project_.streamDestinations)
        if (destination.provider == PlatformProviderId::YouTube)
            return destination.configuration.value(QStringLiteral("broadcastId")).toString();
    return {};
}

void StudioController::setPlatformState(const QString &state, const QString &message)
{
    platformState_ = state;
    if (!message.isEmpty()) {
        statusMessage_ = message;
        emit statusMessageChanged();
    }
    emit platformStateChanged();
}

void StudioController::connectYouTube()
{
    if (!youtube_ || youtube_->clientId().isEmpty()) {
        setPlatformState(QStringLiteral("Not configured"), providerSetupMessage(QStringLiteral("youtube")));
        return;
    }
    if (youtube_->hasCredentials()) { loadYouTubeChannel(); return; }
    OAuthRequest request;
    request.authorizationEndpoint = QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth");
    request.clientId = youtube_->clientId();
    request.scope = QStringLiteral("https://www.googleapis.com/auth/youtube");
    request.extraParameters = {{QStringLiteral("access_type"), QStringLiteral("offline")},
                               {QStringLiteral("prompt"), QStringLiteral("consent")}};
    QString error;
    const QUrl url = oauth_.begin(request, &error);
    if (!url.isValid()) { setPlatformState(QStringLiteral("Error"), error); return; }
    if (!QDesktopServices::openUrl(url)) {
        oauth_.cancel();
        setPlatformState(QStringLiteral("Error"), QStringLiteral("Unable to open your system browser for YouTube sign-in."));
        return;
    }
    setPlatformState(QStringLiteral("Authenticating"), QStringLiteral("Complete YouTube sign-in in your browser."));
}

void StudioController::cancelYouTubeConnection()
{
    oauth_.cancel();
    setPlatformState(QStringLiteral("Idle"), QStringLiteral("YouTube sign-in cancelled."));
}

void StudioController::disconnectYouTube()
{
    if (!youtube_) return;
    if (!youtubeBroadcastId_.isEmpty()) stopYouTubeStream();
    oauth_.cancel();
    youtube_->disconnect();
    if (StreamDestination *destination = youtubeDestination()) {
        destination->connected = false;
        destination->accountName.clear();
        destination->configuration.remove(QStringLiteral("channelId"));
    }
    youtubeBroadcasts_.clear();
    youtubeCategories_.clear();
    save();
    emit projectChanged();
    setPlatformState(QStringLiteral("Idle"), QStringLiteral("YouTube account disconnected."));
}

void StudioController::loadYouTubeChannel()
{
    if (!youtube_) return;
    setPlatformState(QStringLiteral("Loading YouTube account"), QStringLiteral("Authorization succeeded. Loading your YouTube channel identity..."));
    youtube_->loadOwnChannel([this](const QString &id, const QString &name, const QString &error) {
        if (!error.isEmpty()) { setPlatformState(QStringLiteral("Account loading failed"), error); return; }
        if (StreamDestination *destination = youtubeDestination()) {
            destination->connected = true;
            destination->accountName = name;
            destination->configuration.insert(QStringLiteral("channelId"), id);
        }
        save();
        emit projectChanged();
        setPlatformState(QStringLiteral("Ready"), QStringLiteral("Connected to YouTube channel %1.").arg(name));
        refreshYouTubeOptions();
    });
}

void StudioController::setActiveStreamMetadata(const QVariantMap &values)
{
    for (StreamDestination &destination : project_.streamDestinations) {
        if (destination.id != project_.activeStreamDestinationId) continue;
        destination.metadata = metadataFromVariant(values);
        save();
        emit projectChanged();
        return;
    }
}

void StudioController::selectYouTubeBroadcast(const QString &broadcastId)
{
    StreamDestination *destination = youtubeDestination();
    if (!destination) return;
    destination->configuration.insert(QStringLiteral("broadcastId"), broadcastId);
    if (broadcastId.isEmpty()) {
        destination->metadata = PlatformMetadata{};
        destination->metadata.title = QStringLiteral("StaxStudio Live");
        save();
        emit projectChanged();
        return;
    }
    for (const QVariant &candidate : youtubeBroadcasts_) {
        const QVariantMap item = candidate.toMap();
        if (item.value(QStringLiteral("id")).toString() != broadcastId) continue;
        destination->metadata.title = item.value(QStringLiteral("title")).toString();
        destination->metadata.description = item.value(QStringLiteral("description")).toString();
        destination->metadata.privacy = item.value(QStringLiteral("privacy")).toString();
        destination->metadata.categoryId = item.value(QStringLiteral("categoryId")).toString();
        destination->metadata.latency = item.value(QStringLiteral("latency")).toString();
        destination->metadata.madeForKids = item.value(QStringLiteral("madeForKids")).toBool();
        destination->metadata.enableDvr = item.value(QStringLiteral("enableDvr")).toBool();
        destination->metadata.autoStart = item.value(QStringLiteral("autoStart")).toBool();
        destination->metadata.autoStop = item.value(QStringLiteral("autoStop")).toBool();
        destination->metadata.scheduledStartTime = QDateTime::fromString(item.value(QStringLiteral("scheduledStartTime")).toString(), Qt::ISODate);
        destination->metadata.scheduleForLater = destination->metadata.scheduledStartTime.isValid();
        destination->metadata.thumbnailPath.clear();
        break;
    }
    save();
    emit projectChanged();
}

void StudioController::refreshYouTubeOptions()
{
    if (!youtube_ || !youtube_->hasCredentials()) return;
    QString region = QLocale::system().name().section(QLatin1Char('_'), 1, 1).toUpper();
    if (region.size() != 2) region = QStringLiteral("US");
    youtube_->get(YouTubeClient::apiUrl(QStringLiteral("videoCategories"),
                      {{"part", QStringLiteral("snippet")}, {"regionCode", region}}),
                  [this](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) { setPlatformState(QStringLiteral("Error"), error); return; }
        youtubeCategories_.clear();
        for (const QJsonValue &value : json.value(QStringLiteral("items")).toArray()) {
            const QJsonObject item = value.toObject();
            const QJsonObject snippet = item.value(QStringLiteral("snippet")).toObject();
            if (!snippet.value(QStringLiteral("assignable")).toBool(true)) continue;
            youtubeCategories_.append(QVariantMap{{"id", item.value(QStringLiteral("id")).toString()},
                                                 {"title", snippet.value(QStringLiteral("title")).toString()}});
        }
        emit projectChanged();
    });
    youtubeBroadcasts_.clear();
    youtubeBroadcastPageTokens_.clear();
    const quint64 generation = ++youtubeBroadcastRefreshGeneration_;
    loadYouTubeBroadcastPage({}, generation);
}

void StudioController::loadYouTubeBroadcastPage(const QString &pageToken, const quint64 generation)
{
    if (!youtube_ || generation != youtubeBroadcastRefreshGeneration_) return;
    if (!pageToken.isEmpty()) {
        if (youtubeBroadcastPageTokens_.contains(pageToken)) {
            setPlatformState(QStringLiteral("Broadcast refresh failed"),
                             QStringLiteral("YouTube liveBroadcasts.list returned a repeated page token."));
            return;
        }
        youtubeBroadcastPageTokens_.insert(pageToken);
    }
    youtube_->get(YouTubeClient::apiUrl(QStringLiteral("liveBroadcasts"),
                                       YouTubeProvider::broadcastListQuery(pageToken)),
                  [this, generation](const QJsonObject &json, const QString &error) {
        if (generation != youtubeBroadcastRefreshGeneration_) return;
        if (!error.isEmpty()) {
            setPlatformState(QStringLiteral("Broadcast refresh failed"),
                             QStringLiteral("YouTube liveBroadcasts.list failed: %1").arg(error));
            return;
        }
        for (const QVariant &candidate : YouTubeProvider::eligibleBroadcasts(json)) {
            const QString id = candidate.toMap().value(QStringLiteral("id")).toString();
            bool duplicate = false;
            for (const QVariant &existing : youtubeBroadcasts_) {
                if (existing.toMap().value(QStringLiteral("id")).toString() == id) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) youtubeBroadcasts_.append(candidate);
        }
        emit projectChanged();
        const QString nextPageToken = json.value(QStringLiteral("nextPageToken")).toString();
        if (!nextPageToken.isEmpty()) {
            loadYouTubeBroadcastPage(nextPageToken, generation);
            return;
        }
        const StreamDestination *destination = youtubeDestination();
        if (destination && destination->connected && youtubeBroadcastId_.isEmpty()
            && !streamer_.isAcceptingInput()) {
            setPlatformState(QStringLiteral("Ready"), QStringLiteral("YouTube broadcasts refreshed."));
        }
    });
}

void StudioController::goLiveYouTube(const bool recordWhileStreaming)
{
    StreamDestination *destination = youtubeDestination();
    if (!youtube_ || !destination || !destination->connected || !youtube_->hasCredentials()) {
        setPlatformState(QStringLiteral("Error"), QStringLiteral("Connect your YouTube channel before going live."));
        return;
    }
    if (streamer_.isAcceptingInput() || !youtubeBroadcastId_.isEmpty() || twitchStreamActive_) return;
    if (destination->metadata.title.trimmed().isEmpty()) {
        setPlatformState(QStringLiteral("Error"), QStringLiteral("Enter a broadcast title."));
        return;
    }
    const QString existingId = selectedYouTubeBroadcastId();
    if (existingId.isEmpty() && destination->metadata.scheduleForLater
        && (!destination->metadata.scheduledStartTime.isValid()
            || destination->metadata.scheduledStartTime.toUTC() <= QDateTime::currentDateTimeUtc().addSecs(60))) {
        setPlatformState(QStringLiteral("Error"), QStringLiteral("Enter a valid UTC start time at least one minute in the future."));
        return;
    }
    recordWithYouTube_ = recordWhileStreaming;
    youtubeTransitionRequested_ = false;
    if (!existingId.isEmpty()) {
        bool eligible = false;
        for (const QVariant &value : youtubeBroadcasts_)
            if (value.toMap().value(QStringLiteral("id")).toString() == existingId) eligible = true;
        if (!eligible) { setPlatformState(QStringLiteral("Error"), QStringLiteral("Refresh and select an available YouTube broadcast.")); return; }
        setPlatformState(QStringLiteral("Preparing"), QStringLiteral("Preparing the selected YouTube broadcast..."));
        prepareYouTubeStream(existingId);
        return;
    }
    setPlatformState(QStringLiteral("Creating Broadcast"), QStringLiteral("Creating a YouTube broadcast..."));
    const QJsonObject body = QJsonObject::fromVariantMap(YouTubeProvider::createBroadcastRequest(destination->metadata));
    youtube_->post(YouTubeClient::apiUrl(QStringLiteral("liveBroadcasts"),
                       {{"part", QStringLiteral("snippet,status,contentDetails")}}), body,
                   [this](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) { setPlatformState(QStringLiteral("Error"), error); return; }
        const QString id = json.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) { setPlatformState(QStringLiteral("Error"), QStringLiteral("YouTube did not return a broadcast ID.")); return; }
        const StreamDestination *destination = youtubeDestination();
        if (destination && destination->metadata.scheduleForLater) {
            const auto finishScheduling = [this] {
                refreshYouTubeOptions();
                setPlatformState(QStringLiteral("Scheduled"), QStringLiteral("YouTube broadcast scheduled. Select it later to go live."));
            };
            const QString thumbnail = destination->metadata.thumbnailPath;
            if (thumbnail.isEmpty()) {
                finishScheduling();
            } else {
                youtube_->uploadThumbnail(id, thumbnail, [this, finishScheduling](const QJsonObject &, const QString &uploadError) {
                    if (!uploadError.isEmpty()) { setPlatformState(QStringLiteral("Error"), uploadError); return; }
                    finishScheduling();
                });
            }
            return;
        }
        prepareYouTubeStream(id);
    });
}

void StudioController::prepareYouTubeStream(const QString &broadcastId)
{
    youtubeBroadcastId_ = broadcastId;
    emit platformStateChanged();
    QString boundStreamId;
    for (const QVariant &value : youtubeBroadcasts_) {
        const QVariantMap broadcast = value.toMap();
        if (broadcast.value(QStringLiteral("id")).toString() == broadcastId)
            boundStreamId = broadcast.value(QStringLiteral("boundStreamId")).toString();
    }
    if (!boundStreamId.isEmpty()) {
        youtubeStreamId_ = boundStreamId;
        youtube_->get(YouTubeClient::apiUrl(QStringLiteral("liveStreams"),
                          {{"part", QStringLiteral("cdn,status")}, {"id", boundStreamId}}),
                      [this, broadcastId](const QJsonObject &json, const QString &error) {
            if (!error.isEmpty()) { youtubeBroadcastId_.clear(); setPlatformState(QStringLiteral("Error"), error); return; }
            const QJsonArray items = json.value(QStringLiteral("items")).toArray();
            if (items.isEmpty()) { youtubeBroadcastId_.clear(); setPlatformState(QStringLiteral("Error"), QStringLiteral("The selected broadcast's stream is unavailable.")); return; }
            startYouTubeIngest(broadcastId, items.at(0).toObject());
        });
        return;
    }
    setPlatformState(QStringLiteral("Creating Stream"), QStringLiteral("Creating a YouTube ingest stream..."));
    const QJsonObject body = QJsonObject::fromVariantMap(YouTubeProvider::createStreamRequest(QStringLiteral("StaxStudio Stream")));
    youtube_->post(YouTubeClient::apiUrl(QStringLiteral("liveStreams"),
                       {{"part", QStringLiteral("snippet,cdn,status")}}), body,
                   [this, broadcastId](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) { youtubeBroadcastId_.clear(); setPlatformState(QStringLiteral("Error"), error); return; }
        const QString streamId = json.value(QStringLiteral("id")).toString();
        if (streamId.isEmpty()) { youtubeBroadcastId_.clear(); setPlatformState(QStringLiteral("Error"), QStringLiteral("YouTube did not return an ingest stream.")); return; }
        youtubeStreamId_ = streamId;
        youtube_->postEmpty(QUrl(YouTubeProvider::bindBroadcastUrl(broadcastId, streamId)),
                            [this, broadcastId, json](const QJsonObject &, const QString &bindError) {
            if (!bindError.isEmpty()) { youtubeBroadcastId_.clear(); setPlatformState(QStringLiteral("Error"), bindError); return; }
            startYouTubeIngest(broadcastId, json);
        });
    });
}

void StudioController::startYouTubeIngest(const QString &broadcastId, const QJsonObject &stream)
{
    const QJsonObject ingest = stream.value(QStringLiteral("cdn")).toObject()
                                     .value(QStringLiteral("ingestionInfo")).toObject();
    QString server = ingest.value(QStringLiteral("rtmpsIngestionAddress")).toString();
    if (server.isEmpty()) server = ingest.value(QStringLiteral("ingestionAddress")).toString();
    const QString key = ingest.value(QStringLiteral("streamName")).toString();
    if ((!server.startsWith(QStringLiteral("rtmp://")) && !server.startsWith(QStringLiteral("rtmps://"))) || key.isEmpty()) {
        youtubeBroadcastId_.clear();
        setPlatformState(QStringLiteral("Error"), QStringLiteral("YouTube did not provide usable RTMP ingestion details."));
        return;
    }
    const auto start = [this, server, key]() {
        setPlatformState(QStringLiteral("Connecting"), QStringLiteral("Connecting to YouTube ingest..."));
        StreamSettings settings = project_.streamSettings;
        settings.serverUrl = server;
        settings.streamKey = key;
        audioSources_.discardPendingBlocks();
        if (!streamer_.start(settings, project_.programResolution.size())) {
            youtubeBroadcastId_.clear();
            setPlatformState(QStringLiteral("Error"), QStringLiteral("Unable to start the YouTube RTMP output."));
            return;
        }
        updateProgramAudioSink();
        if (recordWithYouTube_ && recorder_.state() != RecordingState::Recording) {
            RecordingSettings recording;
            recording.detailedDiagnostics = detailedRecordingDiagnostics_;
            recordingStartedForStream_ = recorder_.start(recording, project_.programResolution.size());
            updateProgramAudioSink();
            if (!recordingStartedForStream_) {
                statusMessage_ = QStringLiteral("YouTube ingest started, but local recording could not start.");
                emit statusMessageChanged();
            }
        }
    };
    const StreamDestination *destination = youtubeDestination();
    const QString thumbnail = destination ? destination->metadata.thumbnailPath : QString{};
    if (thumbnail.isEmpty()) { start(); return; }
    youtube_->uploadThumbnail(broadcastId, thumbnail, [this, start](const QJsonObject &, const QString &error) {
        if (!error.isEmpty()) { youtubeBroadcastId_.clear(); setPlatformState(QStringLiteral("Error"), error); return; }
        start();
    });
}

void StudioController::checkYouTubeReady()
{
    if (youtubeChecking_ || youtubeBroadcastId_.isEmpty() || streamer_.state() != StreamingState::Live) return;
    youtubeChecking_ = true;
    youtube_->get(YouTubeClient::apiUrl(QStringLiteral("liveStreams"),
                      {{"part", QStringLiteral("status")}, {"id", youtubeStreamId_}}),
                  [this](const QJsonObject &stream, const QString &error) {
        if (!error.isEmpty()) { youtubeChecking_ = false; youtubePoll_.stop(); setPlatformState(QStringLiteral("Error"), error); return; }
        const QJsonArray streams = stream.value(QStringLiteral("items")).toArray();
        const QString ingestState = streams.isEmpty() ? QString{} : streams.at(0).toObject()
            .value(QStringLiteral("status")).toObject().value(QStringLiteral("streamStatus")).toString();
        youtube_->get(YouTubeClient::apiUrl(QStringLiteral("liveBroadcasts"),
                          {{"part", QStringLiteral("status,contentDetails")}, {"id", youtubeBroadcastId_}}),
                      [this, ingestState](const QJsonObject &broadcast, const QString &broadcastError) {
            youtubeChecking_ = false;
            if (!broadcastError.isEmpty()) { youtubePoll_.stop(); setPlatformState(QStringLiteral("Error"), broadcastError); return; }
            const QJsonArray broadcasts = broadcast.value(QStringLiteral("items")).toArray();
            if (broadcasts.isEmpty()) { youtubePoll_.stop(); setPlatformState(QStringLiteral("Error"), QStringLiteral("YouTube broadcast disappeared.")); return; }
            const QJsonObject item = broadcasts.at(0).toObject();
            const QString lifeCycle = item.value(QStringLiteral("status")).toObject()
                                          .value(QStringLiteral("lifeCycleStatus")).toString();
            if (lifeCycle == QStringLiteral("live")) {
                setPlatformState(QStringLiteral("Live"), QStringLiteral("Live on YouTube."));
                return;
            }
            if (lifeCycle == QStringLiteral("complete") || lifeCycle == QStringLiteral("revoked")) {
                youtubePoll_.stop();
                setPlatformState(QStringLiteral("Error"), QStringLiteral("YouTube ended or revoked this broadcast."));
                return;
            }
            if (ingestState != QStringLiteral("active")) {
                setPlatformState(QStringLiteral("Waiting for YouTube"), QStringLiteral("Waiting for YouTube to receive the stream..."));
                return;
            }
            const bool autoStart = item.value(QStringLiteral("contentDetails")).toObject()
                                       .value(QStringLiteral("enableAutoStart")).toBool();
            if (autoStart || youtubeTransitionRequested_) {
                setPlatformState(QStringLiteral("Waiting for YouTube"), QStringLiteral("Waiting for YouTube broadcast readiness..."));
                return;
            }
            youtubeTransitionRequested_ = true;
            youtube_->postEmpty(QUrl(YouTubeProvider::transitionBroadcastUrl(youtubeBroadcastId_, QStringLiteral("live"))),
                                [this](const QJsonObject &, const QString &transitionError) {
                if (!transitionError.isEmpty()) { youtubePoll_.stop(); setPlatformState(QStringLiteral("Error"), transitionError); }
            });
        });
    });
}

void StudioController::stopYouTubeStream()
{
    youtubePoll_.stop();
    const QString broadcastId = std::exchange(youtubeBroadcastId_, {});
    emit platformStateChanged();
    youtubeStreamId_.clear();
    const bool wasLive = platformState_ == QStringLiteral("Live");
    setPlatformState(QStringLiteral("Stopping"));
    streamer_.stop();
    if (recordingStartedForStream_) { recorder_.stop(); recordingStartedForStream_ = false; }
    updateProgramAudioSink();
    if (wasLive && !broadcastId.isEmpty() && youtube_) {
        const StreamDestination *destination = youtubeDestination();
        if (destination && !destination->metadata.autoStop)
            youtube_->postEmpty(QUrl(YouTubeProvider::transitionBroadcastUrl(broadcastId, QStringLiteral("complete"))),
                                [this](const QJsonObject &, const QString &error) {
                if (!error.isEmpty()) setPlatformState(QStringLiteral("Error"), error);
            });
    }
    setPlatformState(QStringLiteral("Idle"), QStringLiteral("YouTube stream stopped."));
}

StreamDestination *StudioController::twitchDestination()
{
    for (StreamDestination &destination : project_.streamDestinations)
        if (destination.provider == PlatformProviderId::Twitch) return &destination;
    return nullptr;
}

void StudioController::connectTwitch()
{
    if (!twitch_ || twitch_->clientId().isEmpty()) {
        setPlatformState(QStringLiteral("Not configured"), providerSetupMessage(QStringLiteral("twitch")));
        return;
    }
    if (twitch_->hasCredentials()) { loadTwitchAccount(); return; }
    setPlatformState(QStringLiteral("Authenticating"), QStringLiteral("Requesting Twitch sign-in code..."));
    twitch_->beginDeviceAuthorization([this](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) { setPlatformState(QStringLiteral("Error"), error); return; }
        const QUrl verification(json.value(QStringLiteral("verification_uri")).toString());
        const QString host = verification.host().toLower();
        if (verification.scheme() != QStringLiteral("https") ||
            (host != QStringLiteral("twitch.tv") && !host.endsWith(QStringLiteral(".twitch.tv")))) {
            setPlatformState(QStringLiteral("Error"), QStringLiteral("Twitch returned an unexpected sign-in address."));
            return;
        }
        twitchDeviceCode_ = json.value(QStringLiteral("device_code")).toString();
        twitchUserCode_ = json.value(QStringLiteral("user_code")).toString();
        twitchAuthorizationDeadlineMs_ = QDateTime::currentMSecsSinceEpoch() + qMax(60, json.value(QStringLiteral("expires_in")).toInt(600)) * 1000LL;
        twitchAuthPoll_.setInterval(qMax(5, json.value(QStringLiteral("interval")).toInt(5)) * 1000);
        if (!QDesktopServices::openUrl(verification)) {
            cancelTwitchConnection();
            setPlatformState(QStringLiteral("Error"), QStringLiteral("Unable to open Twitch sign-in in your browser."));
            return;
        }
        twitchAuthPoll_.start();
        emit platformStateChanged();
        setPlatformState(QStringLiteral("Authenticating"), QStringLiteral("Approve Twitch access in your browser, then enter the displayed code."));
    });
}

void StudioController::pollTwitchConnection()
{
    if (twitchDeviceCode_.isEmpty() || !twitch_) return;
    if (QDateTime::currentMSecsSinceEpoch() >= twitchAuthorizationDeadlineMs_) {
        cancelTwitchConnection();
        setPlatformState(QStringLiteral("Error"), QStringLiteral("Twitch sign-in timed out. Try again."));
        return;
    }
    twitchAuthPoll_.stop();
    const QString code = twitchDeviceCode_;
    twitch_->pollDeviceAuthorization(code, [this, code](const QJsonObject &json, const QString &error) {
        if (code != twitchDeviceCode_) return;
        if (!error.isEmpty()) {
            cancelTwitchConnection();
            setPlatformState(QStringLiteral("Error"), error);
            return;
        }
        if (json.value(QStringLiteral("pending")).toBool()) {
            if (json.value(QStringLiteral("slowDown")).toBool()) twitchAuthPoll_.setInterval(twitchAuthPoll_.interval() + 5000);
            twitchAuthPoll_.start();
            return;
        }
        twitchDeviceCode_.clear();
        twitchUserCode_.clear();
        emit platformStateChanged();
        loadTwitchAccount();
    });
}

void StudioController::cancelTwitchConnection()
{
    twitchAuthPoll_.stop();
    twitchDeviceCode_.clear();
    twitchUserCode_.clear();
    setPlatformState(QStringLiteral("Idle"), QStringLiteral("Twitch sign-in cancelled."));
}

void StudioController::disconnectTwitch()
{
    if (!twitch_) return;
    if (twitchStreamActive_) stopTwitchStream();
    cancelTwitchConnection();
    twitch_->disconnect();
    if (StreamDestination *destination = twitchDestination()) {
        destination->connected = false;
        destination->accountName.clear();
        destination->configuration.remove(QStringLiteral("broadcasterId"));
    }
    twitchCategories_.clear();
    save();
    emit projectChanged();
    setPlatformState(QStringLiteral("Idle"), QStringLiteral("Twitch account disconnected."));
}

void StudioController::loadTwitchAccount()
{
    if (!twitch_) return;
    setPlatformState(QStringLiteral("Authenticating"), QStringLiteral("Loading your Twitch channel..."));
    twitch_->validate([this](const QJsonObject &validation, const QString &error) {
        if (!error.isEmpty()) { setPlatformState(QStringLiteral("Error"), error); return; }
        const QString id = validation.value(QStringLiteral("user_id")).toString();
        if (id.isEmpty() || validation.value(QStringLiteral("client_id")).toString() != twitch_->clientId()) {
            setPlatformState(QStringLiteral("Error"), QStringLiteral("Twitch returned an invalid account identity."));
            return;
        }
        twitch_->get(TwitchClient::apiUrl(QStringLiteral("users"), {{"id", id}}), [this, id](const QJsonObject &users, const QString &userError) {
            if (!userError.isEmpty()) { setPlatformState(QStringLiteral("Error"), userError); return; }
            const QJsonArray data = users.value(QStringLiteral("data")).toArray();
            const QJsonObject user = data.isEmpty() ? QJsonObject{} : data.first().toObject();
            if (user.value(QStringLiteral("id")).toString() != id) {
                setPlatformState(QStringLiteral("Error"), QStringLiteral("Twitch channel could not be verified."));
                return;
            }
            if (StreamDestination *destination = twitchDestination()) {
                destination->connected = true;
                destination->accountName = user.value(QStringLiteral("display_name")).toString();
                destination->configuration.insert(QStringLiteral("broadcasterId"), id);
            }
            save();
            emit projectChanged();
            setPlatformState(QStringLiteral("Ready"), QStringLiteral("Connected to Twitch."));
        });
    });
}

void StudioController::searchTwitchCategories(const QString &query)
{
    if (!twitch_ || !twitch_->hasCredentials() || query.trimmed().isEmpty()) return;
    twitch_->get(TwitchClient::apiUrl(QStringLiteral("search/categories"), {{"query", query.trimmed()}, {"first", QStringLiteral("20")}}),
                 [this](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) { setPlatformState(QStringLiteral("Error"), error); return; }
        twitchCategories_.clear();
        for (const QJsonValue &item : json.value(QStringLiteral("data")).toArray()) {
            const QJsonObject category = item.toObject();
            twitchCategories_.append(QVariantMap{{"id", category.value(QStringLiteral("id")).toString()},
                                                {"name", category.value(QStringLiteral("name")).toString()}});
        }
        emit projectChanged();
    });
}

void StudioController::goLiveTwitch(const bool recordWhileStreaming)
{
    StreamDestination *destination = twitchDestination();
    if (!destination || !destination->connected || !twitch_ || !twitch_->hasCredentials()) {
        setPlatformState(QStringLiteral("Error"), QStringLiteral("Connect your Twitch channel before going live."));
        return;
    }
    if (streamer_.isAcceptingInput() || !youtubeBroadcastId_.isEmpty() || twitchStreamActive_) {
        setPlatformState(QStringLiteral("Error"), QStringLiteral("Stop the current stream before starting another."));
        return;
    }
    const QString broadcasterId = destination->configuration.value(QStringLiteral("broadcasterId")).toString();
    if (broadcasterId.isEmpty() || destination->metadata.title.trimmed().isEmpty()) {
        setPlatformState(QStringLiteral("Error"), QStringLiteral("Twitch channel or stream title is missing."));
        return;
    }
    recordWithTwitch_ = recordWhileStreaming;
    setPlatformState(QStringLiteral("Preparing"), QStringLiteral("Updating your Twitch channel..."));
    twitch_->updateChannel(broadcasterId, destination->metadata, [this, broadcasterId](const QJsonObject &, const QString &error) {
        if (!error.isEmpty()) { setPlatformState(QStringLiteral("Error"), error); return; }
        twitch_->get(TwitchClient::apiUrl(QStringLiteral("streams/key"), {{"broadcaster_id", broadcasterId}}),
                     [this](const QJsonObject &json, const QString &keyError) {
            if (!keyError.isEmpty()) { setPlatformState(QStringLiteral("Error"), keyError); return; }
            const QJsonArray keys = json.value(QStringLiteral("data")).toArray();
            const QString streamKey = keys.isEmpty() ? QString{} : keys.first().toObject().value(QStringLiteral("stream_key")).toString();
            if (streamKey.isEmpty()) { setPlatformState(QStringLiteral("Error"), QStringLiteral("Twitch did not provide a stream key.")); return; }
            PlatformHttpRequest request;
            request.url = QUrl(QStringLiteral("https://ingest.twitch.tv/ingests"));
            platformHttp_.send(std::move(request), [this, streamKey](PlatformHttpResult result) {
                if (!result.ok()) { setPlatformState(QStringLiteral("Error"), QStringLiteral("Unable to load Twitch ingest servers.")); return; }
                const QJsonArray ingests = QJsonDocument::fromJson(result.body).object().value(QStringLiteral("ingests")).toArray();
                QString urlTemplate;
                for (const QJsonValue &value : ingests) {
                    const QJsonObject ingest = value.toObject();
                    if (ingest.value(QStringLiteral("default")).toBool()) {
                        urlTemplate = ingest.value(QStringLiteral("url_template")).toString();
                        break;
                    }
                }
                if (urlTemplate.isEmpty() && !ingests.isEmpty()) urlTemplate = ingests.first().toObject().value(QStringLiteral("url_template")).toString();
                const int marker = urlTemplate.indexOf(QStringLiteral("{stream_key}"));
                const QString server = marker > 0 ? urlTemplate.left(marker) : QString{};
                if (!server.startsWith(QStringLiteral("rtmp://")) && !server.startsWith(QStringLiteral("rtmps://"))) {
                    setPlatformState(QStringLiteral("Error"), QStringLiteral("Twitch returned no usable ingest server."));
                    return;
                }
                StreamSettings settings = project_.streamSettings;
                settings.serverUrl = server;
                settings.streamKey = streamKey;
                setPlatformState(QStringLiteral("Connecting"), QStringLiteral("Connecting to Twitch ingest..."));
                audioSources_.discardPendingBlocks();
                twitchStreamActive_ = true;
                emit platformStateChanged();
                if (!streamer_.start(settings, project_.programResolution.size())) {
                    twitchStreamActive_ = false;
                    setPlatformState(QStringLiteral("Error"), QStringLiteral("Unable to start Twitch RTMP output."));
                    return;
                }
                updateProgramAudioSink();
                if (recordWithTwitch_ && recorder_.state() != RecordingState::Recording) {
                    RecordingSettings recording;
                    recording.detailedDiagnostics = detailedRecordingDiagnostics_;
                    twitchRecordingStarted_ = recorder_.start(recording, project_.programResolution.size());
                    updateProgramAudioSink();
                    if (!twitchRecordingStarted_) {
                        statusMessage_ = QStringLiteral("Twitch ingest started, but local recording could not start.");
                        emit statusMessageChanged();
                    }
                }
            });
        });
    });
}

void StudioController::checkTwitchReady()
{
    if (!twitchStreamActive_ || twitchChecking_ || streamer_.state() != StreamingState::Live || !twitch_) return;
    const StreamDestination *destination = twitchDestination();
    const QString id = destination ? destination->configuration.value(QStringLiteral("broadcasterId")).toString() : QString{};
    if (id.isEmpty()) return;
    twitchChecking_ = true;
    twitch_->get(TwitchClient::apiUrl(QStringLiteral("streams"), {{"user_id", id}}), [this, id](const QJsonObject &json, const QString &error) {
        twitchChecking_ = false;
        if (!twitchStreamActive_) return;
        if (!error.isEmpty()) { twitchLivePoll_.stop(); setPlatformState(QStringLiteral("Error"), error); return; }
        for (const QJsonValue &entry : json.value(QStringLiteral("data")).toArray()) {
            if (entry.toObject().value(QStringLiteral("user_id")).toString() == id) {
                setPlatformState(QStringLiteral("Live"), QStringLiteral("Live on Twitch."));
                return;
            }
        }
        setPlatformState(QStringLiteral("Waiting for Twitch"), QStringLiteral("Waiting for Twitch to report the stream as live..."));
    });
}

void StudioController::stopTwitchStream()
{
    twitchLivePoll_.stop();
    twitchStreamActive_ = false;
    emit platformStateChanged();
    setPlatformState(QStringLiteral("Stopping"));
    streamer_.stop();
    if (twitchRecordingStarted_) { recorder_.stop(); twitchRecordingStarted_ = false; }
    updateProgramAudioSink();
    setPlatformState(QStringLiteral("Idle"), QStringLiteral("Twitch stream stopped."));
}

StreamDestination *StudioController::kickDestination()
{
    for (StreamDestination &destination : project_.streamDestinations)
        if (destination.provider == PlatformProviderId::Kick) return &destination;
    return nullptr;
}

void StudioController::connectKick()
{
    bool validPort = false;
    const quint16 port = qEnvironmentVariable("STAX_KICK_REDIRECT_PORT").toUShort(&validPort);
    if (!kick_ || !kick_->configured() || !validPort || port == 0) {
        setPlatformState(QStringLiteral("Not configured"), providerSetupMessage(QStringLiteral("kick")));
        return;
    }
    if (kick_->hasCredentials()) { loadKickAccount(); return; }
    OAuthRequest request;
    request.authorizationEndpoint = QStringLiteral("https://id.kick.com/oauth/authorize");
    request.clientId = kick_->clientId();
    request.scope = KickProvider::oauthScopes().join(QLatin1Char(' '));
    QString error;
    const QUrl url = kickOauth_.begin(request, &error, QStringLiteral("localhost"), port);
    if (!url.isValid()) { setPlatformState(QStringLiteral("Error"), error); return; }
    if (!QDesktopServices::openUrl(url)) {
        kickOauth_.cancel();
        setPlatformState(QStringLiteral("Error"), QStringLiteral("Unable to open Kick sign-in in your browser."));
        return;
    }
    setPlatformState(QStringLiteral("Authenticating"), QStringLiteral("Complete Kick sign-in in your browser."));
}

void StudioController::cancelKickConnection()
{
    kickOauth_.cancel();
    setPlatformState(QStringLiteral("Idle"), QStringLiteral("Kick sign-in cancelled."));
}

void StudioController::disconnectKick()
{
    if (!kick_) return;
    kickOauth_.cancel();
    kick_->disconnect();
    if (StreamDestination *destination = kickDestination()) {
        destination->connected = false;
        destination->accountName.clear();
        destination->configuration.remove(QStringLiteral("userId"));
        destination->configuration.remove(QStringLiteral("channelSlug"));
    }
    kickCategories_.clear();
    save();
    emit projectChanged();
    setPlatformState(QStringLiteral("Idle"), QStringLiteral("Kick account disconnected."));
}

void StudioController::loadKickAccount()
{
    if (!kick_) return;
    setPlatformState(QStringLiteral("Authenticating"), QStringLiteral("Loading your Kick channel..."));
    kick_->get(KickClient::apiUrl(QStringLiteral("users")), [this](const QJsonObject &users, const QString &error) {
        if (!error.isEmpty()) { setPlatformState(QStringLiteral("Error"), error); return; }
        const QJsonArray data = users.value(QStringLiteral("data")).toArray();
        const QJsonObject user = data.isEmpty() ? QJsonObject{} : data.first().toObject();
        const qint64 id = user.value(QStringLiteral("user_id")).toInteger();
        const QString name = user.value(QStringLiteral("name")).toString();
        if (id <= 0 || name.isEmpty()) { setPlatformState(QStringLiteral("Error"), QStringLiteral("Kick account identity is unavailable.")); return; }
        kick_->get(KickClient::apiUrl(QStringLiteral("channels")), [this, id, name](const QJsonObject &channels, const QString &channelError) {
            if (!channelError.isEmpty()) { setPlatformState(QStringLiteral("Error"), channelError); return; }
            const QJsonArray data = channels.value(QStringLiteral("data")).toArray();
            QJsonObject channel;
            for (const QJsonValue &entry : data) {
                if (entry.toObject().value(QStringLiteral("broadcaster_user_id")).toInteger() == id) {
                    channel = entry.toObject();
                    break;
                }
            }
            if (channel.isEmpty()) { setPlatformState(QStringLiteral("Error"), QStringLiteral("Kick channel could not be verified.")); return; }
            if (StreamDestination *destination = kickDestination()) {
                destination->connected = true;
                destination->accountName = name;
                destination->configuration.insert(QStringLiteral("userId"), id);
                destination->configuration.insert(QStringLiteral("channelSlug"), channel.value(QStringLiteral("slug")).toString());
            }
            save();
            emit projectChanged();
            setPlatformState(QStringLiteral("Ready"), QStringLiteral("Connected to Kick."));
        });
    });
}

void StudioController::searchKickCategories(const QString &query)
{
    if (!kick_ || !kick_->hasCredentials() || query.trimmed().size() < 3) return;
    QUrl url(QStringLiteral("https://api.kick.com/public/v2/categories"));
    QUrlQuery params;
    params.addQueryItem(QStringLiteral("name"), query.trimmed());
    params.addQueryItem(QStringLiteral("limit"), QStringLiteral("20"));
    url.setQuery(params);
    kick_->get(url, [this](const QJsonObject &json, const QString &error) {
        if (!error.isEmpty()) { setPlatformState(QStringLiteral("Error"), error); return; }
        kickCategories_.clear();
        for (const QJsonValue &entry : json.value(QStringLiteral("data")).toArray()) {
            const QJsonObject category = entry.toObject();
            kickCategories_.append(QVariantMap{{"id", QString::number(category.value(QStringLiteral("id")).toInteger())},
                                               {"name", category.value(QStringLiteral("name")).toString()}});
        }
        emit projectChanged();
    });
}

void StudioController::updateKickChannel()
{
    const StreamDestination *destination = kickDestination();
    if (!destination || !destination->connected || !kick_) {
        setPlatformState(QStringLiteral("Error"), QStringLiteral("Connect Kick before updating its channel."));
        return;
    }
    if (destination->metadata.title.trimmed().isEmpty()) {
        setPlatformState(QStringLiteral("Error"), QStringLiteral("Enter a Kick stream title."));
        return;
    }
    setPlatformState(QStringLiteral("Preparing"), QStringLiteral("Updating Kick channel..."));
    kick_->updateChannel(destination->metadata, [this](const QJsonObject &, const QString &error) {
        if (!error.isEmpty()) { setPlatformState(QStringLiteral("Error"), error); return; }
        setPlatformState(QStringLiteral("Ready"), QStringLiteral("Kick channel metadata updated."));
    });
}

void StudioController::refresh()
{
    visualSources_.synchronizeSources(project_.sources);
    audioSources_.synchronizeSources(project_.sources);
    for (const MixerChannel &channel : project_.mixerChannels) audioSources_.setMixControls(channel.id, channel.volume, channel.muted);
    mixerModel_.synchronize();
    programEngine_.setScene(compositorLayers(), project_.programResolution.size());
    save();
    emit projectChanged();
}

void StudioController::save()
{
    QString error;
    if (!repository_.save(project_, &error)) { statusMessage_ = QStringLiteral("Unable to save studio changes: %1").arg(error); emit statusMessageChanged(); }
}

void StudioController::updateProgramAudioSink()
{
    const bool recording = recorder_.state() == RecordingState::Recording;
    const bool streaming = streamer_.isAcceptingInput();
    if (!recording && !streaming) {
        audioSources_.setRecordingSink({});
        return;
    }
    audioSources_.setRecordingSink([this](ProgramMixedAudioBlock block) {
        if (recorder_.state() == RecordingState::Recording)
            recorder_.submitProgramAudio({block.audio});
        if (streamer_.isAcceptingInput())
            streamer_.submitProgramAudio(std::move(block));
    });
}
