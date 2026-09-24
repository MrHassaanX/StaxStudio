#include "ui/controllers/AppController.h"
#include "ui/controllers/StudioController.h"
#include "ui/render/ProgramPreview.h"

#include <QAbstractItemModelTester>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

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
    for (const QSize size : {QSize(1280,760), QSize(1440,900), QSize(1920,1080), QSize(2560,1440)}) {
        window->resize(size);
        QTest::qWait(50);
        auto *panel = find("controlsPanel");
        auto *button = find("combinedOutputButton");
        QVERIFY(panel && button);
        const QPointF bottom = button->mapToItem(panel, QPointF(button->width(), button->height()));
        QVERIFY2(bottom.y() <= panel->height() && bottom.x() <= panel->width(), "Output controls overflow");
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
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");
    qmlRegisterType<ProgramPreview>("StaxStudio.Render", 1, 0, "ProgramPreview");
    StudioUiTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "StudioUiTests.moc"
