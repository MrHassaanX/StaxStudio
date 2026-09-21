#include "core/project/StudioProject.h"
#include "core/project/StudioRepository.h"

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
    void persistsAndRestoresProject();
    void recoversFromInvalidConfiguration();
    void recoversFromUnsupportedSchema();
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
}

void StudioProjectTests::persistsAndRestoresProject()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    StudioRepository repository(directory.path());
    StudioProject project = StudioProject::createDefault();
    project.profileName = QStringLiteral("Creator Profile");
    project.transition = {TransitionType::Fade, 850};
    Source &source = project.addSource(SourceType::Image);
    SceneItem &item = project.activeScene()->items.first();
    item.transform.x = 120.0;
    item.transform.width = 640.0;
    project.setSceneItemVisible(item.id, false);
    project.setSceneItemLocked(item.id, true);
    project.setMixerVolume(project.mixerChannels.first().id, 0.42);
    project.setMixerMuted(project.mixerChannels.last().id, true);
    QVERIFY(repository.save(project));

    const StudioProject restored = repository.load();
    QCOMPARE(restored.profileName, QStringLiteral("Creator Profile"));
    QCOMPARE(restored.transition.durationMs, 850);
    QCOMPARE(restored.sources.size(), 1);
    QCOMPARE(restored.sources.first().id, source.id);
    const SceneItem &restoredItem = restored.activeScene()->items.first();
    QCOMPARE(restoredItem.transform.x, 120.0);
    QCOMPARE(restoredItem.transform.width, 640.0);
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
    QCOMPARE(recovered.mixerChannels.size(), 2);
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

QTEST_APPLESS_MAIN(StudioProjectTests)

#include "StudioProjectTests.moc"
