#include "core/project/StudioProject.h"
#include "core/audio/AudioProcessing.h"
#include "core/audio/ProgramAudioMixer.h"
#include "core/audio/wasapi/WasapiAudioSource.h"
#include "core/platform/CredentialStore.h"
#include "core/platform/OAuthPkce.h"
#include "core/platform/OAuthLoopback.h"
#include "core/platform/GoogleOAuthConfiguration.h"
#include "core/platform/PlatformHttp.h"
#include "core/platform/PlatformProviders.h"
#include "core/platform/PlatformTypes.h"
#include "core/platform/YouTubeClient.h"
#include "core/platform/TwitchClient.h"
#include "core/platform/KickClient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include "core/project/StudioRepository.h"
#include "core/render/CompositorScene.h"
#include "core/render/ProgramFrameMath.h"
#include "core/recorder/LocalRecorder.h"
#include "core/recorder/RecordingDiagnostics.h"
#include "core/streaming/StreamingOutput.h"
#include "core/streaming/StreamingTypes.h"
#include "ui/controllers/StudioModels.h"
#include "ui/controllers/DockLayout.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTcpSocket>
#include <QImage>
#include <QtTest>
#include <cmath>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

class StudioProjectTests final : public QObject
{
    Q_OBJECT
private slots:
    void createsAndSelectsScenes();
    void protectsValidSceneState();
    void reordersScenesAndItems();
    void managesSourceLayerState();
    void appliesAndPersistsTransformOperations();
    void composesVisibleLayersAtProgramResolution();
    void persistsAndRestoresProject();
    void recoversFromInvalidConfiguration();
    void recoversFromUnsupportedSchema();
    void notifiesSceneModelMutations();
    void notifiesSourceModelMutations();
    void keepsModelsConsistentThroughRepeatedLifecycleChanges();
    void normalizesAudioBlocks();
    void programMixerAlignsAndBoundsSources();
    void streamsThroughMockLifecycle();
    void streamFailureAndRedactionStayIsolated();
    void boundsStreamingQueues();
    void persistsStreamSettingsWithoutStreamKey();
    void providerCapabilitiesReflectOfficialDifferences();
    void generatesPkceAndRedactsSecrets();
    void resolvesLocalGoogleDesktopClientId();
    void storesCredentialsBehindAbstraction();
    void persistsDestinationsWithoutSecrets();
    void serializesYouTubeBroadcastRequests();
    void filtersYouTubeBroadcastListsLocally();
    void serializesTwitchAndKickMetadata();
    void validatesOAuthLoopbackState();
    void executesYouTubeLifecycleWithMockTransport();
    void sendsGoogleDesktopTokenCredentials();
    void reportsYouTubeOAuthFailuresSafely();
    void rejectsMissingYouTubeChannel();
    void reportsYouTubeCredentialPersistenceFailure();
    void refreshesYouTubeTokensWithoutPersistingThemInProject();
    void executesTwitchDeviceAndChannelLifecycle();
    void executesKickOAuthAndChannelLifecycle();
    void refreshesProviderTokensWithoutProjectSecrets();
    void providerFailuresDoNotExposeResponseSecrets();
    void recordsSimultaneousProgramAudio_data();
    void recordsSimultaneousProgramAudio();
    void optionalWasapiEndpointProbe();
    void convertsNativePcmAtUnity();
    void dockLayoutSupportsNestedMovesAndPersistence();
    void preservesLongRunAudioResampleCounts();
    void usesExactRecordingFrameClock();
    void keepsSixtySecondOutputClockIndependentOfPreview();
    void preservesUnityGainThroughProgramMix();
    void keepsNativePixelGeometrySeparateFromUiScaling();
    void fitsCaptureSourcesToTheProgramCanvas();
    void mapsImportedDxgiTextureRowsIntoProgramOrientation();
    void bestFitsPreviewWithoutChangingProgramSpace();
    void writesPlayableMkv();
    void writesCompleteSixtySecondMkvWithoutPreview();
    void keepsAacTimelineBoundToAudioTimestamps();
    void recordingDiagnosticsStayOutOfVideoFolder();
};

void StudioProjectTests::programMixerAlignsAndBoundsSources()
{
    ProgramAudioMixer mixer;
    mixer.reset(0);
    mixer.push("mic", {0,48000,1,QVector<float>(480,0.2f)});
    mixer.push("desktop", {5'000'000,48000,2,QVector<float>(960,0.3f)});
    // Duplicate the same microphone packet: it must not double that source.
    mixer.push("mic", {0,48000,1,QVector<float>(480,0.2f)});
    QVector<ProgramMixedAudioBlock> out;
    mixer.produceUntil(20'000'000,[&](auto block){out.append(std::move(block));});
    QCOMPARE(out.size(),2);
    for(int i=0;i<960;++i) {
        const float expected=i<240?0.2f:i<480?0.5f:i<720?0.3f:0;
        const auto &block=out[i/480].audio;
        QVERIFY(std::abs(block.samples[(i%480)*2]-expected)<1e-6f);
        QCOMPARE(block.samples[(i%480)*2],block.samples[(i%480)*2+1]);
    }
    mixer.push("late",{0,48000,1,QVector<float>(480,1)});
    mixer.push("future",{9'000'000'000LL,48000,1,QVector<float>(480,1)});
    QCOMPARE(mixer.diagnostics().value("lateSourceBlocks").toULongLong(),1ULL);
    QCOMPARE(mixer.diagnostics().value("rejectedSourceBlocks").toULongLong(),1ULL);
    mixer.reset(0);
    mixer.push("mic",{0,48000,1,QVector<float>(480,0.2f)});
    mixer.produceUntil(10'000'000,[&](auto block){QCOMPARE(block.audio.samples.first(),0.2f);});
    mixer.reset(0);
    AudioBlock surround{0,48000,3,QVector<float>(480*3)};
    for(int i=0;i<480;++i) {surround.samples[i*3]=0.2f;surround.samples[i*3+1]=0.3f;surround.samples[i*3+2]=0.9f;}
    mixer.push("existing-front-channel-mapping",std::move(surround));
    mixer.produceUntil(10'000'000,[&](auto block){QCOMPARE(block.audio.samples[0],0.2f);QCOMPARE(block.audio.samples[1],0.3f);});
}

void StudioProjectTests::streamsThroughMockLifecycle()
{
    StreamingOutput stream;
    StreamSettings settings;
    settings.serverUrl = QStringLiteral("staxmock://success");
    settings.streamKey = QStringLiteral("secret-key");
    settings.frameRate = 30;
    QVERIFY(stream.start(settings, {32, 32}));
    QTRY_COMPARE(stream.state(), StreamingState::Live);
    QImage image(32, 32, QImage::Format_ARGB32);
    image.fill(Qt::red);
    stream.submitVideoFrame({image, 0});
    stream.submitProgramAudio({{0, 48000, 2, QVector<float>(960, 0.1f)}});
    QTRY_VERIFY(stream.diagnostics().value("encodedVideoFrames").toULongLong() >= 1);
    QTRY_VERIFY(stream.diagnostics().value("encodedAudioFrames").toULongLong() >= 1);
    stream.stop();
    QTRY_COMPARE(stream.state(), StreamingState::Idle);
    QVERIFY(stream.start(settings, {32, 32}));
    QTRY_COMPARE(stream.state(), StreamingState::Live);
    stream.stop();
    QTRY_COMPARE(stream.state(), StreamingState::Idle);
}

void StudioProjectTests::streamFailureAndRedactionStayIsolated()
{
    StreamSettings settings;
    settings.serverUrl = QStringLiteral("rtmp://example.invalid/live");
    settings.streamKey = QStringLiteral("super-secret");
    QCOMPARE(redactedStreamUrl(settings), QStringLiteral("rtmp://example.invalid/live/<stream-key>"));

    StreamingOutput stream;
    StreamSettings failing = settings;
    failing.serverUrl = QStringLiteral("staxmock://fail");
    failing.maxReconnectAttempts = 1;
    QVERIFY(stream.start(failing, {32, 32}));
    QTRY_COMPARE(stream.state(), StreamingState::Error);
    QVERIFY(stream.diagnostics().value("reconnectCount").toULongLong() >= 1);
    QVERIFY(!QJsonDocument::fromVariant(stream.diagnostics()).toJson(QJsonDocument::Compact).contains("super-secret"));

    StreamingOutput live;
    StreamSettings ok = settings;
    ok.serverUrl = QStringLiteral("staxmock://success");
    QVERIFY(live.start(ok, {32, 32}));
    QTRY_COMPARE(live.state(), StreamingState::Live);
    live.fail(QStringLiteral("write failed for super-secret"));
    QTRY_COMPARE(live.state(), StreamingState::Error);
    QVERIFY(!live.errorMessage().contains(QStringLiteral("super-secret")));
}

void StudioProjectTests::boundsStreamingQueues()
{
    StreamingOutput stream;
    StreamSettings settings;
    settings.serverUrl = QStringLiteral("staxmock://slow");
    QVERIFY(stream.start(settings, {16, 16}));
    QTRY_COMPARE(stream.state(), StreamingState::Live);
    QImage image(16, 16, QImage::Format_ARGB32);
    image.fill(Qt::blue);
    for (int i = 0; i < 200; ++i)
        stream.submitVideoFrame({image, i * 16'666'666LL});
    for (int i = 0; i < 700; ++i)
        stream.submitProgramAudio({{i * 10'000'000LL, 48000, 2, QVector<float>(960, 0.05f)}});
    QTRY_VERIFY(stream.diagnostics().value("droppedStreamFrames").toULongLong() > 0);
    QVERIFY(stream.diagnostics().value("queuedVideoFrames").toULongLong() <= 3);
    QVERIFY(stream.diagnostics().value("queuedAudioBlocks").toULongLong() <= 256);
    stream.stop();
}

void StudioProjectTests::persistsStreamSettingsWithoutStreamKey()
{
    QTemporaryDir directory;
    StudioRepository repository(directory.path());
    StudioProject project = StudioProject::createDefault();
    project.streamSettings.serverUrl = QStringLiteral("rtmp://localhost/live");
    project.streamSettings.streamKey = QStringLiteral("do-not-save");
    project.streamSettings.videoBitrateKbps = 4500;
    project.streamSettings.audioBitrateKbps = 128;
    project.streamSettings.frameRate = 30;
    QVERIFY(repository.save(project));
    QFile file(repository.filePath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray saved = file.readAll();
    QVERIFY(!saved.contains("do-not-save"));
    StudioProject restored = repository.load();
    QCOMPARE(restored.streamSettings.serverUrl, QStringLiteral("rtmp://localhost/live"));
    QCOMPARE(restored.streamSettings.videoBitrateKbps, 4500);
    QCOMPARE(restored.streamSettings.audioBitrateKbps, 128);
    QCOMPARE(restored.streamSettings.frameRate, 30);
    QVERIFY(restored.streamSettings.streamKey.isEmpty());
}

void StudioProjectTests::providerCapabilitiesReflectOfficialDifferences()
{
    const PlatformCapabilities custom = platformCapabilities(PlatformProviderId::CustomRtmp);
    QVERIFY(custom.supportsStreamKey);
    QVERIFY(!custom.supportsOAuth);
    QVERIFY(!custom.supportsPrivacy);

    const PlatformCapabilities youtube = platformCapabilities(PlatformProviderId::YouTube);
    QVERIFY(youtube.supportsOAuth);
    QVERIFY(youtube.supportsAutomaticIngestion);
    QVERIFY(youtube.supportsTitle);
    QVERIFY(youtube.supportsDescription);
    QVERIFY(youtube.supportsThumbnail);
    QVERIFY(youtube.supportsPrivacy);
    QVERIFY(youtube.supportsScheduling);
    QVERIFY(youtube.supportsLatency);
    QVERIFY(youtube.supportsDvr);
    QVERIFY(youtube.supportsMadeForKids);
    QVERIFY(youtube.supportsAutoStartStop);

    const PlatformCapabilities twitch = platformCapabilities(PlatformProviderId::Twitch);
    QVERIFY(twitch.supportsOAuth);
    QVERIFY(twitch.supportsAutomaticIngestion);
    QVERIFY(twitch.supportsTitle);
    QVERIFY(twitch.supportsCategory);
    QVERIFY(twitch.supportsTags);
    QVERIFY(!twitch.supportsDescription);
    QVERIFY(!twitch.supportsPrivacy);
    QVERIFY(!twitch.supportsThumbnail);

    const PlatformCapabilities kick = platformCapabilities(PlatformProviderId::Kick);
    QVERIFY(kick.supportsOAuth);
    QVERIFY(kick.supportsTitle);
    QVERIFY(kick.supportsCategory);
    QVERIFY(!kick.supportsPrivacy);
    QVERIFY(!kick.supportsTags);
}

void StudioProjectTests::generatesPkceAndRedactsSecrets()
{
    const QString verifier = QStringLiteral("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk");
    QCOMPARE(OAuthPkce::challengeForVerifier(verifier),
             QStringLiteral("E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM"));

    OAuthRequest request;
    request.authorizationEndpoint = QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth");
    request.clientId = QStringLiteral("client-id");
    request.redirectUri = QStringLiteral("http://127.0.0.1:53190/callback");
    request.scope = YouTubeProvider::oauthScopes().join(QLatin1Char(' '));
    request.state = QStringLiteral("state-value");
    request.codeVerifier = verifier;
    request.codeChallenge = OAuthPkce::challengeForVerifier(verifier);
    request.extraParameters.insert(QStringLiteral("access_type"), QStringLiteral("offline"));
    const QUrl url = OAuthPkce::authorizationUrl(request);
    const QUrlQuery query(url);
    QCOMPARE(url.toString(QUrl::RemoveQuery), request.authorizationEndpoint);
    QCOMPARE(query.queryItemValue(QStringLiteral("response_type")), QStringLiteral("code"));
    QCOMPARE(query.queryItemValue(QStringLiteral("client_id")), QStringLiteral("client-id"));
    QCOMPARE(query.queryItemValue(QStringLiteral("redirect_uri")), request.redirectUri);
    QCOMPARE(query.queryItemValue(QStringLiteral("state")), QStringLiteral("state-value"));
    QCOMPARE(query.queryItemValue(QStringLiteral("code_challenge")), request.codeChallenge);
    QCOMPARE(query.queryItemValue(QStringLiteral("code_challenge_method")), QStringLiteral("S256"));
    QCOMPARE(query.queryItemValue(QStringLiteral("access_type")), QStringLiteral("offline"));
    QVERIFY(query.queryItemValue(QStringLiteral("scope")).contains(QStringLiteral("youtube.force-ssl")));

    const QString redacted = OAuthPkce::redact(QStringLiteral("access=token refresh=refresh-key code=auth-code"),
                                               {QStringLiteral("token"), QStringLiteral("refresh-key"), QStringLiteral("auth-code")});
    QVERIFY(!redacted.contains(QStringLiteral("token")));
    QVERIFY(!redacted.contains(QStringLiteral("refresh-key")));
    QVERIFY(!redacted.contains(QStringLiteral("auth-code")));
    QVERIFY(redacted.contains(QStringLiteral("<redacted>")));
}

void StudioProjectTests::resolvesLocalGoogleDesktopClientId()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString preferencesPath = directory.filePath(QStringLiteral("preferences.ini"));
    const QString localId = QStringLiteral("123456789-localdesktop.apps.googleusercontent.com");
    const QString buildId = QStringLiteral("123456789-release.apps.googleusercontent.com");
    const QString overrideId = QStringLiteral("123456789-override.apps.googleusercontent.com");
    QVERIFY(GoogleOAuthConfiguration::validClientId(localId));
    QVERIFY(!GoogleOAuthConfiguration::validClientId(QStringLiteral("client-secret")));
    QSettings preferences(preferencesPath, QSettings::IniFormat);
    preferences.setValue(QStringLiteral("oauth/googleClientId"), localId);
    preferences.sync();
    QCOMPARE(preferences.status(), QSettings::NoError);
    QCOMPARE(GoogleOAuthConfiguration::resolveClientId(preferencesPath, buildId, {}), localId);
    QCOMPARE(GoogleOAuthConfiguration::resolveClientId(preferencesPath, buildId, overrideId), overrideId);
    QCOMPARE(GoogleOAuthConfiguration::resolveClientId(preferencesPath, buildId, QStringLiteral("invalid")), localId);

    InMemoryCredentialStore credentials;
    const QString storedSecret = QStringLiteral("stored-desktop-secret");
    QVERIFY(credentials.writeSecret(GoogleOAuthConfiguration::clientSecretCredentialKey(localId), storedSecret));
    QCOMPARE(GoogleOAuthConfiguration::resolveClientSecret(credentials, localId, {}, {}), storedSecret);
    QCOMPARE(GoogleOAuthConfiguration::resolveClientSecret(credentials, localId,
                                                            QStringLiteral("release-desktop-secret"),
                                                            QStringLiteral("override-desktop-secret")),
             QStringLiteral("override-desktop-secret"));
    QVERIFY(GoogleOAuthConfiguration::resolveClientSecret(credentials, QStringLiteral("wrong-client"), {}, {}).isEmpty());

    OAuthLoopback loopback;
    OAuthRequest request;
    request.authorizationEndpoint = QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth");
    request.clientId = GoogleOAuthConfiguration::resolveClientId(preferencesPath, {}, {});
    request.scope = QStringLiteral("https://www.googleapis.com/auth/youtube");
    const QUrl url = loopback.begin(request);
    QVERIFY(url.isValid());
    const QUrlQuery query(url);
    QCOMPARE(query.queryItemValue(QStringLiteral("client_id")), localId);
    QCOMPARE(query.queryItemValue(QStringLiteral("scope")), request.scope);
    QVERIFY(!query.queryItemValue(QStringLiteral("state")).isEmpty());
    QVERIFY(!query.queryItemValue(QStringLiteral("code_challenge")).isEmpty());
    QCOMPARE(query.queryItemValue(QStringLiteral("code_challenge_method")), QStringLiteral("S256"));
    const QUrl redirect(query.queryItemValue(QStringLiteral("redirect_uri")));
    QCOMPARE(redirect.host(), QStringLiteral("127.0.0.1"));
    QCOMPARE(redirect.path(), QStringLiteral("/oauth/callback"));
    QVERIFY(redirect.port() > 0);
    loopback.cancel();
}

void StudioProjectTests::storesCredentialsBehindAbstraction()
{
    InMemoryCredentialStore store;
    QString error;
    QVERIFY(store.writeSecret(QStringLiteral("youtube/channel-a/refresh"), QStringLiteral("refresh-token"), &error));
    QCOMPARE(store.readSecret(QStringLiteral("youtube/channel-a/refresh"), &error), QStringLiteral("refresh-token"));
    QVERIFY(store.deleteSecret(QStringLiteral("youtube/channel-a/refresh"), &error));
    QVERIFY(store.readSecret(QStringLiteral("youtube/channel-a/refresh"), &error).isEmpty());
}

void StudioProjectTests::persistsDestinationsWithoutSecrets()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    StudioRepository repository(directory.path());
    StudioProject project = StudioProject::createDefault();
    project.streamSettings.serverUrl = QStringLiteral("rtmp://example/live");
    project.streamSettings.streamKey = QStringLiteral("legacy-secret");

    bool foundYouTube = false;
    for (StreamDestination &destination : project.streamDestinations) {
        if (destination.provider != PlatformProviderId::YouTube) continue;
        foundYouTube = true;
        project.activeStreamDestinationId = destination.id;
        destination.accountName = QStringLiteral("Stax Channel");
        destination.connected = true;
        destination.configuration.insert(QStringLiteral("channelId"), QStringLiteral("UC123"));
        destination.configuration.insert(QStringLiteral("accessToken"), QStringLiteral("access-secret"));
        destination.configuration.insert(QStringLiteral("refreshToken"), QStringLiteral("refresh-secret"));
        destination.configuration.insert(QStringLiteral("streamKey"), QStringLiteral("stream-secret"));
        destination.metadata.title = QStringLiteral("Launch Stream");
        destination.metadata.description = QStringLiteral("A development stream");
        destination.metadata.tags = {QStringLiteral("stax"), QStringLiteral("studio")};
    }
    QVERIFY(foundYouTube);
    QVERIFY(repository.save(project));

    QFile file(repository.filePath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray saved = file.readAll();
    QVERIFY(!saved.contains("legacy-secret"));
    QVERIFY(!saved.contains("access-secret"));
    QVERIFY(!saved.contains("refresh-secret"));
    QVERIFY(!saved.contains("stream-secret"));

    const StudioProject restored = repository.load();
    QCOMPARE(restored.streamDestinations.size(), 4);
    QVERIFY(!restored.activeStreamDestinationId.isEmpty());
    bool restoredYouTube = false;
    bool restoredCustom = false;
    for (const StreamDestination &destination : restored.streamDestinations) {
        if (destination.provider == PlatformProviderId::CustomRtmp) {
            restoredCustom = true;
            QCOMPARE(destination.configuration.value(QStringLiteral("serverUrl")).toString(), QStringLiteral("rtmp://example/live"));
            QVERIFY(!destination.configuration.contains(QStringLiteral("streamKey")));
        }
        if (destination.provider != PlatformProviderId::YouTube) continue;
        restoredYouTube = true;
        QCOMPARE(destination.accountName, QStringLiteral("Stax Channel"));
        QCOMPARE(destination.metadata.title, QStringLiteral("Launch Stream"));
        QCOMPARE(destination.metadata.description, QStringLiteral("A development stream"));
        QCOMPARE(destination.metadata.tags, QStringList({QStringLiteral("stax"), QStringLiteral("studio")}));
        QCOMPARE(destination.configuration.value(QStringLiteral("channelId")).toString(), QStringLiteral("UC123"));
        QVERIFY(!destination.configuration.contains(QStringLiteral("accessToken")));
        QVERIFY(!destination.configuration.contains(QStringLiteral("refreshToken")));
        QVERIFY(!destination.configuration.contains(QStringLiteral("streamKey")));
    }
    QVERIFY(restoredCustom);
    QVERIFY(restoredYouTube);
}

void StudioProjectTests::serializesYouTubeBroadcastRequests()
{
    PlatformMetadata metadata;
    metadata.title = QStringLiteral("Stax Launch");
    metadata.description = QStringLiteral("Studio platform integration test");
    metadata.privacy = QStringLiteral("unlisted");
    metadata.categoryId = QStringLiteral("20");
    metadata.latency = QStringLiteral("ultraLow");
    metadata.madeForKids = true;
    metadata.enableDvr = false;
    metadata.autoStart = true;
    metadata.autoStop = false;
    metadata.scheduleForLater = true;
    metadata.scheduledStartTime = QDateTime::fromString(QStringLiteral("2026-10-14T10:00:00Z"), Qt::ISODate);

    const QVariantMap request = YouTubeProvider::createBroadcastRequest(metadata);
    const QVariantMap snippet = request.value(QStringLiteral("snippet")).toMap();
    const QVariantMap status = request.value(QStringLiteral("status")).toMap();
    const QVariantMap content = request.value(QStringLiteral("contentDetails")).toMap();
    QCOMPARE(snippet.value(QStringLiteral("title")).toString(), metadata.title);
    QCOMPARE(snippet.value(QStringLiteral("description")).toString(), metadata.description);
    QCOMPARE(snippet.value(QStringLiteral("categoryId")).toString(), metadata.categoryId);
    QCOMPARE(snippet.value(QStringLiteral("scheduledStartTime")).toString(), QStringLiteral("2026-10-14T10:00:00.000Z"));
    QCOMPARE(status.value(QStringLiteral("privacyStatus")).toString(), QStringLiteral("unlisted"));
    QCOMPARE(status.value(QStringLiteral("selfDeclaredMadeForKids")).toBool(), true);
    QCOMPARE(content.value(QStringLiteral("enableDvr")).toBool(), false);
    QCOMPARE(content.value(QStringLiteral("enableAutoStart")).toBool(), true);
    QCOMPARE(content.value(QStringLiteral("enableAutoStop")).toBool(), false);
    QCOMPARE(content.value(QStringLiteral("latencyPreference")).toString(), QStringLiteral("ultraLow"));

    const QVariantMap stream = YouTubeProvider::createStreamRequest(QStringLiteral("  "));
    QCOMPARE(stream.value(QStringLiteral("snippet")).toMap().value(QStringLiteral("title")).toString(), QStringLiteral("StaxStudio Stream"));
    QCOMPARE(stream.value(QStringLiteral("cdn")).toMap().value(QStringLiteral("ingestionType")).toString(), QStringLiteral("rtmp"));
    const QString bind = YouTubeProvider::bindBroadcastUrl(QStringLiteral("broadcast-1"), QStringLiteral("stream-2"));
    QVERIFY(bind.startsWith(QStringLiteral("https://www.googleapis.com/youtube/v3/liveBroadcasts/bind")));
    QVERIFY(bind.contains(QStringLiteral("broadcast-1")));
    QVERIFY(bind.contains(QStringLiteral("stream-2")));
}

void StudioProjectTests::filtersYouTubeBroadcastListsLocally()
{
    const QMap<QString, QString> query = YouTubeProvider::broadcastListQuery(QStringLiteral("page-2"));
    QCOMPARE(query.value(QStringLiteral("mine")), QStringLiteral("true"));
    QCOMPARE(query.value(QStringLiteral("pageToken")), QStringLiteral("page-2"));
    QCOMPARE(query.value(QStringLiteral("maxResults")), QStringLiteral("50"));
    QVERIFY(!query.contains(QStringLiteral("broadcastStatus")));
    QVERIFY(!query.contains(QStringLiteral("id")));
    int primaryFilters = 0;
    for (const QString &filter : {QStringLiteral("mine"), QStringLiteral("broadcastStatus"), QStringLiteral("id")})
        primaryFilters += query.contains(filter) ? 1 : 0;
    QCOMPARE(primaryFilters, 1);

    const auto broadcast = [](const QString &id, const QString &state, const QString &title) {
        return QJsonObject{{"id", id},
                           {"snippet", QJsonObject{{"title", title}, {"description", QStringLiteral("Description")},
                                                    {"categoryId", QStringLiteral("20")},
                                                    {"scheduledStartTime", QStringLiteral("2026-11-01T12:00:00Z")}}},
                           {"status", QJsonObject{{"lifeCycleStatus", state}, {"privacyStatus", QStringLiteral("private")},
                                                   {"selfDeclaredMadeForKids", false}}},
                           {"contentDetails", QJsonObject{{"boundStreamId", QStringLiteral("stream-1")},
                                                           {"enableDvr", true}, {"latencyPreference", QStringLiteral("low")},
                                                           {"enableAutoStart", true}, {"enableAutoStop", true}}}};
    };
    const QJsonObject response{{"items", QJsonArray{
        broadcast(QStringLiteral("upcoming-1"), QStringLiteral("ready"), QStringLiteral("Upcoming")),
        broadcast(QStringLiteral("live-1"), QStringLiteral("live"), QStringLiteral("Current")),
        broadcast(QStringLiteral("complete-1"), QStringLiteral("complete"), QStringLiteral("Finished")),
        broadcast(QStringLiteral("upcoming-1"), QStringLiteral("ready"), QStringLiteral("Duplicate"))}}};
    const QVariantList eligible = YouTubeProvider::eligibleBroadcasts(response);
    QCOMPARE(eligible.size(), 2);
    QCOMPARE(eligible.at(0).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("upcoming-1"));
    QCOMPARE(eligible.at(0).toMap().value(QStringLiteral("boundStreamId")).toString(), QStringLiteral("stream-1"));
    QCOMPARE(eligible.at(0).toMap().value(QStringLiteral("privacy")).toString(), QStringLiteral("private"));
    QCOMPARE(eligible.at(1).toMap().value(QStringLiteral("status")).toString(), QStringLiteral("live"));
}

void StudioProjectTests::serializesTwitchAndKickMetadata()
{
    PlatformMetadata metadata;
    metadata.title = QStringLiteral("Ranked Night");
    metadata.description = QStringLiteral("Unsupported for Twitch/Kick and must not leak into payloads");
    metadata.privacy = QStringLiteral("private");
    metadata.categoryId = QStringLiteral("509658");
    metadata.language = QStringLiteral("en");
    metadata.tags = {QStringLiteral("fps"), QStringLiteral("community")};

    const QVariantMap twitch = TwitchProvider::updateChannelRequest(metadata);
    QCOMPARE(twitch.value(QStringLiteral("title")).toString(), metadata.title);
    QCOMPARE(twitch.value(QStringLiteral("game_id")).toString(), metadata.categoryId);
    QCOMPARE(twitch.value(QStringLiteral("broadcaster_language")).toString(), QStringLiteral("en"));
    QCOMPARE(twitch.value(QStringLiteral("tags")).toStringList(), metadata.tags);
    QVERIFY(!twitch.contains(QStringLiteral("description")));
    QVERIFY(!twitch.contains(QStringLiteral("privacy")));

    const QVariantMap kick = KickProvider::updateLivestreamRequest(metadata);
    QCOMPARE(kick.value(QStringLiteral("stream_title")).toString(), metadata.title);
    QCOMPARE(kick.value(QStringLiteral("category_id")).toLongLong(), metadata.categoryId.toLongLong());
    QVERIFY(!kick.contains(QStringLiteral("language")));
    QVERIFY(!kick.contains(QStringLiteral("tags")));
    QVERIFY(!kick.contains(QStringLiteral("privacy")));
}

namespace {
class FakePlatformTransport final : public PlatformHttpTransport {
public:
    QVector<PlatformHttpRequest> requests;
    QVector<PlatformHttpResult> replies;
    void send(PlatformHttpRequest request, Callback callback) override
    {
        requests.append(std::move(request));
        if (replies.isEmpty()) { callback({0, {}, QStringLiteral("Unscripted request")}); return; }
        callback(replies.takeFirst());
    }
};

class FailingCredentialStore final : public CredentialStore {
public:
    bool writeSecret(const QString &, const QString &, QString *errorMessage) override
    {
        if (errorMessage) *errorMessage = QStringLiteral("Secure credential persistence failed.");
        return false;
    }
    QString readSecret(const QString &, QString *) const override { return {}; }
    bool deleteSecret(const QString &, QString *) override { return true; }
};
}

void StudioProjectTests::validatesOAuthLoopbackState()
{
    OAuthLoopback loopback;
    QSignalSpy authorized(&loopback, &OAuthLoopback::authorized);
    QSignalSpy failed(&loopback, &OAuthLoopback::failed);
    OAuthRequest request;
    request.authorizationEndpoint = QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth");
    request.clientId = QStringLiteral("test-client");
    request.scope = QStringLiteral("youtube");
    const auto sendCallback = [](const QUrl &auth, const QString &state) {
        const QUrl redirect(QUrlQuery(auth).queryItemValue(QStringLiteral("redirect_uri")));
        QTcpSocket socket;
        socket.connectToHost(QStringLiteral("127.0.0.1"), redirect.port());
        if (!socket.waitForConnected(3000)) return false;
        socket.write("GET /oauth/callback?code=test-code&state=" + state.toUtf8()
                     + " HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
        return socket.waitForBytesWritten(3000);
    };
    const QUrl first = loopback.begin(request);
    QVERIFY(first.isValid());
    QVERIFY(sendCallback(first, QStringLiteral("wrong-state")));
    QTRY_COMPARE(failed.count(), 1);
    QCOMPARE(authorized.count(), 0);

    const QUrl second = loopback.begin(request);
    QVERIFY(second.isValid());
    QVERIFY(sendCallback(second, QUrlQuery(second).queryItemValue(QStringLiteral("state"))));
    QTRY_COMPARE(authorized.count(), 1);
    QCOMPARE(authorized.at(0).at(0).toString(), QStringLiteral("test-code"));
    QVERIFY(!authorized.at(0).at(1).toString().isEmpty());
    QVERIFY(authorized.at(0).at(2).toString().startsWith(QStringLiteral("http://127.0.0.1:")));

    const QUrl kickStyle = loopback.begin(request, nullptr, QStringLiteral("localhost"));
    QVERIFY(kickStyle.isValid());
    QVERIFY(QUrlQuery(kickStyle).queryItemValue(QStringLiteral("redirect_uri")).startsWith(QStringLiteral("http://localhost:")));
    QVERIFY(sendCallback(kickStyle, QUrlQuery(kickStyle).queryItemValue(QStringLiteral("state"))));
    QTRY_COMPARE(authorized.count(), 2);
    QVERIFY(authorized.at(1).at(2).toString().startsWith(QStringLiteral("http://localhost:")));
}

void StudioProjectTests::executesYouTubeLifecycleWithMockTransport()
{
    FakePlatformTransport transport;
    InMemoryCredentialStore credentials;
    YouTubeClient client(&transport, &credentials, QStringLiteral("youtube/test"), QStringLiteral("test-client"));
    transport.replies = {
        {200, R"({"access_token":"access-secret","refresh_token":"refresh-secret","expires_in":3600})", {}},
        {200, R"({"items":[{"id":"UC123","snippet":{"title":"Test Channel"}}]})", {}},
        {200, R"({"id":"broadcast-1"})", {}},
        {200, R"({"id":"stream-1","cdn":{"ingestionInfo":{"rtmpsIngestionAddress":"rtmps://ingest.test/live","streamName":"secret-key"}}})", {}},
        {200, R"({"id":"broadcast-1"})", {}},
        {200, R"({"items":[{"id":"broadcast-1","snippet":{"title":"Test Live"}}]})", {}},
        {200, R"({"items":[]})", {}}
    };
    QString error;
    client.exchangeCode(QStringLiteral("authorization-code"), QStringLiteral("verifier"),
                        QStringLiteral("http://127.0.0.1:10001/oauth/callback"),
                        [&](QString value) { error = value; });
    QVERIFY(error.isEmpty());
    QVERIFY(client.hasCredentials());
    QString channelId;
    QString channelName;
    client.loadOwnChannel([&](QString id, QString name, QString value) {
        channelId = std::move(id);
        channelName = std::move(name);
        error = std::move(value);
    });
    QVERIFY(error.isEmpty());
    QCOMPARE(channelId, QStringLiteral("UC123"));
    QCOMPARE(channelName, QStringLiteral("Test Channel"));
    PlatformMetadata metadata;
    metadata.title = QStringLiteral("Test Live");
    metadata.privacy = QStringLiteral("unlisted");
    client.post(YouTubeClient::apiUrl(QStringLiteral("liveBroadcasts"), {{"part", QStringLiteral("snippet,status,contentDetails")}}),
                QJsonObject::fromVariantMap(YouTubeProvider::createBroadcastRequest(metadata)),
                [&](QJsonObject json, QString value) { error = value; QCOMPARE(json.value("id").toString(), QStringLiteral("broadcast-1")); });
    QVERIFY(error.isEmpty());
    client.post(YouTubeClient::apiUrl(QStringLiteral("liveStreams"), {{"part", QStringLiteral("snippet,cdn,status")}}),
                QJsonObject::fromVariantMap(YouTubeProvider::createStreamRequest(QStringLiteral("Test Stream"))),
                [&](QJsonObject json, QString value) { error = value; QCOMPARE(json.value("id").toString(), QStringLiteral("stream-1")); });
    QVERIFY(error.isEmpty());
    client.postEmpty(QUrl(YouTubeProvider::bindBroadcastUrl(QStringLiteral("broadcast-1"), QStringLiteral("stream-1"))),
                     [&](QJsonObject, QString value) { error = value; });
    QVERIFY(error.isEmpty());
    client.get(YouTubeClient::apiUrl(QStringLiteral("liveBroadcasts"), {{"mine", QStringLiteral("true")}, {"part", QStringLiteral("snippet")}}),
               [&](QJsonObject json, QString value) { error = value; QCOMPARE(json.value("items").toArray().size(), 1); });
    QVERIFY(error.isEmpty());
    QTemporaryDir thumbnailDirectory;
    QVERIFY(thumbnailDirectory.isValid());
    const QString imagePath = thumbnailDirectory.filePath(QStringLiteral("thumbnail.png"));
    QImage image(16, 16, QImage::Format_ARGB32);
    image.fill(Qt::blue);
    QVERIFY(image.save(imagePath));
    client.uploadThumbnail(QStringLiteral("broadcast-1"), imagePath,
                           [&](QJsonObject, QString value) { error = value; });
    QVERIFY(error.isEmpty());
    QCOMPARE(transport.requests.size(), 7);
    QCOMPARE(transport.requests.at(0).url.host(), QStringLiteral("oauth2.googleapis.com"));
    QCOMPARE(transport.requests.at(1).bearerToken, QByteArray("access-secret"));
    QCOMPARE(transport.requests.at(4).body, QByteArray{});
    QVERIFY(transport.requests.at(6).url.path().contains(QStringLiteral("upload/youtube/v3/thumbnails/set")));
    QCOMPARE(transport.requests.at(6).contentType, QByteArray("image/png"));
    QVERIFY(!transport.requests.at(2).body.contains("access-secret"));
    const QString stored = credentials.readSecret(QStringLiteral("youtube/test"));
    QVERIFY(stored.contains(QStringLiteral("refresh-secret")));
    client.disconnect();
    QVERIFY(credentials.readSecret(QStringLiteral("youtube/test")).isEmpty());
}

void StudioProjectTests::sendsGoogleDesktopTokenCredentials()
{
    const QString clientId = QStringLiteral("123456789-desktop.apps.googleusercontent.com");
    const QString clientSecret = QStringLiteral("test-desktop-secret");
    const QString redirectUri = QStringLiteral("http://127.0.0.1:43127/oauth/callback");
    const QString verifier = QStringLiteral("test-pkce-verifier");

    FakePlatformTransport exchangeTransport;
    InMemoryCredentialStore exchangeCredentials;
    exchangeTransport.replies = {{200, R"({"access_token":"access","refresh_token":"refresh","expires_in":3600})", {}}};
    YouTubeClient exchangeClient(&exchangeTransport, &exchangeCredentials,
                                 QStringLiteral("youtube/exchange"), clientId, clientSecret);
    QString error;
    exchangeClient.exchangeCode(QStringLiteral("authorization-code"), verifier, redirectUri,
                                [&](QString value) { error = std::move(value); });
    QVERIFY(error.isEmpty());
    QCOMPARE(exchangeTransport.requests.size(), 1);
    const PlatformHttpRequest &exchange = exchangeTransport.requests.first();
    QCOMPARE(exchange.method, QByteArray("POST"));
    QCOMPARE(exchange.url, QUrl(QStringLiteral("https://oauth2.googleapis.com/token")));
    QCOMPARE(exchange.contentType, QByteArray("application/x-www-form-urlencoded"));
    const QUrlQuery exchangeForm(QString::fromUtf8(exchange.body));
    QCOMPARE(exchangeForm.queryItemValue(QStringLiteral("client_id")), clientId);
    QCOMPARE(exchangeForm.queryItemValue(QStringLiteral("client_secret")), clientSecret);
    QCOMPARE(exchangeForm.queryItemValue(QStringLiteral("code")), QStringLiteral("authorization-code"));
    QCOMPARE(exchangeForm.queryItemValue(QStringLiteral("code_verifier")), verifier);
    QCOMPARE(exchangeForm.queryItemValue(QStringLiteral("redirect_uri")), redirectUri);
    QCOMPARE(exchangeForm.queryItemValue(QStringLiteral("grant_type")), QStringLiteral("authorization_code"));
    QVERIFY(!exchangeCredentials.readSecret(QStringLiteral("youtube/exchange")).contains(clientSecret));

    InMemoryCredentialStore refreshCredentials;
    const QJsonObject expired{{"clientId", clientId}, {"accessToken", QStringLiteral("expired")},
                              {"refreshToken", QStringLiteral("refresh-token")}, {"expiresAtMs", 1}};
    refreshCredentials.writeSecret(QStringLiteral("youtube/refresh"),
        QString::fromUtf8(QJsonDocument(expired).toJson(QJsonDocument::Compact)));
    FakePlatformTransport refreshTransport;
    refreshTransport.replies = {{200, R"({"access_token":"new-access","expires_in":3600})", {}},
                                {200, R"({"items":[]})", {}}};
    YouTubeClient refreshClient(&refreshTransport, &refreshCredentials,
                                QStringLiteral("youtube/refresh"), clientId, clientSecret);
    refreshClient.get(YouTubeClient::apiUrl(QStringLiteral("channels")),
                      [&](QJsonObject, QString value) { error = std::move(value); });
    QVERIFY(error.isEmpty());
    const QUrlQuery refreshForm(QString::fromUtf8(refreshTransport.requests.first().body));
    QCOMPARE(refreshForm.queryItemValue(QStringLiteral("client_id")), clientId);
    QCOMPARE(refreshForm.queryItemValue(QStringLiteral("client_secret")), clientSecret);
    QCOMPARE(refreshForm.queryItemValue(QStringLiteral("refresh_token")), QStringLiteral("refresh-token"));
    QCOMPARE(refreshForm.queryItemValue(QStringLiteral("grant_type")), QStringLiteral("refresh_token"));
    QVERIFY(!refreshCredentials.readSecret(QStringLiteral("youtube/refresh")).contains(clientSecret));

    FakePlatformTransport publicClientTransport;
    InMemoryCredentialStore publicClientCredentials;
    publicClientTransport.replies = {{400, R"({"error":"invalid_request","error_description":"client_secret is missing."})", {}}};
    YouTubeClient publicClient(&publicClientTransport, &publicClientCredentials,
                               QStringLiteral("youtube/public"), clientId);
    publicClient.exchangeCode(QStringLiteral("private-code"), verifier, redirectUri,
                              [&](QString value) { error = std::move(value); });
    QVERIFY(!publicClientTransport.requests.first().body.contains("client_secret"));
    QVERIFY(error.contains(QStringLiteral("invalid_request")));
    QVERIFY(error.contains(QStringLiteral("missing_client_secret")));
    QVERIFY(!error.contains(QStringLiteral("private-code")));
}

void StudioProjectTests::reportsYouTubeOAuthFailuresSafely()
{
    InMemoryCredentialStore credentials;
    QString error;

    FakePlatformTransport tokenTransport;
    tokenTransport.replies = {{400, R"({"error":"invalid_grant","error_description":"authorization-code-must-never-appear"})", {}}};
    YouTubeClient tokenClient(&tokenTransport, &credentials, QStringLiteral("youtube/token-failure"), QStringLiteral("test-client"));
    tokenClient.exchangeCode(QStringLiteral("private-code"), QStringLiteral("private-verifier"),
                             QStringLiteral("http://127.0.0.1:10001/oauth/callback"),
                             [&](QString value) { error = std::move(value); });
    QVERIFY(error.contains(QStringLiteral("token exchange"), Qt::CaseInsensitive));
    QVERIFY(error.contains(QStringLiteral("HTTP 400")));
    QVERIFY(error.contains(QStringLiteral("invalid_grant")));
    QVERIFY(!error.contains(QStringLiteral("authorization-code-must-never-appear")));
    QVERIFY(!error.contains(QStringLiteral("private-code")));
    QVERIFY(!error.contains(QStringLiteral("private-verifier")));

    FakePlatformTransport missingParameterTransport;
    missingParameterTransport.replies = {{400, R"({"error":"invalid_request","error_description":"Missing required parameter: redirect_uri; private details ignored"})", {}}};
    YouTubeClient missingParameterClient(&missingParameterTransport, &credentials,
                                         QStringLiteral("youtube/missing-parameter"), QStringLiteral("test-client"));
    missingParameterClient.exchangeCode(QStringLiteral("private-code"), QStringLiteral("private-verifier"), {},
                                        [&](QString value) { error = std::move(value); });
    QVERIFY(error.contains(QStringLiteral("invalid_request")));
    QVERIFY(error.contains(QStringLiteral("missing_redirect_uri")));
    QVERIFY(!error.contains(QStringLiteral("private details ignored")));

    const QJsonObject stored{{"clientId", QStringLiteral("test-client")}, {"accessToken", QStringLiteral("access")},
                             {"refreshToken", QStringLiteral("refresh")}, {"expiresAtMs", QDateTime::currentMSecsSinceEpoch() + 3600000}};
    credentials.writeSecret(QStringLiteral("youtube/scope-failure"), QString::fromUtf8(QJsonDocument(stored).toJson()));
    FakePlatformTransport scopeTransport;
    scopeTransport.replies = {{403, R"({"error":{"code":403,"message":"sensitive provider text","errors":[{"reason":"insufficientPermissions"}]}})", {}}};
    YouTubeClient scopeClient(&scopeTransport, &credentials, QStringLiteral("youtube/scope-failure"), QStringLiteral("test-client"));
    scopeClient.loadOwnChannel([&](QString, QString, QString value) { error = std::move(value); });
    QVERIFY(error.contains(QStringLiteral("channel lookup"), Qt::CaseInsensitive));
    QVERIFY(error.contains(QStringLiteral("HTTP 403")));
    QVERIFY(error.contains(QStringLiteral("insufficientPermissions")));
    QVERIFY(!error.contains(QStringLiteral("sensitive provider text")));

    credentials.writeSecret(QStringLiteral("youtube/api-disabled"), QString::fromUtf8(QJsonDocument(stored).toJson()));
    FakePlatformTransport apiTransport;
    apiTransport.replies = {{403, R"({"error":{"code":403,"errors":[{"reason":"accessNotConfigured"}]}})", {}}};
    YouTubeClient apiClient(&apiTransport, &credentials, QStringLiteral("youtube/api-disabled"), QStringLiteral("test-client"));
    apiClient.loadOwnChannel([&](QString, QString, QString value) { error = std::move(value); });
    QVERIFY(error.contains(QStringLiteral("accessNotConfigured")));
    QVERIFY(error.contains(QStringLiteral("Enable YouTube Data API v3")));
}

void StudioProjectTests::rejectsMissingYouTubeChannel()
{
    InMemoryCredentialStore credentials;
    FakePlatformTransport transport;
    transport.replies = {{200, R"({"access_token":"access","refresh_token":"refresh","expires_in":3600})", {}},
                         {200, R"({"items":[]})", {}}};
    YouTubeClient client(&transport, &credentials, QStringLiteral("youtube/no-channel"), QStringLiteral("test-client"));
    QString error;
    client.exchangeCode(QStringLiteral("code"), QStringLiteral("verifier"),
                        QStringLiteral("http://127.0.0.1:10001/oauth/callback"),
                        [&](QString value) { error = std::move(value); });
    QVERIFY(error.isEmpty());
    client.loadOwnChannel([&](QString, QString, QString value) { error = std::move(value); });
    QCOMPARE(error, QStringLiteral("No YouTube channel is available for this Google account."));
}

void StudioProjectTests::reportsYouTubeCredentialPersistenceFailure()
{
    FailingCredentialStore credentials;
    FakePlatformTransport transport;
    transport.replies = {{200, R"({"access_token":"access-secret","refresh_token":"refresh-secret","expires_in":3600})", {}}};
    YouTubeClient client(&transport, &credentials, QStringLiteral("youtube/storage-failure"), QStringLiteral("test-client"));
    QString error;
    client.exchangeCode(QStringLiteral("private-code"), QStringLiteral("private-verifier"),
                        QStringLiteral("http://127.0.0.1:10001/oauth/callback"),
                        [&](QString value) { error = std::move(value); });
    QCOMPARE(error, QStringLiteral("Secure credential persistence failed."));
    QVERIFY(!client.hasCredentials());
    QVERIFY(!error.contains(QStringLiteral("access-secret")));
    QVERIFY(!error.contains(QStringLiteral("refresh-secret")));
}

void StudioProjectTests::refreshesYouTubeTokensWithoutPersistingThemInProject()
{
    FakePlatformTransport transport;
    InMemoryCredentialStore credentials;
    const QJsonObject expired{{"clientId", QStringLiteral("test-client")},
                              {"accessToken", QStringLiteral("expired-secret")},
                              {"refreshToken", QStringLiteral("refresh-secret")}, {"expiresAtMs", 1}};
    credentials.writeSecret(QStringLiteral("youtube/test"), QString::fromUtf8(QJsonDocument(expired).toJson()));
    YouTubeClient client(&transport, &credentials, QStringLiteral("youtube/test"), QStringLiteral("test-client"));
    transport.replies = {{200, R"({"access_token":"new-secret","expires_in":3600})", {}},
                         {200, R"({"items":[]})", {}}};
    QString error;
    client.get(YouTubeClient::apiUrl(QStringLiteral("channels")),
               [&](QJsonObject, QString value) { error = value; });
    QVERIFY(error.isEmpty());
    QCOMPARE(transport.requests.size(), 2);
    QCOMPARE(transport.requests.at(0).url.host(), QStringLiteral("oauth2.googleapis.com"));
    QCOMPARE(transport.requests.at(1).bearerToken, QByteArray("new-secret"));
    QVERIFY(credentials.readSecret(QStringLiteral("youtube/test")).contains(QStringLiteral("refresh-secret")));
}

void StudioProjectTests::executesTwitchDeviceAndChannelLifecycle()
{
    FakePlatformTransport transport;
    InMemoryCredentialStore credentials;
    TwitchClient client(&transport, &credentials, QStringLiteral("twitch/test"), QStringLiteral("twitch-client"));
    transport.replies = {
        {200, R"({"device_code":"device-code","user_code":"ABCD","verification_uri":"https://www.twitch.tv/activate","interval":5})", {}},
        {400, R"({"message":"authorization_pending"})", {}},
        {200, R"({"access_token":"twitch-access","refresh_token":"twitch-refresh","expires_in":14400})", {}},
        {200, R"({"client_id":"twitch-client","user_id":"123","login":"tester"})", {}},
        {200, R"({"data":[{"id":"123","display_name":"Test Channel"}]})", {}},
        {204, {}, {}},
        {200, R"({"data":[{"stream_key":"live_secret"}]})", {}}
    };
    QString error;
    client.beginDeviceAuthorization([&](QJsonObject json, QString value) {
        error = value;
        QCOMPARE(json.value(QStringLiteral("user_code")).toString(), QStringLiteral("ABCD"));
    });
    QVERIFY(error.isEmpty());
    client.pollDeviceAuthorization(QStringLiteral("device-code"), [&](QJsonObject json, QString value) {
        error = value;
        QVERIFY(json.value(QStringLiteral("pending")).toBool());
    });
    QVERIFY(error.isEmpty());
    client.pollDeviceAuthorization(QStringLiteral("device-code"), [&](QJsonObject json, QString value) {
        error = value;
        QVERIFY(json.value(QStringLiteral("connected")).toBool());
    });
    QVERIFY(error.isEmpty());
    QVERIFY(client.hasCredentials());
    client.validate([&](QJsonObject json, QString value) {
        error = value;
        QCOMPARE(json.value(QStringLiteral("user_id")).toString(), QStringLiteral("123"));
    });
    QVERIFY(error.isEmpty());
    client.get(TwitchClient::apiUrl(QStringLiteral("users"), {{"id", QStringLiteral("123")}}),
               [&](QJsonObject json, QString value) { error = value; QCOMPARE(json.value(QStringLiteral("data")).toArray().size(), 1); });
    QVERIFY(error.isEmpty());
    PlatformMetadata metadata;
    metadata.title = QStringLiteral("Twitch Test");
    metadata.categoryId = QStringLiteral("509658");
    metadata.language = QStringLiteral("en");
    metadata.tags = {QStringLiteral("Coding")};
    client.updateChannel(QStringLiteral("123"), metadata, [&](QJsonObject, QString value) { error = value; });
    QVERIFY(error.isEmpty());
    client.get(TwitchClient::apiUrl(QStringLiteral("streams/key"), {{"broadcaster_id", QStringLiteral("123")}}),
               [&](QJsonObject json, QString value) { error = value; QCOMPARE(json.value(QStringLiteral("data")).toArray().size(), 1); });
    QVERIFY(error.isEmpty());
    QCOMPARE(transport.requests.size(), 7);
    QVERIFY(transport.requests.at(1).body.contains("scopes="));
    QCOMPARE(transport.requests.at(3).authScheme, QByteArray("OAuth"));
    QCOMPARE(transport.requests.at(5).method, QByteArray("PATCH"));
    QCOMPARE(transport.requests.at(6).bearerToken, QByteArray("twitch-access"));
    client.disconnect();
    QVERIFY(credentials.readSecret(QStringLiteral("twitch/test")).isEmpty());
}

void StudioProjectTests::executesKickOAuthAndChannelLifecycle()
{
    FakePlatformTransport transport;
    InMemoryCredentialStore credentials;
    KickClient client(&transport, &credentials, QStringLiteral("kick/test"),
                      QStringLiteral("kick-client"), QStringLiteral("user-supplied-secret"));
    transport.replies = {
        {200, R"({"access_token":"kick-access","refresh_token":"kick-refresh","expires_in":3600})", {}},
        {200, R"({"data":[{"user_id":123,"name":"Test Kick"}]})", {}},
        {200, R"({"data":[{"broadcaster_user_id":123,"slug":"test-kick"}]})", {}},
        {204, {}, {}}
    };
    QString error;
    client.exchangeCode(QStringLiteral("auth-code"), QStringLiteral("verifier"),
                        QStringLiteral("http://localhost:49494/oauth/callback"),
                        [&](QString value) { error = value; });
    QVERIFY(error.isEmpty());
    QVERIFY(client.hasCredentials());
    client.get(KickClient::apiUrl(QStringLiteral("users")), [&](QJsonObject json, QString value) {
        error = value;
        QCOMPARE(json.value(QStringLiteral("data")).toArray().first().toObject().value(QStringLiteral("user_id")).toInteger(), 123);
    });
    QVERIFY(error.isEmpty());
    client.get(KickClient::apiUrl(QStringLiteral("channels")), [&](QJsonObject json, QString value) {
        error = value;
        QCOMPARE(json.value(QStringLiteral("data")).toArray().size(), 1);
    });
    QVERIFY(error.isEmpty());
    PlatformMetadata metadata;
    metadata.title = QStringLiteral("Kick Test");
    metadata.categoryId = QStringLiteral("123");
    client.updateChannel(metadata, [&](QJsonObject, QString value) { error = value; });
    QVERIFY(error.isEmpty());
    QCOMPARE(transport.requests.size(), 4);
    QVERIFY(transport.requests.at(0).body.contains("client_secret=user-supplied-secret"));
    QCOMPARE(transport.requests.at(3).method, QByteArray("PATCH"));
    QCOMPARE(QJsonDocument::fromJson(transport.requests.at(3).body).object().value(QStringLiteral("category_id")).toInteger(), 123);
    QVERIFY(!credentials.readSecret(QStringLiteral("kick/test")).contains(QStringLiteral("user-supplied-secret")));
    client.disconnect();
    QVERIFY(credentials.readSecret(QStringLiteral("kick/test")).isEmpty());
}

void StudioProjectTests::refreshesProviderTokensWithoutProjectSecrets()
{
    const auto expiredToken = [](const QString &clientId) {
        return QString::fromUtf8(QJsonDocument(QJsonObject{{"clientId", clientId},
            {"accessToken", QStringLiteral("expired")}, {"refreshToken", QStringLiteral("old-refresh")},
            {"expiresAtMs", 1}}).toJson(QJsonDocument::Compact));
    };
    InMemoryCredentialStore credentials;
    credentials.writeSecret(QStringLiteral("twitch/refresh"), expiredToken(QStringLiteral("twitch-client")));
    FakePlatformTransport twitchTransport;
    TwitchClient twitch(&twitchTransport, &credentials, QStringLiteral("twitch/refresh"), QStringLiteral("twitch-client"));
    twitchTransport.replies = {
        {200, R"({"access_token":"new-access","refresh_token":"new-refresh","expires_in":3600})", {}},
        {200, R"({"data":[]})", {}}
    };
    QString error;
    twitch.get(TwitchClient::apiUrl(QStringLiteral("streams")), [&](QJsonObject, QString value) { error = value; });
    QVERIFY(error.isEmpty());
    QCOMPARE(twitchTransport.requests.size(), 2);
    QVERIFY(twitchTransport.requests.first().body.contains("refresh_token=old-refresh"));
    QVERIFY(!twitchTransport.requests.first().body.contains("client_secret"));
    QCOMPARE(twitchTransport.requests.last().bearerToken, QByteArray("new-access"));
    QVERIFY(credentials.readSecret(QStringLiteral("twitch/refresh")).contains(QStringLiteral("new-refresh")));

    credentials.writeSecret(QStringLiteral("kick/refresh"), expiredToken(QStringLiteral("kick-client")));
    FakePlatformTransport kickTransport;
    KickClient kick(&kickTransport, &credentials, QStringLiteral("kick/refresh"),
                    QStringLiteral("kick-client"), QStringLiteral("user-secret"));
    kickTransport.replies = {
        {200, R"({"access_token":"new-kick-access","refresh_token":"new-kick-refresh","expires_in":3600})", {}},
        {200, R"({"data":[]})", {}}
    };
    kick.get(KickClient::apiUrl(QStringLiteral("channels")), [&](QJsonObject, QString value) { error = value; });
    QVERIFY(error.isEmpty());
    QCOMPARE(kickTransport.requests.size(), 2);
    QVERIFY(kickTransport.requests.first().body.contains("client_secret=user-secret"));
    QCOMPARE(kickTransport.requests.last().bearerToken, QByteArray("new-kick-access"));
    QVERIFY(!credentials.readSecret(QStringLiteral("kick/refresh")).contains(QStringLiteral("user-secret")));
    QCOMPARE(KickClient::apiUrl(QStringLiteral("channels")).host(), QStringLiteral("api.kick.com"));
}

void StudioProjectTests::providerFailuresDoNotExposeResponseSecrets()
{
    InMemoryCredentialStore credentials;
    FakePlatformTransport transport;
    YouTubeClient youtube(&transport, &credentials, QStringLiteral("youtube/failure"), QStringLiteral("client"));
    transport.replies = {{403, R"({"error":"quotaExceeded","message":"private-secret"})", {}}};
    QString error;
    youtube.exchangeCode(QStringLiteral("code"), QStringLiteral("verifier"),
                         QStringLiteral("http://127.0.0.1:12345/oauth/callback"),
                         [&](QString value) { error = value; });
    QVERIFY(error.contains(QStringLiteral("Google")));
    QVERIFY(!error.contains(QStringLiteral("private-secret")));

    TwitchClient twitch(&transport, &credentials, QStringLiteral("twitch/failure"), QStringLiteral("client"));
    transport.replies = {{403, R"({"message":"private-secret"})", {}}};
    twitch.beginDeviceAuthorization([&](QJsonObject, QString value) { error = value; });
    QVERIFY(error.contains(QStringLiteral("Twitch")));
    QVERIFY(!error.contains(QStringLiteral("private-secret")));

    KickClient kick(&transport, &credentials, QStringLiteral("kick/failure"),
                    QStringLiteral("client"), QStringLiteral("user-secret"));
    transport.replies = {{403, R"({"message":"private-secret"})", {}}};
    kick.exchangeCode(QStringLiteral("code"), QStringLiteral("verifier"),
                      QStringLiteral("http://localhost:49494/oauth/callback"),
                      [&](QString value) { error = value; });
    QVERIFY(error.contains(QStringLiteral("Kick")));
    QVERIFY(!error.contains(QStringLiteral("private-secret")));
    QVERIFY(!error.contains(QStringLiteral("user-secret")));
}

void StudioProjectTests::recordsSimultaneousProgramAudio_data()
{
    QTest::addColumn<int>("scenario");
    QTest::newRow("microphone-only")<<0;
    QTest::newRow("desktop-only")<<1;
    QTest::newRow("two-simultaneous-tones")<<2;
    QTest::newRow("desktop-start20-stop40")<<3;
}

void StudioProjectTests::recordsSimultaneousProgramAudio()
{
    QFETCH(int,scenario);
    QTemporaryDir directory;
    LocalRecorder recorder;
    RecordingSettings settings; settings.outputDirectory=directory.path(); settings.frameRate=60;
    QVERIFY(recorder.start(settings,{32,32}));
    QTRY_COMPARE(recorder.state(),RecordingState::Recording);
    QImage image(32,32,QImage::Format_ARGB32); image.fill(Qt::blue);
    recorder.submitVideoFrame({image,0});
    recorder.submitVideoFrame({image,recordingFrameTimestampNs(3599,60)});
    ProgramAudioMixer mixer; mixer.reset(0);
    qint64 emitted=0;
    const auto consume=[&](ProgramMixedAudioBlock block) {
        QCOMPARE(block.audio.timestampNs,emitted*10'000'000LL);
        QCOMPARE(block.audio.frameCount(),480);
        recorder.submitProgramAudio(std::move(block)); ++emitted;
        while(recorder.diagnostics().value("queuedAudioBlocks").toInt()>48) QTest::qWait(1);
    };
    for(int tick=0;tick<6000;++tick) {
        const qint64 time=tick*10'000'000LL;
        if(scenario!=1) {
            AudioBlock a{time,48000,1,QVector<float>(480)};
            for(int i=0;i<480;++i) a.samples[i]=0.2f*std::sin(2*M_PI*440*(tick*480+i)/48000.0);
            mixer.push("mic",std::move(a));
        }
        // Stagger arrivals as independent WASAPI workers do. Neither source
        // can force an interval out before the bounded collection deadline.
        mixer.produceUntil(time-ProgramAudioMixer::LatencyNs,consume);
        if(scenario==1 || scenario==2 || (scenario==3 && tick>=2000 && tick<4000)) {
            // Different packetization as well: two 5-ms stereo packets.
            for(int part=0;part<2;++part) {
                AudioBlock b{time+part*5'000'000LL,48000,2,QVector<float>(480)};
                for(int i=0;i<240;++i) {
                    const float v=std::sin(2*M_PI*880*(tick*480+part*240+i)/48000.0);
                    b.samples[i*2]=0.15f*v; b.samples[i*2+1]=0.1f*v;
                }
                mixer.push("desktop",std::move(b));
            }
        }
        mixer.produceUntil(time+5'000'000-ProgramAudioMixer::LatencyNs,consume);
    }
    mixer.produceUntil(60'000'000'000LL,consume);
    recorder.stop();
    QCOMPARE(recorder.state(),RecordingState::Idle);
    QCOMPARE(emitted,6000LL);
    QCOMPARE(mixer.diagnostics().value("sourceBlocksReceived").toULongLong(),
             scenario==0?6000ULL:scenario==1?12000ULL:scenario==2?18000ULL:10000ULL);
    QCOMPARE(mixer.diagnostics().value("lateSourceBlocks").toULongLong(),0ULL);
    QCOMPARE(mixer.diagnostics().value("mixedFramesProduced").toULongLong(),2'880'000ULL);
    QCOMPARE(recorder.diagnostics().value("recorderMixedBlocksReceived").toULongLong(),6000ULL);
    QCOMPARE(recorder.diagnostics().value("droppedAudioBlocks").toULongLong(),0ULL);
    AVFormatContext *input=nullptr;
    QCOMPARE(avformat_open_input(&input,recorder.outputPath().toUtf8().constData(),nullptr,nullptr),0);
    QVERIFY(avformat_find_stream_info(input,nullptr)>=0);
    const int index=av_find_best_stream(input,AVMEDIA_TYPE_AUDIO,-1,-1,nullptr,0);
    QVERIFY(index>=0);
    const double duration=double(input->duration)/AV_TIME_BASE;
    QVERIFY(std::abs(duration-60)<0.15);
    AVCodecContext *decoder=avcodec_alloc_context3(avcodec_find_decoder(input->streams[index]->codecpar->codec_id));
    QVERIFY(avcodec_parameters_to_context(decoder,input->streams[index]->codecpar)>=0);
    QVERIFY(avcodec_open2(decoder,decoder->codec,nullptr)>=0);
    AVPacket *packet=av_packet_alloc(); AVFrame *frame=av_frame_alloc();
    QVector<float> left,right;
    const auto receive=[&] {
        while(avcodec_receive_frame(decoder,frame)==0) {
            QCOMPARE(frame->format,int(AV_SAMPLE_FMT_FLTP)); QCOMPARE(frame->ch_layout.nb_channels,2);
            const auto *l=reinterpret_cast<float *>(frame->extended_data[0]);
            const auto *r=reinterpret_cast<float *>(frame->extended_data[1]);
            for(int i=0;i<frame->nb_samples;++i) {left.append(l[i]);right.append(r[i]);}
        }
    };
    qint64 lastPts=AV_NOPTS_VALUE;
    while(av_read_frame(input,packet)>=0) {
        if(packet->stream_index==index) {
            QVERIFY(lastPts==AV_NOPTS_VALUE || packet->pts>=lastPts); lastPts=packet->pts;
            QVERIFY(avcodec_send_packet(decoder,packet)>=0); receive();
        }
        av_packet_unref(packet);
    }
    avcodec_send_packet(decoder,nullptr); receive();
    QVERIFY(std::abs(double(left.size())/48000-60)<0.15);
    // Measure both tones in EACH half-second window, not just whole-file FFT.
    const auto amplitude=[](const QVector<float> &samples,int begin,int frequency) {
        double re=0,im=0;
        for(int i=0;i<24000;++i) {double phase=2*M_PI*frequency*i/48000.; re+=samples[begin+i]*std::cos(phase); im+=samples[begin+i]*std::sin(phase);}
        return 2*std::hypot(re,im)/24000;
    };
    for(int second=1;second<59;++second) {
        const int begin=second*48000+12000;
        const bool mic=scenario!=1, desktop=scenario==1||scenario==2||(scenario==3&&second>=20&&second<40);
        QVERIFY(std::abs(amplitude(left,begin,440)-(mic?0.2:0))<0.025);
        QVERIFY(std::abs(amplitude(right,begin,440)-(mic?0.2:0))<0.025);
        QVERIFY(std::abs(amplitude(left,begin,880)-(desktop?0.15:0))<0.025);
        QVERIFY(std::abs(amplitude(right,begin,880)-(desktop?0.1:0))<0.025);
    }
    // Search ALL 240 possible 5-ms boundary phases after AAC, not only phase 0.
    double energies[240]{}; int counts[240]{}; double maxStep=0;
    for(int i=48000;i<59*48000;++i) {
        if(scenario==3 && (std::abs(i-20*48000)<4800 || std::abs(i-40*48000)<4800)) continue;
        const double delta=left[i]-left[i-1]; maxStep=std::max(maxStep,std::abs(delta));
        energies[i%240]+=delta*delta; ++counts[i%240];
    }
    double mean=0,maximum=0;
    for(int i=0;i<240;++i) {energies[i]/=counts[i];mean+=energies[i]/240;maximum=std::max(maximum,energies[i]);}
    QVERIFY(maxStep<0.09);
    QVERIFY(std::sqrt(maximum/mean)<1.9);
    qInfo()<<"Program mix validation"<<scenario<<"duration"<<duration<<"blocks"<<emitted<<"max step"<<maxStep<<"boundary RMS ratio"<<std::sqrt(maximum/mean);
    av_frame_free(&frame);av_packet_free(&packet);avcodec_free_context(&decoder);avformat_close_input(&input);
}

void StudioProjectTests::optionalWasapiEndpointProbe()
{
    if (!qEnvironmentVariableIsSet("STAX_WASAPI_PROBE_ENDPOINT")) QSKIP("Opt-in hardware diagnostic only");
    const QString requested = qEnvironmentVariable("STAX_WASAPI_PROBE_ENDPOINT");
    WasapiAudioSource source(requested, false);
    source.setMixControls(1.0, false);
    source.start();
    for (int second = 0; second < 5; ++second) {
        QTest::qWait(1100);
        source.discardPendingBlocks();
        qInfo().noquote() << QJsonDocument::fromVariant(source.diagnostics()).toJson(QJsonDocument::Compact);
    }
    source.stop();
    const auto diagnostic = source.diagnostics();
    QCOMPARE(diagnostic.value("requestedEndpointId").toString(), requested);
    if (!requested.isEmpty()) QCOMPARE(diagnostic.value("endpointId").toString(), requested);
    QVERIFY(diagnostic.value("capturedBlocks").toULongLong() > 0);
    QVERIFY(diagnostic.value("WindowsEndpointPeakDbfs").isValid());
    QVERIFY(diagnostic.value("CapturedSamplePeakDbfs").isValid());
}

void StudioProjectTests::createsAndSelectsScenes()
{
    StudioProject project = StudioProject::createDefault();
    const Scene &scene = project.addScene(QStringLiteral("Gameplay"));
    QCOMPARE(scene.name, QStringLiteral("Gameplay (2)"));
    project.activeSceneId = scene.id;
    QCOMPARE(project.activeScene()->name, QStringLiteral("Gameplay (2)"));
}

void StudioProjectTests::protectsValidSceneState()
{
    StudioProject project = StudioProject::createDefault();
    const QString activeId = project.activeSceneId;
    QVERIFY(project.removeScene(activeId));
    QVERIFY(project.activeScene() != nullptr);
    while (project.scenes.size() > 1) QVERIFY(project.removeScene(project.scenes.last().id));
    QVERIFY(!project.removeScene(project.scenes.first().id));
    QCOMPARE(project.scenes.size(), 1);
}

void StudioProjectTests::reordersScenesAndItems()
{
    StudioProject project = StudioProject::createDefault();
    const QString first = project.scenes.first().id;
    QVERIFY(project.moveScene(first, 1));
    QCOMPARE(project.scenes.at(1).id, first);
    project.activeSceneId = project.scenes.first().id;
    Source &one = project.addSource(SourceType::Image);
    Source &two = project.addSource(SourceType::Text);
    Q_UNUSED(one)
    Q_UNUSED(two)
    Scene *scene = project.activeScene();
    QCOMPARE(scene->items.size(), 2);
    const QString item = scene->items.first().id;
    QVERIFY(project.moveSceneItem(item, 1));
    QCOMPARE(scene->items.at(1).id, item);
    QCOMPARE(scene->items.at(1).zOrder, 1);
}

void StudioProjectTests::managesSourceLayerState()
{
    StudioProject project = StudioProject::createDefault();
    Source &source = project.addSource(SourceType::DisplayCapture);
    SceneItem &item = project.activeScene()->items.first();
    QVERIFY(project.renameSource(source.id, QStringLiteral("Primary Display")));
    QVERIFY(project.setSceneItemVisible(item.id, false));
    QVERIFY(project.setSceneItemLocked(item.id, true));
    QCOMPARE(project.source(source.id)->name, QStringLiteral("Primary Display"));
    QVERIFY(!item.visible);
    QVERIFY(item.locked);
    QVERIFY(project.removeSceneItem(item.id));
    QVERIFY(project.activeScene()->items.isEmpty());
    QVERIFY(project.sources.isEmpty());
}

void StudioProjectTests::appliesAndPersistsTransformOperations()
{
    StudioProject project = StudioProject::createDefault();
    project.addSource(SourceType::Image);
    SceneItem &item = project.activeScene()->items.first();
    const QString itemId = item.id;

    Transform transform = item.transform;
    transform.x = 200.0;
    transform.y = 100.0;
    transform.width = 640.0;
    transform.height = 360.0;
    transform.scaleX = 1.5;
    transform.scaleY = 1.5;
    transform.cropLeft = 10.0;
    transform.cropRight = 10.0;
    QVERIFY(project.setSceneItemTransform(itemId, transform));
    QVERIFY(project.centerSceneItem(itemId, true, false));
    QCOMPARE(item.transform.x, 495.0);
    QCOMPARE(item.transform.y, 100.0);
    QVERIFY(project.rotateSceneItem(itemId, 90.0));
    QCOMPARE(item.transform.rotation, 90.0);
    QVERIFY(project.flipSceneItem(itemId, true));
    QVERIFY(item.transform.flipHorizontal);
    QVERIFY(project.fitSceneItemToCanvas(itemId));
    QCOMPARE(item.transform.x, 30.0);
    QCOMPARE(item.transform.y, 0.0);
    QCOMPARE(item.transform.width, 1860.0);
    QCOMPARE(item.transform.height, 1080.0);
    QVERIFY(project.stretchSceneItemToCanvas(itemId));
    QVERIFY(project.resetSceneItemTransform(itemId));
    QCOMPARE(item.transform.rotation, 0.0);
    QVERIFY(!item.transform.flipHorizontal);
}

void StudioProjectTests::composesVisibleLayersAtProgramResolution()
{
    StudioProject project = StudioProject::createDefault();
    project.programResolution = ProgramResolution::qhd1440();
    project.addSource(SourceType::Color);
    project.addSource(SourceType::Image);
    project.addSource(SourceType::Microphone);
    Scene *scene = project.activeScene();
    QCOMPARE(scene->items.size(), 3);
    scene->items[0].transform.width = 2560.0;
    scene->items[0].transform.height = 1440.0;
    scene->items[1].transform.x = 250.0;
    scene->items[1].transform.y = 120.0;
    scene->items[1].transform.rotation = 25.0;
    scene->items[1].transform.flipHorizontal = true;

    QVector<CompositorLayer> layers = CompositorScene::activeLayers(project);
    QCOMPARE(layers.size(), 2);
    QCOMPARE(layers[0].zOrder, 0);
    QCOMPARE(layers[1].zOrder, 1);
    QCOMPARE(layers[0].frame.pixelSize, QSize(2560, 1440));
    QCOMPARE(layers[1].transform.rotation, 25.0);
    QVERIFY(layers[1].transform.flipHorizontal);

    QVERIFY(project.setSceneItemVisible(scene->items[0].id, false));
    layers = CompositorScene::activeLayers(project);
    QCOMPARE(layers.size(), 1);
    QCOMPARE(layers[0].sceneItemId, scene->items[1].id);
}

void StudioProjectTests::persistsAndRestoresProject()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    StudioRepository repository(directory.path());
    StudioProject project = StudioProject::createDefault();
    project.profileName = QStringLiteral("Creator Profile");
    project.programResolution = ProgramResolution::qhd1440();
    project.transition = {TransitionType::Fade, 850};
    const QString imageId = project.addSource(SourceType::Image).id;
    project.addSource(SourceType::Microphone);
    project.addSource(SourceType::DesktopAudio);
    SceneItem &item = project.activeScene()->items.first();
    item.transform.x = 120.0;
    item.transform.width = 640.0;
    item.transform.flipHorizontal = true;
    project.setSceneItemVisible(item.id, false);
    project.setSceneItemLocked(item.id, true);
    project.setMixerVolume(project.mixerChannels.first().id, 0.42);
    project.setMixerMuted(project.mixerChannels.last().id, true);
    QVERIFY(repository.save(project));

    const StudioProject restored = repository.load();
    QCOMPARE(restored.profileName, QStringLiteral("Creator Profile"));
    QCOMPARE(restored.programResolution, ProgramResolution::qhd1440());
    QCOMPARE(restored.transition.durationMs, 850);
    QCOMPARE(restored.sources.size(), 3);
    QCOMPARE(restored.sources.first().id, imageId);
    const SceneItem &restoredItem = restored.activeScene()->items.first();
    QCOMPARE(restoredItem.transform.x, 120.0);
    QCOMPARE(restoredItem.transform.width, 640.0);
    QVERIFY(restoredItem.transform.flipHorizontal);
    QVERIFY(!restoredItem.visible);
    QVERIFY(restoredItem.locked);
    QCOMPARE(restored.mixerChannels.first().volume, 0.42);
    QVERIFY(restored.mixerChannels.last().muted);
}

void StudioProjectTests::recoversFromInvalidConfiguration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    StudioRepository repository(directory.path());
    QFile file(repository.filePath());
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("not json"), 8LL);
    file.close();
    const StudioProject recovered = repository.load();
    QVERIFY(!recovered.scenes.isEmpty());
    QVERIFY(recovered.activeScene() != nullptr);
    QCOMPARE(recovered.mixerChannels.size(), 0);
}

void StudioProjectTests::recoversFromUnsupportedSchema()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    StudioRepository repository(directory.path());
    QFile file(repository.filePath());
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{\"schemaVersion\": 999}");
    file.close();
    const StudioProject recovered = repository.load();
    QCOMPARE(recovered.profileName, QStringLiteral("My Studio"));
    QVERIFY(recovered.activeScene() != nullptr);
}

void StudioProjectTests::notifiesSceneModelMutations()
{
    StudioProject project = StudioProject::createDefault();
    SceneListModel model(&project);
    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    QSignalSpy moved(&model, &QAbstractItemModel::rowsMoved);
    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);

    const QString id = model.addScene(QStringLiteral("Test Scene"));
    QCOMPARE(model.rowCount(), 4);
    QCOMPARE(inserted.count(), 1);
    QVERIFY(model.renameScene(id, QStringLiteral("Gameplay")));
    QCOMPARE(changed.count(), 1);
    QVERIFY(model.moveScene(id, -1));
    QCOMPARE(moved.count(), 1);
    QVERIFY(model.removeScene(id));
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(removed.count(), 1);
}

void StudioProjectTests::notifiesSourceModelMutations()
{
    StudioProject project = StudioProject::createDefault();
    QString selectedItemId;
    SceneItemListModel model(&project, &selectedItemId);
    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    QSignalSpy moved(&model, &QAbstractItemModel::rowsMoved);
    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);

    const QString firstItemId = model.addSource(SourceType::WindowCapture, QStringLiteral("Window"));
    const QString secondItemId = model.addSource(SourceType::Image, QStringLiteral("Artwork"));
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(inserted.count(), 2);
    const QString sourceId = project.activeScene()->items.first().sourceId;
    QVERIFY(model.renameSource(sourceId, QStringLiteral("Primary Window")));
    QVERIFY(model.setItemVisible(firstItemId, false));
    QVERIFY(model.setItemLocked(firstItemId, true));
    QCOMPARE(changed.count(), 3);
    QVERIFY(model.moveItem(secondItemId, -1));
    QCOMPARE(moved.count(), 1);
    QVERIFY(model.removeItem(firstItemId));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(removed.count(), 1);
}

void StudioProjectTests::keepsModelsConsistentThroughRepeatedLifecycleChanges()
{
    StudioProject project = StudioProject::createDefault();
    QString selectedItemId;
    SceneListModel scenes(&project);
    SceneItemListModel items(&project, &selectedItemId);

    for (int cycle = 0; cycle < 25; ++cycle) {
        const QString sceneId = scenes.addScene(QStringLiteral("Cycle %1").arg(cycle));
        items.beginSceneChange();
        project.activeSceneId = sceneId;
        scenes.notifyActiveSceneChanged();
        items.endSceneChange();

        const QString windowItemId = items.addSource(SourceType::WindowCapture, QStringLiteral("Window %1").arg(cycle));
        const QString imageItemId = items.addSource(SourceType::Image, QStringLiteral("Image %1").arg(cycle));
        QVERIFY(items.setItemVisible(windowItemId, false));
        QVERIFY(items.setItemLocked(windowItemId, true));
        QVERIFY(items.moveItem(imageItemId, -1));
        QVERIFY(items.removeItem(windowItemId));
        QCOMPARE(items.rowCount(), 1);

        items.beginSceneChange();
        project.activeSceneId = project.scenes.first().id;
        scenes.notifyActiveSceneChanged();
        items.endSceneChange();
        QVERIFY(scenes.removeScene(sceneId));
        QCOMPARE(project.sceneIndex(sceneId), -1);
        QVERIFY(project.activeScene() != nullptr);
        QCOMPARE(items.rowCount(), project.activeScene()->items.size());
    }
}

void StudioProjectTests::normalizesAudioBlocks()
{
    AudioBlock fortyEight{100, 48000, 1, {0.0f, 0.5f, -0.5f}};
    const AudioBlock passthrough = AudioProcessing::normalizeToInternalFormat(fortyEight);
    QCOMPARE(passthrough.sampleRate, 48000);
    QCOMPARE(passthrough.channelCount, 1);
    QCOMPARE(passthrough.timestampNs, 100);
    QCOMPARE(passthrough.samples, fortyEight.samples);

    AudioBlock fortyFour{200, 44100, 2, QVector<float>(441 * 2, 0.5f)};
    AudioBlock upsampled = AudioProcessing::normalizeToInternalFormat(std::move(fortyFour));
    QCOMPARE(upsampled.sampleRate, 48000);
    QCOMPARE(upsampled.channelCount, 2);
    QCOMPARE(upsampled.timestampNs, 200);
    QCOMPARE(upsampled.samples.size(), 480 * 2);
    QVERIFY(qAbs(upsampled.samples.first() - 0.5f) < 0.001f);

    AudioBlock downsampled = AudioProcessing::normalizeToInternalFormat({300, 96000, 1, QVector<float>(960, 1.0f)});
    QCOMPARE(downsampled.sampleRate, 48000);
    QCOMPARE(downsampled.channelCount, 1);
    QCOMPARE(downsampled.samples.size(), 480);
    AudioProcessing::applyGainAndMute(downsampled, 0.5f, false);
    QVERIFY(qAbs(downsampled.samples.first() - 0.5f) < 0.001f);
    QVERIFY(AudioProcessing::peakDb(downsampled) < -5.9f && AudioProcessing::peakDb(downsampled) > -6.2f);
    AudioProcessing::applyGainAndMute(downsampled, 1.0f, true);
    QCOMPARE(AudioProcessing::peakDb(downsampled), -90.0f);

    const AudioBlock mono{400, 48000, 1, {0.25f, -0.25f}};
    const AudioBlock stereo = AudioProcessing::mixToStereo({mono});
    QCOMPARE(stereo.channelCount, 2);
    QCOMPARE(stereo.samples, QVector<float>({0.25f, 0.25f, -0.25f, -0.25f}));
    AudioBlock unity{500, 48000, 1, {0.3f}};
    AudioProcessing::applyGainAndMute(unity, 1.0f, false);
    QCOMPARE(unity.samples.first(), 0.3f);
}

void StudioProjectTests::convertsNativePcmAtUnity()
{
    for(const int rate:{44100,48000,96000}) for(const int channels:{1,2}) {
        AudioResampleState state;
        qint64 count=0,last=-1;
        float peak=0;
        const int frames=rate/100;
        for(int packet=0;packet<100;++packet) {
            QVector<qint16> pcm(frames*channels);
            for(int frame=0;frame<frames;++frame) {
                const auto value=qint16(std::sin(2*M_PI*440*(packet*frames+frame)/rate)*32768*std::pow(10,-6.0/20));
                for(int channel=0;channel<channels;++channel) pcm[frame*channels+channel]=value;
            }
            auto block=AudioProcessing::convertNativePcm(pcm.constData(),frames,rate,channels,16,false,channels==1 ? 4 : 3,
                packet*10'000'000LL,state);
            QCOMPARE(block.sampleRate,48000); QCOMPARE(block.channelCount,channels);
            QVERIFY(block.timestampNs>last);last=block.timestampNs;
            count+=block.samples.size()/channels;
            const auto before=block.samples;
            AudioProcessing::applyGainAndMute(block,1,false);QCOMPARE(block.samples,before);
            const auto mixed=AudioProcessing::mixToStereo({block});
            peak=qMax(peak,AudioProcessing::peakDb(mixed)+90);
        }
        auto tail=AudioProcessing::convertNativePcm(nullptr,0,rate,channels,16,false,channels==1 ? 4 : 3,0,state);
        count+=tail.samples.size()/channels;
        QCOMPARE(count,48000);
        QVERIFY(qAbs((peak-90)+6)<0.1);
        qInfo()<<"Unity PCM -> swresample -> source gain -> stereo/pre-AAC:"<<rate<<channels<<peak-90<<"dBFS";
    }
    for(int bits:{24,32}) {
        QByteArray pcm(480*(bits/8),0);
        for(int i=0;i<480;++i) pcm[i*(bits/8)+bits/8-1]=64;
        AudioResampleState state;
        auto block=AudioProcessing::convertNativePcm(pcm.constData(),480,48000,1,bits,false,4,0,state);
        QVERIFY(qAbs(block.samples.first()-0.5f)<0.0001f);
        AudioProcessing::applyGainAndMute(block,2,false);QVERIFY(qAbs(block.samples.first()-1)<0.0001f);
        AudioProcessing::applyGainAndMute(block,10,true);QCOMPARE(AudioProcessing::peakDb(block),-90.0f);
    }
}

void StudioProjectTests::preservesLongRunAudioResampleCounts()
{
    AudioResampleState state;
    int frames = 0;
    qint64 previousTimestamp = -1;
    for (int packet = 0; packet < 100; ++packet) {
        AudioBlock input{packet * 23'219'954LL, 44100, 2, QVector<float>(1024 * 2, 0.25f)};
        AudioBlock output = AudioProcessing::normalizeToInternalFormat(std::move(input), &state);
        QCOMPARE(output.sampleRate, 48000);
        QCOMPARE(output.channelCount, 2);
        QVERIFY(output.timestampNs > previousTimestamp);
        previousTimestamp = output.timestampNs;
        frames += output.samples.size() / output.channelCount;
    }
    const AudioBlock tail=AudioProcessing::normalizeToInternalFormat({0,44100,2,{}},&state);
    frames+=tail.samples.size()/2;
    QVERIFY(qAbs(frames-static_cast<int>((100LL * 1024 * 48000) / 44100))<=1);
}

void StudioProjectTests::dockLayoutSupportsNestedMovesAndPersistence()
{
    QTemporaryDir directory;
    DockLayout layout(directory.path());
    const auto initial=layout.arrange(1600,900);
    QCOMPARE(initial.value("panels").toMap().size(),6);
    QVERIFY(layout.movePanel("scenes","preview","left"));
    QVERIFY(layout.movePanel("sources","scenes","bottom"));
    QVERIFY(layout.movePanel("mixer","preview","right"));
    auto arranged=layout.arrange(1600,900);
    const auto panels=arranged.value("panels").toMap();
    QVERIFY(panels.value("sources").toMap().value("y").toDouble()>panels.value("scenes").toMap().value("y").toDouble());
    QVERIFY(panels.value("mixer").toMap().value("x").toDouble()>panels.value("preview").toMap().value("x").toDouble());
    for(const auto &handle:arranged.value("handles").toList())
        QVERIFY(layout.resizeSplit(handle.toMap().value("id").toString(),0.65));
    layout.save();
    DockLayout restored(directory.path());
    QCOMPARE(restored.state(),layout.state());
    layout.setLocked(true);
    QVERIFY(!layout.movePanel("controls","preview","top"));
    QVERIFY(!layout.resizeSplit(arranged.value("handles").toList().first().toMap().value("id").toString(),0.3));
    const auto small=layout.arrange(10,10).value("panels").toMap();
    for(const auto &rect:small) { QVERIFY(rect.toMap().value("width").toDouble()>=170); QVERIFY(rect.toMap().value("height").toDouble()>=150); }
    QVERIFY(!layout.restore({{"version",1},{"tree",QVariantMap{{"panel","scenes"}}}}));
    layout.reset();
    const auto defaults=layout.arrange(1600,900).value("panels").toMap();
    QCOMPARE(defaults.value("scenes").toMap().value("y"),defaults.value("controls").toMap().value("y"));
    QVERIFY(!layout.locked());
}

void StudioProjectTests::usesExactRecordingFrameClock()
{
    QCOMPARE(recordingFrameIndexAt(60'000'000'000LL, 60), 3600LL);
    QCOMPARE(recordingFrameTimestampNs(3600, 60), 60'000'000'000LL);
    QCOMPARE(recordingFrameIndexAt(999'999'999LL, 60), 59LL);
    QCOMPARE(recordingFrameTimestampNs(1, 60), 16'666'666LL);
}

void StudioProjectTests::keepsSixtySecondOutputClockIndependentOfPreview()
{
    for (const int fps : {30, 60}) {
        int videoFrames = 0;
        qint64 lastVideoNs = -1;
        int previewFrames = 0;
        for (int frame = 0; frame < 60 * fps; ++frame) {
            // Rounded-up tick timestamps represent the first instant in each
            // output slot; preview presentation does not enter this clock.
            const qint64 elapsedNs = (static_cast<qint64>(frame) * 1'000'000'000LL + fps - 1) / fps;
            QCOMPARE(recordingFrameIndexAt(elapsedNs, fps), static_cast<qint64>(frame));
            const qint64 timestampNs = recordingFrameTimestampNs(frame, fps);
            QVERIFY(timestampNs > lastVideoNs);
            lastVideoNs = timestampNs;
            ++videoFrames;
            if (frame < 20 * fps || frame >= 40 * fps) ++previewFrames;
        }
        QCOMPARE(videoFrames, 60 * fps);
        QCOMPARE(previewFrames, 40 * fps);
        QVERIFY(lastVideoNs >= 60'000'000'000LL - 1'000'000'000LL / fps - 1);
    }
    qint64 samplePosition = 0, lastAudioNs = -1;
    while (samplePosition < 60LL * 48000) {
        samplePosition = qMin(60LL * 48000, samplePosition + 1024);
        const qint64 blockEndNs = samplePosition * 1'000'000'000LL / 48000;
        QVERIFY(blockEndNs > lastAudioNs);
        lastAudioNs = blockEndNs;
    }
    QCOMPARE(samplePosition, 2'880'000LL);
    QCOMPARE(lastAudioNs, 60'000'000'000LL);
}

void StudioProjectTests::preservesUnityGainThroughProgramMix()
{
    const float inputPeak = std::pow(10.0f, -6.0f / 20.0f);
    AudioBlock source{1'000'000'000LL, 44100, 1, QVector<float>(441, inputPeak)};
    source = AudioProcessing::normalizeToInternalFormat(std::move(source));
    AudioProcessing::applyGainAndMute(source, 1.0f, false);
    const AudioBlock output = AudioProcessing::mixToStereo({source});
    QCOMPARE(output.sampleRate, 48000);
    QCOMPARE(output.channelCount, 2);
    QCOMPARE(output.samples.size(), 960);
    QVERIFY(qAbs(output.samples.first() - inputPeak) < 0.001f);
    QVERIFY(qAbs(output.samples.at(1) - inputPeak) < 0.001f);
    QVERIFY(qAbs(AudioProcessing::peakDb(output) + 6.0f) < 0.05f);
}

void StudioProjectTests::keepsNativePixelGeometrySeparateFromUiScaling()
{
    // A 125% Windows display must report/use DXGI's native 1920x1080 output,
    // never Qt's 1536x864 device-independent layout size.
    const CaptureTarget display{QStringLiteral("\\\\.\\DISPLAY1"), QStringLiteral("DISPLAY1"), QSize(1920, 1080), true, QStringLiteral("1920 x 1080 native pixels")};
    const QVariantMap uiTarget = display.toVariantMap();
    QCOMPARE(uiTarget.value("width").toInt(), 1920);
    QCOMPARE(uiTarget.value("height").toInt(), 1080);
    QVERIFY(uiTarget.value("width").toInt() != 1536);
    QVERIFY(uiTarget.value("height").toInt() != 864);
}

void StudioProjectTests::fitsCaptureSourcesToTheProgramCanvas()
{
    const QSize canvas(1920, 1080);
    const Transform matching = ProgramFrameMath::fitToCanvas(QSize(1920, 1080), canvas);
    QCOMPARE(matching.x, 0.0);
    QCOMPARE(matching.y, 0.0);
    QCOMPARE(matching.width, 1920.0);
    QCOMPARE(matching.height, 1080.0);

    const Transform sameAspect = ProgramFrameMath::fitToCanvas(QSize(2560, 1440), canvas);
    QCOMPARE(sameAspect.x, 0.0);
    QCOMPARE(sameAspect.y, 0.0);
    QCOMPARE(sameAspect.width, 1920.0);
    QCOMPARE(sameAspect.height, 1080.0);

    const Transform differentAspect = ProgramFrameMath::fitToCanvas(QSize(1440, 1080), canvas);
    QCOMPARE(differentAspect.x, 240.0);
    QCOMPARE(differentAspect.y, 0.0);
    QCOMPARE(differentAspect.width, 1440.0);
    QCOMPARE(differentAspect.height, 1080.0);
}

void StudioProjectTests::mapsImportedDxgiTextureRowsIntoProgramOrientation()
{
    // A directional 2x2 source: red/green on top, blue/yellow on bottom.
    const QImage source = [] {
        QImage image(2, 2, QImage::Format_RGBA8888);
        image.setPixelColor(0, 0, Qt::red); image.setPixelColor(1, 0, Qt::green);
        image.setPixelColor(0, 1, Qt::blue); image.setPixelColor(1, 1, Qt::yellow);
        return image;
    }();
    const auto sampledAt = [&source](const bool bottomVertex, const bool flipped) {
        const int row = qRound(ProgramFrameMath::textureV(bottomVertex, true, flipped));
        return source.pixelColor(0, row);
    };
    QCOMPARE(sampledAt(false, false), QColor(Qt::red));
    QCOMPARE(sampledAt(true, false), QColor(Qt::blue));
    QCOMPARE(sampledAt(false, true), QColor(Qt::blue));
    QCOMPARE(sampledAt(true, true), QColor(Qt::red));
}

void StudioProjectTests::bestFitsPreviewWithoutChangingProgramSpace()
{
    const QRectF wide = ProgramFrameMath::bestFitPreviewRect(QSize(1920, 1080), QSizeF(1000, 700));
    QCOMPARE(wide.x(), 0.0);
    QCOMPARE(wide.y(), 68.75);
    QCOMPARE(wide.width(), 1000.0);
    QCOMPARE(wide.height(), 562.5);
    const QRectF square = ProgramFrameMath::bestFitPreviewRect(QSize(1920, 1080), QSizeF(700, 700));
    QCOMPARE(square.x(), 0.0);
    QCOMPARE(square.y(), 153.125);
    QCOMPARE(square.width(), 700.0);
    QCOMPARE(square.height(), 393.75);
}

void StudioProjectTests::writesPlayableMkv()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    LocalRecorder recorder;
    RecordingSettings settings;
    settings.outputDirectory = directory.path();
    settings.frameRate = 30;
    QVERIFY(recorder.start(settings, QSize(64, 64)));
    QTRY_COMPARE(recorder.stateName(), QStringLiteral("Recording"));
    QImage programFrame(64, 64, QImage::Format_ARGB32);
    for (int y = 0; y < 64; ++y) for (int x = 0; x < 64; ++x)
        programFrame.setPixelColor(x, y, x < 32 ? (y < 32 ? Qt::red : Qt::blue) : (y < 32 ? Qt::green : Qt::yellow));
    recorder.submitVideoFrame({programFrame, 0});
    QTest::qWait(300);
    QVERIFY(recorder.elapsedMs() > 100);
    constexpr int recordingSeconds = 30;
    recorder.submitVideoFrame({programFrame, recordingSeconds * 1'000'000'000LL});
    const int blockCount = (recordingSeconds * 48000 + 1023) / 1024;
    for (int blockIndex = 0; blockIndex < blockCount; ++blockIndex) {
        AudioBlock audio{blockIndex * 21'333'333LL, 48000, 1, QVector<float>(1024)};
        for (int sample = 0; sample < 1024; ++sample)
            audio.samples[sample] = 0.3f * qSin(2.0 * M_PI * 440.0 * (blockIndex * 1024 + sample) / 48000.0);
        recorder.submitProgramAudio({AudioProcessing::mixToStereo({audio})});
        // Let the bounded asynchronous recorder consume a realistic burst
        // before producing more audio. A lost tail must fail this test.
        if ((blockIndex + 1) % 64 == 0) QTest::qWait(20);
    }
    recorder.stop();
    QCOMPARE(recorder.stateName(), QStringLiteral("Idle"));
    QCOMPARE(recorder.elapsedMs(), 0LL);
    QVERIFY(QFileInfo::exists(recorder.outputPath()));
    QVERIFY(!QFileInfo::exists(recorder.outputPath() + QStringLiteral(".diagnostics.jsonl")));
    QVERIFY(QFileInfo(recorder.outputPath()).size() > 1024);

    AVFormatContext *input = nullptr;
    QCOMPARE(avformat_open_input(&input, recorder.outputPath().toUtf8().constData(), nullptr, nullptr), 0);
    QCOMPARE(avformat_find_stream_info(input, nullptr), 0);
    QVERIFY(qAbs(static_cast<double>(input->duration) / AV_TIME_BASE - recordingSeconds) < 0.2);
    bool hasVideo = false, hasAudio = false;
    int audioStream = -1, videoStream = -1;
    for (unsigned index = 0; index < input->nb_streams; ++index) {
        if (input->streams[index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) { hasVideo = true; videoStream = static_cast<int>(index); }
        if (input->streams[index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) { hasAudio = true; audioStream = static_cast<int>(index); }
    }
    QVERIFY(hasVideo);
    QVERIFY(hasAudio);
    const AVCodec *audioDecoder = avcodec_find_decoder(input->streams[audioStream]->codecpar->codec_id);
    const AVCodec *videoDecoder = avcodec_find_decoder(input->streams[videoStream]->codecpar->codec_id);
    AVCodecContext *decodedAudio = avcodec_alloc_context3(audioDecoder);
    AVCodecContext *decodedVideo = avcodec_alloc_context3(videoDecoder);
    QVERIFY(avcodec_parameters_to_context(decodedAudio, input->streams[audioStream]->codecpar) >= 0);
    QVERIFY(avcodec_parameters_to_context(decodedVideo, input->streams[videoStream]->codecpar) >= 0);
    QVERIFY(avcodec_open2(decodedAudio, audioDecoder, nullptr) >= 0);
    QVERIFY(avcodec_open2(decodedVideo, videoDecoder, nullptr) >= 0);
    AVPacket *packet = av_packet_alloc(); AVFrame *audioFrame = av_frame_alloc(); AVFrame *videoFrame = av_frame_alloc(); QVector<float> decodedSamples; QImage decodedProgram;
    while (av_read_frame(input, packet) >= 0) {
        if (packet->stream_index == audioStream && avcodec_send_packet(decodedAudio, packet) >= 0)
            while (avcodec_receive_frame(decodedAudio, audioFrame) == 0) {
                const float *samples = reinterpret_cast<const float *>(audioFrame->extended_data[0]);
                for (int sample = 0; sample < audioFrame->nb_samples; ++sample) decodedSamples.append(samples[sample]);
            }
        if (packet->stream_index == videoStream && decodedProgram.isNull() && avcodec_send_packet(decodedVideo, packet) >= 0)
            while (avcodec_receive_frame(decodedVideo, videoFrame) == 0 && decodedProgram.isNull()) {
                decodedProgram = QImage(videoFrame->width, videoFrame->height, QImage::Format_RGB888);
                uint8_t *planes[] = {decodedProgram.bits(), nullptr, nullptr, nullptr}; int strides[] = {static_cast<int>(decodedProgram.bytesPerLine()), 0, 0, 0};
                SwsContext *rgb = sws_getContext(videoFrame->width, videoFrame->height, static_cast<AVPixelFormat>(videoFrame->format), videoFrame->width, videoFrame->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);
                QVERIFY(rgb != nullptr); sws_scale(rgb, videoFrame->data, videoFrame->linesize, 0, videoFrame->height, planes, strides); sws_freeContext(rgb);
            }
        av_packet_unref(packet);
    }
    if (decodedProgram.isNull() && avcodec_send_packet(decodedVideo, nullptr) >= 0)
        while (avcodec_receive_frame(decodedVideo, videoFrame) == 0 && decodedProgram.isNull()) {
            decodedProgram = QImage(videoFrame->width, videoFrame->height, QImage::Format_RGB888);
            uint8_t *planes[] = {decodedProgram.bits(), nullptr, nullptr, nullptr}; int strides[] = {static_cast<int>(decodedProgram.bytesPerLine()), 0, 0, 0};
            SwsContext *rgb = sws_getContext(videoFrame->width, videoFrame->height, static_cast<AVPixelFormat>(videoFrame->format), videoFrame->width, videoFrame->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);
            QVERIFY(rgb != nullptr); sws_scale(rgb, videoFrame->data, videoFrame->linesize, 0, videoFrame->height, planes, strides); sws_freeContext(rgb);
        }
    QVERIFY(!decodedProgram.isNull());
    const auto hasDominant = [](const QColor &color, const int channel) { const int value[] = {color.red(), color.green(), color.blue()}; return value[channel] > 120 && value[channel] > value[(channel + 1) % 3] + 50 && value[channel] > value[(channel + 2) % 3] + 50; };
    QVERIFY(hasDominant(decodedProgram.pixelColor(8, 8), 0));
    QVERIFY(hasDominant(decodedProgram.pixelColor(56, 8), 1));
    QVERIFY(hasDominant(decodedProgram.pixelColor(8, 56), 2));
    const QColor bottomRight = decodedProgram.pixelColor(56, 56);
    QVERIFY(bottomRight.red() > 120 && bottomRight.green() > 120 && bottomRight.blue() < 120);
    QCOMPARE(decodedSamples.size() / 48000, recordingSeconds);
    const auto peakRange = [&decodedSamples](const int begin, const int end) { float peak = 0.0f; for (int index = begin; index < end; ++index) peak = qMax(peak, qAbs(decodedSamples[index])); return peak; };
    QVERIFY(peakRange(0, 48000) > 0.1f);
    QVERIFY(peakRange(decodedSamples.size() / 2 - 24000, decodedSamples.size() / 2 + 24000) > 0.1f);
    QVERIFY(peakRange(decodedSamples.size() - 48000, decodedSamples.size()) > 0.1f);
    QVERIFY(peakRange(0, decodedSamples.size()) < 0.5f);
    av_frame_free(&videoFrame); av_frame_free(&audioFrame); av_packet_free(&packet); avcodec_free_context(&decodedVideo); avcodec_free_context(&decodedAudio);
    avformat_close_input(&input);

    QVERIFY(recorder.start(settings, QSize(64, 64)));
    QTRY_COMPARE(recorder.stateName(), QStringLiteral("Recording"));
    QTest::qWait(120);
    QVERIFY(recorder.elapsedMs() > 0);
    recorder.stop();
    QCOMPARE(recorder.elapsedMs(), 0LL);
}

void StudioProjectTests::recordingDiagnosticsStayOutOfVideoFolder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString videos = directory.filePath(QStringLiteral("videos"));
    const QString logs = directory.filePath(QStringLiteral("logs"));
    RecordingSettings settings;
    settings.outputDirectory = videos;
    settings.detailedDiagnostics = true;
    settings.diagnosticsDirectory = logs;
    LocalRecorder recorder;
    QVERIFY(recorder.start(settings, QSize(64, 64)));
    QTRY_COMPARE(recorder.stateName(), QStringLiteral("Recording"));
    recorder.submitVideoFrame({QImage(64, 64, QImage::Format_ARGB32), 0});
    QTest::qWait(100);
    recorder.stop();
    const QString media = recorder.outputPath();
    QVERIFY(QFileInfo::exists(media));
    QVERIFY(!QFileInfo::exists(media + QStringLiteral(".diagnostics.jsonl")));
    const QString log = RecordingDiagnostics::logPath(media, logs);
    QVERIFY(QFileInfo::exists(log));
    QFile logFile(log);
    QVERIFY(logFile.open(QIODevice::ReadOnly));
    QVERIFY(logFile.readAll().contains("\"event\":\"stop\""));
    logFile.close();

    // Simulate a legacy log that accidentally included credentials. Export must scrub it.
    QVERIFY(logFile.open(QIODevice::WriteOnly | QIODevice::Append));
    QVERIFY(logFile.write("{\"refreshToken\":\"private-refresh\",\"nested\":{\"streamKey\":\"private-stream\"},\"message\":\"Bearer private-bearer\"}\n") > 0);
    logFile.close();
    const QString exported = directory.filePath(QStringLiteral("export.jsonl"));
    QVERIFY(RecordingDiagnostics::exportLatest(exported, logs));
    QFile exportFile(exported);
    QVERIFY(exportFile.open(QIODevice::ReadOnly));
    const QByteArray output = exportFile.readAll();
    QVERIFY(output.contains("\"event\":\"stop\""));
    QVERIFY(!output.contains("private-refresh"));
    QVERIFY(!output.contains("private-stream"));
    QVERIFY(!output.contains("private-bearer"));
}

void StudioProjectTests::writesCompleteSixtySecondMkvWithoutPreview()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    LocalRecorder recorder;
    RecordingSettings settings;
    settings.outputDirectory = directory.path();
    settings.frameRate = 60;
    QVERIFY(recorder.start(settings, QSize(64, 64)));
    QTRY_COMPARE(recorder.state(), RecordingState::Recording);

    QImage image(64, 64, QImage::Format_ARGB32);
    image.fill(Qt::red);
    constexpr int frames = 60 * 60;
    constexpr qint64 audioFrames = 60LL * 48000;
    qint64 audioPosition = 0;
    for (int frame = 0; frame < frames; ++frame) {
        recorder.submitVideoFrame({image, recordingFrameTimestampNs(frame, 60)});
        const qint64 audioTarget = (static_cast<qint64>(frame) + 1) * 800;
        while (audioPosition < audioTarget) {
            const int count = static_cast<int>(qMin<qint64>(1024, audioTarget - audioPosition));
            AudioBlock block{audioPosition * 1'000'000'000LL / 48000, 48000, 2, QVector<float>(count * 2)};
            for (int sample = 0; sample < count; ++sample) {
                const float value = 0.5f * std::sin(2.0 * M_PI * 440.0 * (audioPosition + sample) / 48000.0);
                block.samples[sample * 2] = value;
                block.samples[sample * 2 + 1] = value;
            }
            recorder.submitProgramAudio({AudioProcessing::mixToStereo({block})});
            audioPosition += count;
        }
        while (recorder.diagnostics().value("queuedVideoFrames").toULongLong() > 1 ||
               recorder.diagnostics().value("queuedAudioBlocks").toULongLong() > 48)
            QTest::qWait(1);
    }
    QCOMPARE(audioPosition, audioFrames);
    recorder.stop();
    QCOMPARE(recorder.state(), RecordingState::Idle);
    QCOMPARE(recorder.diagnostics().value("droppedVideoFrames").toULongLong(), 0ULL);
    QCOMPARE(recorder.diagnostics().value("droppedAudioBlocks").toULongLong(), 0ULL);

    AVFormatContext *input = nullptr;
    QCOMPARE(avformat_open_input(&input, recorder.outputPath().toUtf8().constData(), nullptr, nullptr), 0);
    QVERIFY(avformat_find_stream_info(input, nullptr) >= 0);
    int videoStream = -1, audioStream = -1;
    for (unsigned i = 0; i < input->nb_streams; ++i) {
        const auto kind = input->streams[i]->codecpar->codec_type;
        if (kind == AVMEDIA_TYPE_VIDEO) videoStream = static_cast<int>(i);
        if (kind == AVMEDIA_TYPE_AUDIO) audioStream = static_cast<int>(i);
    }
    QVERIFY(videoStream >= 0);
    QVERIFY(audioStream >= 0);
    int videoPackets = 0;
    qint64 lastVideoEnd = -1, lastAudioEnd = -1;
    AVPacket *packet = av_packet_alloc();
    while (av_read_frame(input, packet) >= 0) {
        if (packet->stream_index == videoStream) {
            ++videoPackets;
            lastVideoEnd = qMax(lastVideoEnd, packet->pts + packet->duration);
        } else if (packet->stream_index == audioStream) {
            lastAudioEnd = qMax(lastAudioEnd, packet->pts + packet->duration);
        }
        av_packet_unref(packet);
    }
    QCOMPARE(videoPackets, frames);
    const double videoSeconds = lastVideoEnd * av_q2d(input->streams[videoStream]->time_base);
    const double audioSeconds = lastAudioEnd * av_q2d(input->streams[audioStream]->time_base);
    qInfo().noquote() << QStringLiteral("60-second MKV: video packets=%1, video end=%2 s, audio end=%3 s")
                             .arg(videoPackets).arg(videoSeconds, 0, 'f', 3).arg(audioSeconds, 0, 'f', 3);
    QVERIFY2(qAbs(videoSeconds - 60.0) < 0.05, qPrintable(QStringLiteral("Video ended at %1 s").arg(videoSeconds)));
    QVERIFY2(qAbs(audioSeconds - 60.0) < 0.1, qPrintable(QStringLiteral("Audio ended at %1 s").arg(audioSeconds)));
    QVERIFY2(qAbs(audioSeconds - videoSeconds) < 0.1,
             qPrintable(QStringLiteral("A/V duration mismatch: video %1 s, audio %2 s")
                 .arg(videoSeconds, 0, 'f', 3).arg(audioSeconds, 0, 'f', 3)));
    av_packet_free(&packet);
    avformat_close_input(&input);
}

void StudioProjectTests::keepsAacTimelineBoundToAudioTimestamps()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    LocalRecorder recorder;
    RecordingSettings settings;
    settings.outputDirectory = directory.path();
    settings.frameRate = 60;
    QVERIFY(recorder.start(settings, QSize(32, 32)));
    QTRY_COMPARE(recorder.state(), RecordingState::Recording);

    QImage image(32, 32, QImage::Format_ARGB32);
    image.fill(Qt::blue);
    for (int frame = 0; frame < 60 * 60; ++frame)
        recorder.submitVideoFrame({image, recordingFrameTimestampNs(frame, 60)});

    // Defensive duplicate PROGRAM submission, not a model of separate devices.
    // Real source overlap is now summed in ProgramAudioMixer before this API.
    for (int block = 0; block < 60 * 200; ++block) {
        AudioBlock audio{block * 5'000'000LL, 48000, 2, QVector<float>(480 * 2, 0.5f)};
        recorder.submitProgramAudio({AudioProcessing::mixToStereo({audio})});
        while (recorder.diagnostics().value("queuedAudioBlocks").toULongLong() > 48)
            QTest::qWait(1);
    }
    recorder.stop();

    AVFormatContext *input = nullptr;
    QCOMPARE(avformat_open_input(&input, recorder.outputPath().toUtf8().constData(), nullptr, nullptr), 0);
    QVERIFY(avformat_find_stream_info(input, nullptr) >= 0);
    int audioStream = -1;
    for (unsigned i = 0; i < input->nb_streams; ++i)
        if (input->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) audioStream = static_cast<int>(i);
    QVERIFY(audioStream >= 0);
    qint64 end = 0;
    AVPacket *packet = av_packet_alloc();
    while (av_read_frame(input, packet) >= 0) {
        if (packet->stream_index == audioStream) end = qMax(end, packet->pts + packet->duration);
        av_packet_unref(packet);
    }
    const double seconds = end * av_q2d(input->streams[audioStream]->time_base);
    QVERIFY2(qAbs(seconds - 60.0) < 0.1, qPrintable(QStringLiteral("AAC ended at %1 s").arg(seconds)));
    av_packet_free(&packet);
    avformat_close_input(&input);
}
QTEST_GUILESS_MAIN(StudioProjectTests)

#include "StudioProjectTests.moc"
