#include "core/project/StudioProject.h"
#include "core/project/StudioRepository.h"
#include "ui/controllers/StudioModels.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class StudioProjectTests final : public QObject
{
    Q_OBJECT
private slots:
    void createsAndSelectsScenes();
    void protectsValidSceneState();
    void reordersScenesAndItems();
    void managesSourceLayerState();
    void appliesAndPersistsTransformOperations();
    void persistsAndRestoresProject();
    void recoversFromInvalidConfiguration();
    void recoversFromUnsupportedSchema();
    void notifiesSceneModelMutations();
    void notifiesSourceModelMutations();
    void keepsModelsConsistentThroughRepeatedLifecycleChanges();
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

void StudioProjectTests::persistsAndRestoresProject()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    StudioRepository repository(directory.path());
    StudioProject project = StudioProject::createDefault();
    project.profileName = QStringLiteral("Creator Profile");
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

QTEST_APPLESS_MAIN(StudioProjectTests)

#include "StudioProjectTests.moc"
