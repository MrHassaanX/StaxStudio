#pragma once

#include "core/render/VisualSourceFrame.h"
#include "core/scene/Scene.h"

#include <QString>
#include <QVector>

class StudioProject;

struct CompositorLayer final {
    QString sceneItemId;
    Transform transform;
    VisualSourceFrame frame;
    int zOrder = 0;
};

class CompositorScene final
{
public:
    [[nodiscard]] static QVector<CompositorLayer> activeLayers(const StudioProject &project);
};
