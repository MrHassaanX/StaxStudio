#include "StudioRepository.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
QJsonObject transformToJson(const Transform &value)
{
    return {{"x", value.x}, {"y", value.y}, {"width", value.width}, {"height", value.height},
            {"scaleX", value.scaleX}, {"scaleY", value.scaleY}, {"rotation", value.rotation},
            {"cropLeft", value.cropLeft}, {"cropTop", value.cropTop}, {"cropRight", value.cropRight},
            {"cropBottom", value.cropBottom}};
}

Transform transformFromJson(const QJsonObject &json)
{
    Transform value;
    value.x = json.value("x").toDouble(); value.y = json.value("y").toDouble();
    value.width = json.value("width").toDouble(1920.0); value.height = json.value("height").toDouble(1080.0);
    value.scaleX = json.value("scaleX").toDouble(1.0); value.scaleY = json.value("scaleY").toDouble(1.0);
    value.rotation = json.value("rotation").toDouble(); value.cropLeft = json.value("cropLeft").toDouble();
    value.cropTop = json.value("cropTop").toDouble(); value.cropRight = json.value("cropRight").toDouble();
    value.cropBottom = json.value("cropBottom").toDouble();
    return value;
}
}

StudioRepository::StudioRepository(QString storageDirectory)
    : storageDirectory_(storageDirectory.isEmpty()
            ? QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
            : std::move(storageDirectory))
{
}

QString StudioRepository::filePath() const { return QDir(storageDirectory_).filePath(QStringLiteral("studio.json")); }

StudioProject StudioRepository::load() const
{
    QFile file(filePath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) return StudioProject::createDefault();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) return StudioProject::createDefault();
    const QJsonObject root = document.object();
    if (root.value("schemaVersion").toInt() != StudioProject::SchemaVersion) return StudioProject::createDefault();

    StudioProject project;
    project.profileId = root.value("profileId").toString();
    project.profileName = root.value("profileName").toString();
    project.activeSceneId = root.value("activeSceneId").toString();
    for (const QJsonValue &value : root.value("sources").toArray()) {
        const QJsonObject item = value.toObject(); bool typeOk = false;
        const SourceType type = sourceTypeFromName(item.value("type").toString(), &typeOk);
        if (!typeOk || item.value("id").toString().isEmpty()) continue;
        project.sources.append({item.value("id").toString(), item.value("name").toString(), type,
                                item.value("enabled").toBool(true), item.value("configuration").toObject()});
    }
    for (const QJsonValue &value : root.value("scenes").toArray()) {
        const QJsonObject sceneJson = value.toObject();
        if (sceneJson.value("id").toString().isEmpty()) continue;
        Scene scene{sceneJson.value("id").toString(), sceneJson.value("name").toString(), {}};
        for (const QJsonValue &itemValue : sceneJson.value("items").toArray()) {
            const QJsonObject itemJson = itemValue.toObject();
            if (itemJson.value("id").toString().isEmpty() || project.sourceIndex(itemJson.value("sourceId").toString()) < 0) continue;
            scene.items.append({itemJson.value("id").toString(), itemJson.value("sourceId").toString(),
                                transformFromJson(itemJson.value("transform").toObject()), itemJson.value("visible").toBool(true),
                                itemJson.value("locked").toBool(false), itemJson.value("zOrder").toInt()});
        }
        project.scenes.append(scene);
    }
    for (const QJsonValue &value : root.value("mixerChannels").toArray()) {
        const QJsonObject item = value.toObject();
        if (!item.value("id").toString().isEmpty()) project.mixerChannels.append({item.value("id").toString(), item.value("name").toString(), item.value("volume").toDouble(0.8), item.value("muted").toBool(false)});
    }
    const QJsonObject transition = root.value("transition").toObject();
    project.transition.type = transition.value("type").toString() == "Cut" ? TransitionType::Cut : TransitionType::Fade;
    project.transition.durationMs = qBound(0, transition.value("durationMs").toInt(300), 10000);
    if (project.profileId.isEmpty() || project.profileName.trimmed().isEmpty() || project.mixerChannels.isEmpty()) return StudioProject::createDefault();
    project.normalize();
    return project;
}

bool StudioRepository::save(const StudioProject &project, QString *errorMessage) const
{
    if (!QDir().mkpath(storageDirectory_)) { if (errorMessage) *errorMessage = QStringLiteral("Unable to create configuration folder."); return false; }
    QJsonArray sources;
    for (const Source &source : project.sources) {
        sources.append(QJsonObject{{"id", source.id}, {"name", source.name}, {"type", sourceTypeName(source.type)}, {"enabled", source.enabled}, {"configuration", source.configuration}});
    }
    QJsonArray scenes;
    for (const Scene &scene : project.scenes) {
        QJsonArray items;
        for (const SceneItem &item : scene.items) {
            items.append(QJsonObject{{"id", item.id}, {"sourceId", item.sourceId}, {"transform", transformToJson(item.transform)}, {"visible", item.visible}, {"locked", item.locked}, {"zOrder", item.zOrder}});
        }
        scenes.append(QJsonObject{{"id", scene.id}, {"name", scene.name}, {"items", items}});
    }
    QJsonArray mixer;
    for (const MixerChannel &channel : project.mixerChannels) {
        mixer.append(QJsonObject{{"id", channel.id}, {"name", channel.name}, {"volume", channel.volume}, {"muted", channel.muted}});
    }
    const QJsonObject root{{"schemaVersion", StudioProject::SchemaVersion}, {"profileId", project.profileId}, {"profileName", project.profileName}, {"activeSceneId", project.activeSceneId}, {"sources", sources}, {"scenes", scenes}, {"mixerChannels", mixer}, {"transition", QJsonObject{{"type", project.transition.type == TransitionType::Cut ? "Cut" : "Fade"}, {"durationMs", project.transition.durationMs}}}};
    QSaveFile file(filePath());
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0 || !file.commit()) { if (errorMessage) *errorMessage = file.errorString(); return false; }
    return true;
}
