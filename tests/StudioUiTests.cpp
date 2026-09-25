#include "ui/controllers/AppController.h"
#include "ui/controllers/StudioController.h"
#include "ui/render/ProgramPreview.h"
#include "core/render/ProgramRenderEngine.h"
#include "core/render/ProgramFrameMath.h"
#include "core/recorder/LocalRecorder.h"

#include <QAbstractItemModelTester>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>
#include <QElapsedTimer>
#ifdef Q_OS_WIN
#include <d3d11.h>
#include <wrl/client.h>
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

namespace {
QQuickItem *visualItem(QQuickItem *parent, const QString &name)
{
    if (parent->objectName() == name) return parent;
    for (auto *child : parent->childItems()) {
        if (auto *found = visualItem(child, name)) return found;
    }
    return nullptr;
}

QString role(QAbstractItemModel *model, int row, int role)
{
    return model->data(model->index(row, 0), role).toString();
}
}

class StudioUiTests final : public QObject
{
    Q_OBJECT
private slots:
    void visibleRowsAndRepeatedLifecycle();
    void captureConfigurationUsesMonotonicTimestamps();
    void offscreenRecordingNeedsNoPreviewWindow();
    void draggingUpdatesOnlyOneRenderItem();
    void outputSurvivesPreviewDetach();
    void gpuPreviewShowsAllCorners();
    void dockMouseInteractions();
};

void StudioUiTests::visibleRowsAndRepeatedLifecycle()
{
    QTest::failOnWarning(QRegularExpression(".*"));
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppController navigation;
    StudioController controller(directory.path());
    navigation.setGpuPreviewEnabled(false);
    QAbstractItemModelTester sceneContract(controller.scenesModel(), QAbstractItemModelTester::FailureReportingMode::QtTest);
    QAbstractItemModelTester sourceContract(controller.sceneItemsModel(), QAbstractItemModelTester::FailureReportingMode::QtTest);
    QAbstractItemModelTester mixerContract(controller.mixerModel(), QAbstractItemModelTester::FailureReportingMode::QtTest);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appController", &navigation);
    engine.rootContext()->setContextProperty("studioController", &controller);
    navigation.setActivePage("studio");
    engine.load(QUrl::fromLocalFile(QStringLiteral(STAX_SOURCE_DIR "/ui/qml/Main.qml")));
    QCOMPARE(engine.rootObjects().size(), 1);
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QVERIFY(window);
    window->resize(1920, 1080);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    auto find = [&](const QString &name) { return visualItem(window->contentItem(), name); };
    auto *scenes = controller.scenesModel();
    auto *items = controller.sceneItemsModel();
    auto *mixer = controller.mixerModel();
    QTRY_VERIFY(find("sceneRow_" + role(scenes, 0, SceneListModel::IdRole)));
    QCOMPARE(mixer->rowCount(), 0);

    // Exercise the actual popup and mouse path, not only controller mutations.
    const QString firstScene = role(scenes, 0, SceneListModel::IdRole);
    auto *firstRow = find("sceneRow_" + firstScene);
    auto click = [&](QQuickItem *item) {
        if (!item) return false;
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                         item->mapToScene(QPointF(item->width()/2, item->height()/2)).toPoint());
        return true;
    };
    QVERIFY(click(visualItem(firstRow, "Scene options")));
    QTRY_VERIFY(find("Move down"));
    QVERIFY2(find("Move down")->width() >= 150, "Scene menu must have usable item width");
    QVERIFY(click(find("Move down")));
    QTRY_COMPARE(role(scenes, 1, SceneListModel::IdRole), firstScene);
    QVERIFY(click(visualItem(find("sceneRow_" + firstScene), "Scene options")));
    QTRY_VERIFY(find("Move up"));
    QVERIFY(click(find("Move up")));
    QTRY_COMPARE(role(scenes, 0, SceneListModel::IdRole), firstScene);

    for (int cycle = 0; cycle < 15; ++cycle) {
        controller.addScene(QStringLiteral("QA Scene %1").arg(cycle));
        const QString sceneId = role(scenes, scenes->rowCount() - 1, SceneListModel::IdRole);
        const QString rowName = "sceneRow_" + sceneId;
        QTRY_VERIFY(find(rowName));
        QVERIFY(find(rowName)->isVisible());
        QVERIFY(find(rowName)->height() > 0);
        QVERIFY(find(rowName)->property("active").toBool());
        QCOMPARE(items->rowCount(), 0);
        QCOMPARE(mixer->rowCount(), 0);
        auto *canvas = find("previewCanvas");
        QVERIFY(canvas);
        QTest::qWait(30);
        const QPoint center = canvas->mapToScene(QPointF(canvas->width()/2, canvas->height()/2)).toPoint();
        const QImage screenshot = window->grabWindow();
        QVERIFY(!screenshot.isNull());
        QCOMPARE(screenshot.pixelColor(center * window->devicePixelRatio()), QColor(Qt::black));
        controller.renameScene(sceneId, "Renamed QA Scene");
        QTRY_COMPARE(find(rowName)->property("name").toString(), QString("Renamed QA Scene"));
        controller.moveScene(sceneId, -1);
        controller.moveScene(sceneId, 1);
        controller.moveScene(sceneId, 0);
        controller.addSource("Window Capture", "QA Window");
        const QString itemId = role(items, 0, SceneItemListModel::IdRole);
        const QString sourceId = role(items, 0, SceneItemListModel::SourceIdRole);
        const QString sourceRow = "sourceRow_" + itemId;
        QTRY_VERIFY(find(sourceRow));
        QVERIFY(find(sourceRow)->isVisible());
        controller.selectItem(itemId);
        controller.renameSource(sourceId, "Renamed Window");
        QVERIFY(click(visualItem(find(sourceRow), "Hide source")));
        QVERIFY(click(visualItem(find(sourceRow), "Lock source")));
        QTRY_COMPARE(find(sourceRow)->property("name").toString(), QString("Renamed Window"));
        QVERIFY(find(sourceRow)->property("selected").toBool());
        QVERIFY(!find(sourceRow)->property("itemVisible").toBool());
        QVERIFY(find(sourceRow)->property("itemLocked").toBool());
        QVERIFY(click(visualItem(find(sourceRow), "Show source")));
        QVERIFY(click(visualItem(find(sourceRow), "Unlock source")));
        controller.addSource("Microphone", "QA Mic");
        const QString micItem = role(items, 1, SceneItemListModel::IdRole);
        const QString micSource = role(items, 1, SceneItemListModel::SourceIdRole);
        QTRY_VERIFY(find("sourceRow_" + micItem));
        QTRY_VERIFY(find("mixerRow_" + micSource));
        QCOMPARE(mixer->rowCount(), 1);
        controller.setMixerVolume(micSource, 0.42);
        controller.setMixerMuted(micSource, true);
        controller.renameSource(micSource, "Renamed Mic");
        QTRY_COMPARE(find("mixerRow_" + micSource)->property("name").toString(), QString("Renamed Mic"));
        controller.moveSceneItem(micItem, -1);
        controller.moveSceneItem(micItem, 1);
        controller.moveSceneItem(micItem, 0);
        const QString otherScene = role(scenes, 0, SceneListModel::IdRole);
        controller.selectScene(otherScene);
        QCOMPARE(items->rowCount(), 0);
        QCOMPARE(mixer->rowCount(), 0);
        controller.selectScene(sceneId);
        QTRY_VERIFY(find(sourceRow));
        QTRY_VERIFY(find("mixerRow_" + micSource));
        StudioController restored(directory.path());
        QCOMPARE(restored.activeSceneName(), controller.activeSceneName());
        QCOMPARE(restored.sceneItemsModel()->rowCount(), 2);
        QCOMPARE(restored.mixerModel()->data(restored.mixerModel()->index(0,0), MixerListModel::VolumeRole).toDouble(), 0.42);
        controller.removeSceneItem(micItem);
        QTRY_VERIFY(!find("sourceRow_" + micItem));
        QTRY_VERIFY(!find("mixerRow_" + micSource));
        controller.removeSceneItem(itemId);
        QTRY_VERIFY(!find(sourceRow));
        controller.deleteScene(sceneId);
        QTRY_VERIFY(!find(rowName));
        QCOMPARE(scenes->rowCount(), 3);
    }
    controller.addSource("Color Source", "Resize fixture");
    const QString resizeItem = role(items, 0, SceneItemListModel::IdRole);
    QVariantMap resizeTransform = controller.itemTransform(resizeItem);
    resizeTransform["x"] = 200.0;
    resizeTransform["y"] = 120.0;
    controller.setItemTransform(resizeItem, resizeTransform);
    const QVariantMap persistentTransform = controller.itemTransform(resizeItem);
    for (const QSize size : {QSize(1280,760), QSize(1440,900), QSize(1920,1080), QSize(2560,1440)}) {
        window->resize(size);
        QTest::qWait(50);
        auto *canvas = find("previewCanvas");
        auto *preview = find("previewPanel");
        auto *previewWorkspace = find("previewWorkspace");
        auto *topWorkspace = find("topWorkspace");
        auto *workspace = find("studioWorkspace");
        auto *overlay = find("editorOverlay_" + resizeItem);
        auto *scaleMode = find("previewScaleMode");
        auto *scenesPanel = find("scenesPanel");
        auto *sourcesPanel = find("sourcesPanel");
        auto *mixerPanel = find("mixerPanel");
        auto *transitionPanel = find("transitionPanel");
        auto *controlsPanel = find("controlsPanel");
        QVERIFY(canvas);
        QVERIFY(preview && previewWorkspace && topWorkspace && workspace && overlay && scaleMode);
        QCOMPARE(scaleMode->property("currentText").toString(), QStringLiteral("Fit"));
        QVERIFY(scenesPanel && sourcesPanel && mixerPanel && transitionPanel && controlsPanel);
        const QString sizing = QStringLiteral("top=%1 preview=%2 workspace=%3 controls=%4 window=%5x%6")
                                   .arg(topWorkspace->height()).arg(preview->height()).arg(workspace->height()).arg(controlsPanel->height())
                                   .arg(window->width()).arg(window->height());
        QVERIFY2(topWorkspace->height() <= workspace->height() * 0.62, qPrintable(sizing));
        QVERIFY2(topWorkspace->height() >= workspace->height() * 0.50, qPrintable(sizing));
        QVERIFY(controlsPanel->height() >= workspace->height() * 0.34);
        const QList<QQuickItem *> docks{scenesPanel, sourcesPanel, mixerPanel, transitionPanel, controlsPanel};
        const qreal dockTop = docks.first()->mapToScene(QPointF(0, 0)).y();
        qreal previousRight = 0;
        for (QQuickItem *dock : docks) {
            QVERIFY(dock->isVisible() && dock->width() >= 160);
            QVERIFY(qAbs(dock->mapToScene(QPointF(0, 0)).y() - dockTop) <= 2);
            const QPointF topLeft = dock->mapToScene(QPointF(0, 0));
            const QPointF bottomRight = dock->mapToScene(QPointF(dock->width(), dock->height()));
            QVERIFY(topLeft.x() >= previousRight);
            QVERIFY(bottomRight.x() <= window->width() - 8);
            QVERIFY(bottomRight.y() <= window->height() - 8);
            previousRight = bottomRight.x();
        }
        QVERIFY(qAbs(canvas->width() / canvas->height() - 16.0 / 9.0) < 0.01);
        const QPointF canvasTopLeft = canvas->mapToItem(previewWorkspace, QPointF(0, 0));
        QVERIFY(qAbs(canvasTopLeft.x() - (previewWorkspace->width() - canvas->width()) / 2) <= 1);
        QVERIFY(qAbs(canvasTopLeft.y() - (previewWorkspace->height() - canvas->height()) / 2) <= 1);
        QVERIFY(canvasTopLeft.x() >= 15 && canvasTopLeft.y() >= 15);
        const QPointF overlayTopLeft = overlay->mapToItem(canvas, QPointF(0, 0));
        QVERIFY(qAbs(overlayTopLeft.x() - 200.0 / controller.programWidth() * canvas->width()) <= 1);
        QVERIFY(qAbs(overlayTopLeft.y() - 120.0 / controller.programHeight() * canvas->height()) <= 1);
        QCOMPARE(controller.itemTransform(resizeItem), persistentTransform);
        auto *program = qobject_cast<ProgramRenderEngine *>(controller.programEngine());
        QVERIFY(program);
        QCOMPARE(program->programSize(), QSize(1920, 1080));
        auto *button = find("recordingButton");
        QVERIFY(button);
        const QPointF bottom = button->mapToItem(controlsPanel, QPointF(button->width(), button->height()));
        QVERIFY2(bottom.y() <= controlsPanel->height() && bottom.x() <= controlsPanel->width(), "Recording control overflows");
        QVERIFY(!find("combinedOutputButton"));
        auto *smart = find("smartModePanel");
        auto *text = find("smartModeDescription");
        QVERIFY(smart && text);
        const QPointF corner = text->mapToItem(smart, QPointF(text->width(), text->height()));
        QVERIFY(corner.x() <= smart->width() && corner.y() <= smart->height());
    }
    while (scenes->rowCount() > 1) controller.deleteScene(role(scenes, 0, SceneListModel::IdRole));
    controller.deleteScene(role(scenes, 0, SceneListModel::IdRole));
    QCOMPARE(scenes->rowCount(), 1);
}

void StudioUiTests::offscreenRecordingNeedsNoPreviewWindow()
{
#ifdef Q_OS_WIN
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    LocalRecorder recorder;
    ProgramRenderEngine program;
    QVariantList layers;
    const auto tile = [&](const QString &id, const QColor &color, const int x, const int y) {
        layers.append(QVariantMap{{"itemId", id}, {"sourceId", id}, {"sourceType", "Color Source"},
                                  {"color", color}, {"x", x}, {"y", y}, {"width", 32.0}, {"height", 32.0},
                                  {"scaleX", 1.0}, {"scaleY", 1.0}, {"zOrder", layers.size()}});
    };
    tile("top-left", Qt::red, 0, 0);
    tile("top-right", Qt::green, 32, 0);
    tile("bottom-left", Qt::blue, 0, 32);
    tile("bottom-right", Qt::yellow, 32, 32);
    program.setScene(layers, QSize(64, 64));
    program.setRecorder(&recorder);
    RecordingSettings settings;
    settings.outputDirectory = directory.path();
    settings.frameRate = 60;
    QVERIFY(recorder.start(settings, QSize(64, 64)));
    QTRY_COMPARE_WITH_TIMEOUT(recorder.state(), RecordingState::Recording, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(program.renderedFrames() >= 120, 15000);
    recorder.stop();
    QCOMPARE(recorder.state(), RecordingState::Idle);
    AVFormatContext *input = nullptr;
    QCOMPARE(avformat_open_input(&input, recorder.outputPath().toUtf8().constData(), nullptr, nullptr), 0);
    QVERIFY(avformat_find_stream_info(input, nullptr) >= 0);
    int videoStream = -1;
    for (unsigned i = 0; i < input->nb_streams; ++i)
        if (input->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) videoStream = static_cast<int>(i);
    QVERIFY(videoStream >= 0);
    AVPacket *packet = av_packet_alloc();
    const AVCodec *codec = avcodec_find_decoder(input->streams[videoStream]->codecpar->codec_id);
    QVERIFY(codec);
    AVCodecContext *decoder = avcodec_alloc_context3(codec);
    QVERIFY(avcodec_parameters_to_context(decoder, input->streams[videoStream]->codecpar) >= 0);
    QVERIFY(avcodec_open2(decoder, codec, nullptr) >= 0);
    AVFrame *decoded = av_frame_alloc();
    QImage firstFrame;
    int frames = 0;
    qint64 lastPts = -1;
    while (av_read_frame(input, packet) >= 0) {
        if (packet->stream_index == videoStream) {
            ++frames;
            lastPts = qMax(lastPts, packet->pts);
            if (firstFrame.isNull() && avcodec_send_packet(decoder, packet) >= 0 && avcodec_receive_frame(decoder, decoded) == 0) {
                firstFrame = QImage(decoded->width, decoded->height, QImage::Format_RGB888);
                uint8_t *planes[] = {firstFrame.bits(), nullptr, nullptr, nullptr};
                int strides[] = {static_cast<int>(firstFrame.bytesPerLine()), 0, 0, 0};
                SwsContext *scale = sws_getContext(decoded->width, decoded->height, static_cast<AVPixelFormat>(decoded->format),
                                                   decoded->width, decoded->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);
                QVERIFY(scale);
                sws_scale(scale, decoded->data, decoded->linesize, 0, decoded->height, planes, strides);
                sws_freeContext(scale);
            }
        }
        av_packet_unref(packet);
    }
    QVERIFY2(frames >= 105, qPrintable(QStringLiteral("Only %1 video packets without preview").arg(frames)));
    const double lastSeconds = lastPts * av_q2d(input->streams[videoStream]->time_base);
    QVERIFY2(lastSeconds >= 1.7, qPrintable(QStringLiteral("Video stopped at %1 s").arg(lastSeconds)));
    QVERIFY(!firstFrame.isNull());
    const auto topLeft = firstFrame.pixelColor(8, 8);
    const auto topRight = firstFrame.pixelColor(56, 8);
    const auto bottomLeft = firstFrame.pixelColor(8, 56);
    const auto bottomRight = firstFrame.pixelColor(56, 56);
    QVERIFY(topLeft.red() > topLeft.green() + 50 && topLeft.red() > topLeft.blue() + 50);
    QVERIFY(topRight.green() > topRight.red() + 50 && topRight.green() > topRight.blue() + 50);
    QVERIFY(bottomLeft.blue() > bottomLeft.red() + 50 && bottomLeft.blue() > bottomLeft.green() + 50);
    QVERIFY(bottomRight.red() > 100 && bottomRight.green() > 100 && bottomRight.blue() < 80);
    av_frame_free(&decoded);
    avcodec_free_context(&decoder);
    av_packet_free(&packet);
    avformat_close_input(&input);
#else
    QSKIP("D3D11 offscreen program output requires Windows");
#endif
}

void StudioUiTests::draggingUpdatesOnlyOneRenderItem()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    StudioController controller(directory.path());
    controller.addSource("Color Source", "First");
    controller.addSource("Color Source", "Second");
    auto *items = controller.sceneItemsModel();
    QCOMPARE(items->rowCount(), 2);
    const QString firstId = role(items, 0, SceneItemListModel::IdRole);
    const QString secondId = role(items, 1, SceneItemListModel::IdRole);
    const QVariantMap originalFirst = controller.itemTransform(firstId);
    const QVariantMap originalSecond = controller.itemTransform(secondId);
    auto *program = qobject_cast<ProgramRenderEngine *>(controller.programEngine());
    QVERIFY(program);
    const quint64 fullRebuilds = program->sceneRebuilds();
    const quint64 itemUpdates = program->transformUpdates();
    QSignalSpy fullChange(&controller, &StudioController::projectChanged);
    QVariantMap moved = originalSecond;
    moved["x"] = moved.value("x").toDouble() + 120.0;
    controller.previewItemTransform(secondId, moved);
    QCOMPARE(controller.itemTransform(firstId), originalFirst);
    QCOMPARE(controller.itemTransform(secondId).value("x").toDouble(), moved.value("x").toDouble());
    QCOMPARE(program->sceneRebuilds(), fullRebuilds);
    QCOMPARE(program->transformUpdates(), itemUpdates + 1);
    QCOMPARE(fullChange.size(), 0);
    StudioController beforeCommit(directory.path());
    QCOMPARE(beforeCommit.itemTransform(secondId).value("x").toDouble(), originalSecond.value("x").toDouble());
    controller.commitPreviewTransform();
    QCOMPARE(fullChange.size(), 1);
    StudioController afterCommit(directory.path());
    QCOMPARE(afterCommit.itemTransform(secondId).value("x").toDouble(), moved.value("x").toDouble());
}

void StudioUiTests::captureConfigurationUsesMonotonicTimestamps()
{
    const qint64 first = mediaTimestampNs();
    QTest::qWait(2);
    QVERIFY(mediaTimestampNs() > first);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    StudioController controller(directory.path());
    controller.addSource("Display Capture", "Configured display");
    auto *items = controller.sceneItemsModel();
    QCOMPARE(items->rowCount(), 1);
    const QString sourceId = role(items, 0, SceneItemListModel::SourceIdRole);
    const QVariantList displays = controller.captureTargets("Display Capture");
    if (!displays.isEmpty()) {
        const QString targetId = displays.first().toMap().value("id").toString();
        controller.configureCaptureSource(sourceId, targetId, QString{}, false);
        const QVariantMap configuration = controller.sourceConfiguration(sourceId);
        QCOMPARE(configuration.value("targetId").toString(), targetId);
        QCOMPARE(configuration.value("captureCursor").toBool(), false);
    }
}

void StudioUiTests::outputSurvivesPreviewDetach()
{
#ifdef Q_OS_WIN
    using Microsoft::WRL::ComPtr;
    QTemporaryDir directory;
    LocalRecorder recorder;
    ProgramRenderEngine program;
    const bool desktop=qEnvironmentVariableIsSet("STAX_TEST_DESKTOP_OUTPUT");
    const QSize size=desktop ? QSize(1920,1080) : QSize(64,64);
    QVariantMap layer{{"itemId","test"},{"sourceId","test"},{"sourceType",desktop ? "Display Capture" : "Color Source"},
        {"targetId",QStringLiteral("\\\\.\\DISPLAY1")},{"color",QColor(Qt::green)},{"x",0},{"y",0},
        {"width",size.width()},{"height",size.height()},{"scaleX",1.0},{"scaleY",1.0},{"zOrder",0}};
    program.setScene({layer},size); program.setRecorder(&recorder);
    ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context; ComPtr<ID3D11Texture2D> texture;
    QVERIFY(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)));
    D3D11_TEXTURE2D_DESC desc{}; desc.Width=size.width();desc.Height=size.height();
    desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
    desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    QVERIFY(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&texture)));
    RecordingSettings settings;settings.outputDirectory=directory.path();settings.frameRate=60;
    QVERIFY(recorder.start(settings,size));
    QTRY_COMPARE_WITH_TIMEOUT(recorder.state(),RecordingState::Recording,10000);
    QElapsedTimer timer;timer.start();
    const int seconds=desktop ? 85 : 60;
    quint64 atDetach=0, beforeAttach=0;
    int copies=0;
    while(timer.elapsed()<seconds*1000) {
        const auto elapsed=timer.elapsed();
        if(elapsed<seconds*1000/3 || elapsed>=seconds*2000/3)
            if(program.copyLatestTo(device.Get(),context.Get(),texture.Get())) ++copies;
        if(elapsed<seconds*1000/3) atDetach=program.renderedFrames();
        if(elapsed<seconds*2000/3) beforeAttach=program.renderedFrames();
        QTest::qWait(15);
    }
    const auto stats=program.diagnostics();
    recorder.stop();
    qInfo()<<"Production-path results"<<stats<<"recorder"<<recorder.diagnostics();
    QVERIFY(copies>20);
    QVERIFY(beforeAttach>atDetach+quint64(seconds*15));
    QVERIFY(stats.value("lastRenderNs").toLongLong()>0);
    QVERIFY(stats.value("produced").toULongLong()>=quint64(seconds*55));
    QCOMPARE(recorder.state(),RecordingState::Idle);
    AVFormatContext *input=nullptr;
    QCOMPARE(avformat_open_input(&input,recorder.outputPath().toUtf8().constData(),nullptr,nullptr),0);
    QVERIFY(avformat_find_stream_info(input,nullptr)>=0);
    AVPacket *packet=av_packet_alloc(); double last=-1;int count=0;
    while(av_read_frame(input,packet)>=0) {
        if(input->streams[packet->stream_index]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO) {
            const double pts=packet->pts*av_q2d(input->streams[packet->stream_index]->time_base);
            QVERIFY(pts>last);last=pts;++count;
        }
        av_packet_unref(packet);
    }
    av_packet_free(&packet);avformat_close_input(&input);
    QVERIFY2(last>seconds-0.5,qPrintable(QString("Video ended at %1; expected %2").arg(last).arg(seconds)));
    QVERIFY2(count>=seconds*55,qPrintable(QString("Expected %1 frames at 60 FPS; encoded %2, diagnostics above").arg(seconds*60).arg(count)));
#else
    QSKIP("D3D11 test requires Windows");
#endif
}

void StudioUiTests::gpuPreviewShowsAllCorners()
{
    if(qEnvironmentVariable("QT_QUICK_BACKEND")=="software")
        QSKIP("Run this test separately with the D3D11 Qt Quick backend");
#ifdef Q_OS_WIN
    ProgramRenderEngine program;
    QVariantList layers;
    const QList<QColor> colors{Qt::red,Qt::green,Qt::blue,Qt::yellow};
    for(int i=0;i<4;++i) layers.append(QVariantMap{{"itemId",QString::number(i)},{"sourceType","Color Source"},
        {"color",colors[i]},{"x",(i%2)*160},{"y",(i/2)*90},{"width",160},{"height",90},
        {"scaleX",1.0},{"scaleY",1.0},{"zOrder",i}});
    program.setScene(layers,{320,180});
    QQuickWindow window;
    window.setColor(Qt::black);
    auto *preview=new ProgramPreview(window.contentItem());
    preview->setEngine(&program);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    for(const QSize workspace:{QSize(640,360),QSize(600,450),QSize(800,240),QSize(300,600)}) {
        window.resize(workspace);
        const QRectF rect=ProgramFrameMath::bestFitPreviewRect({320,180},workspace);
        preview->setPosition(rect.topLeft());preview->setSize(rect.size());
        QTest::qWait(400);
        const QImage image=window.grabWindow();QVERIFY(!image.isNull());
        const auto pixel=[&](double x,double y){return image.pixelColor(((rect.topLeft()+QPointF(rect.width()*x,rect.height()*y))*window.devicePixelRatio()).toPoint());};
        const auto tl=pixel(.1,.1),tr=pixel(.9,.1),bl=pixel(.1,.9),br=pixel(.9,.9);
        QVERIFY(tl.red()>tl.green()+50 && tl.red()>tl.blue()+50);
        QVERIFY(tr.green()>tr.red()+50 && tr.green()>tr.blue()+50);
        QVERIFY(bl.blue()>bl.red()+50 && bl.blue()>bl.green()+50);
        QVERIFY(br.red()>100 && br.green()>100 && br.blue()<80);
    }
#endif
}


void StudioUiTests::dockMouseInteractions()
{
    QTest::failOnWarning(QRegularExpression(".*"));
    QTemporaryDir directory;
    AppController navigation; StudioController controller(directory.path());
    navigation.setGpuPreviewEnabled(false); navigation.setActivePage("studio");
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appController",&navigation);
    engine.rootContext()->setContextProperty("studioController",&controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(STAX_SOURCE_DIR "/ui/qml/Main.qml")));
    QCOMPARE(engine.rootObjects().size(),1);
    auto *window=qobject_cast<QQuickWindow *>(engine.rootObjects().first());QVERIFY(window);
    window->resize(1440,900);QVERIFY(QTest::qWaitForWindowExposed(window));
    auto find=[&](QString id){return visualItem(window->contentItem(),id);};
    auto drag=[&](QPoint from,QPoint to) {
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,from);
        QTest::mouseMove(window,from+(to-from)/2,30);
        QTest::mouseMove(window,to,30);
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,to);
        QTest::qWait(40);
    };
    auto *workspace=find("studioWorkspace");QVERIFY(workspace);
    auto *header=find("dockHeader_scenes");QVERIFY(header);
    auto *preview=find("topWorkspace");QVERIFY(preview);
    drag(header->mapToScene({20,12}).toPoint(),preview->mapToScene({4,preview->height()/2}).toPoint());
    auto panels=controller.dockLayout()->arrange(workspace->width(),workspace->height()).value("panels").toMap();
    QVERIFY(panels["scenes"].toMap()["x"].toDouble()<panels["preview"].toMap()["x"].toDouble());
    QVERIFY(find("scenesPanel")->mapToScene({0,0}).x()<find("topWorkspace")->mapToScene({0,0}).x());
    header=find("dockHeader_sources");
    auto *scenes=find("scenesPanel");
    drag(header->mapToScene({20,12}).toPoint(),scenes->mapToScene({scenes->width()/2,scenes->height()-3}).toPoint());
    QVERIFY(find("sourcesPanel")->mapToScene({0,0}).y()>find("scenesPanel")->mapToScene({0,0}).y());
    const auto handles=controller.dockLayout()->arrange(workspace->width(),workspace->height()).value("handles").toList();
    bool horizontal=false,vertical=false;
    for(const auto &entry:handles) {
        const auto info=entry.toMap();
        const bool h=info["horizontal"].toBool();
        if(h ? horizontal : vertical) continue;
        auto *splitter=find("dockSplitter_"+info["id"].toString());QVERIFY(splitter);
        const auto before=controller.dockLayout()->state();
        const QPoint from=splitter->mapToScene({splitter->width()/2,splitter->height()/2}).toPoint();
        drag(from,from+(h ? QPoint(35,0) : QPoint(0,35)));
        QVERIFY(controller.dockLayout()->state()!=before);
        if(h) horizontal=true;else vertical=true;
    }
    QVERIFY(horizontal && vertical);
    controller.dockLayout()->setLocked(true);
    const auto locked=controller.dockLayout()->state();
    header=find("dockHeader_scenes");preview=find("topWorkspace");
    drag(header->mapToScene({20,12}).toPoint(),preview->mapToScene({preview->width()-3,20}).toPoint());
    QCOMPARE(controller.dockLayout()->state(),locked);
    controller.dockLayout()->reset();QTest::qWait(40);
    controller.dockLayout()->setPanelVisible("mixer",false);QTest::qWait(40);
    QVERIFY(!find("mixerPanel")->isVisible());
    controller.dockLayout()->save();
    DockLayout restored(directory.path());QCOMPARE(restored.state(),controller.dockLayout()->state());
    controller.dockLayout()->setPanelVisible("mixer",true);QTest::qWait(40);
    QVERIFY(find("mixerPanel")->isVisible());
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    QQuickStyle::setStyle("Basic");
    qmlRegisterType<ProgramPreview>("StaxStudio.Render", 1, 0, "ProgramPreview");
    StudioUiTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "StudioUiTests.moc"
