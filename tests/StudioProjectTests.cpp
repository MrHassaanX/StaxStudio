#include "core/project/StudioProject.h"
#include "core/audio/AudioProcessing.h"
#include "core/project/StudioRepository.h"
#include "core/render/CompositorScene.h"
#include "core/render/ProgramFrameMath.h"
#include "core/recorder/LocalRecorder.h"
#include "ui/controllers/StudioModels.h"
#include "ui/controllers/DockLayout.h"

#include <QFile>
#include <QTemporaryDir>
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
};

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
        recorder.submitAudioBlock(std::move(audio));
        // Let the bounded asynchronous recorder consume a realistic burst
        // before producing more audio. A lost tail must fail this test.
        if ((blockIndex + 1) % 64 == 0) QTest::qWait(20);
    }
    recorder.stop();
    QCOMPARE(recorder.stateName(), QStringLiteral("Idle"));
    QCOMPARE(recorder.elapsedMs(), 0LL);
    QVERIFY(QFileInfo::exists(recorder.outputPath()));
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
            recorder.submitAudioBlock(std::move(block));
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
    av_packet_free(&packet);
    avformat_close_input(&input);
}
QTEST_GUILESS_MAIN(StudioProjectTests)

#include "StudioProjectTests.moc"
