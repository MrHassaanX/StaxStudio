#include "CompositorScene.h"

#include "core/project/StudioProject.h"
#include "core/source/Source.h"

#include <algorithm>

namespace {
QColor placeholderColor(const Source &source)
{
    const QString configuredColor = source.configuration.value(QStringLiteral("color")).toString();
    const QColor configured(configuredColor);
    if (configured.isValid()) return configured;

    switch (source.type) {
    case SourceType::Color: return QColor("#496E78");
    case SourceType::DisplayCapture: return QColor("#335D76");
    case SourceType::WindowCapture: return QColor("#356B68");
    case SourceType::GameCapture: return QColor("#55446F");
    case SourceType::Webcam: return QColor("#775743");
    case SourceType::Image: return QColor("#73475A");
    case SourceType::Text: return QColor("#53527D");
    case SourceType::Microphone:
    case SourceType::DesktopAudio:
    case SourceType::Browser:
    case SourceType::Media:
        return QColor();
    }
    return QColor();
}
}

QVector<CompositorLayer> CompositorScene::activeLayers(const StudioProject &project)
{
    QVector<CompositorLayer> layers;
    const Scene *scene = project.activeScene();
    if (!scene) return layers;

    for (const SceneItem &item : scene->items) {
        const Source *source = project.source(item.sourceId);
        if (!source || !item.visible) continue;
        const QColor color = placeholderColor(*source);
        if (!color.isValid()) continue;
        layers.append({item.id, item.sourceId, item.transform, {VisualFrameKind::SolidColor, color, project.programResolution.size(), 0}, item.zOrder});
    }
    std::sort(layers.begin(), layers.end(), [](const CompositorLayer &left, const CompositorLayer &right) {
        return left.zOrder < right.zOrder;
    });
    return layers;
}
