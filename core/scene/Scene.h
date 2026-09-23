#pragma once

#include <QString>
#include <QVector>

struct Transform final {
    double x = 0.0;
    double y = 0.0;
    double width = 1920.0;
    double height = 1080.0;
    double scaleX = 1.0;
    double scaleY = 1.0;
    double rotation = 0.0;
    double cropLeft = 0.0;
    double cropTop = 0.0;
    double cropRight = 0.0;
    double cropBottom = 0.0;
    bool flipHorizontal = false;
    bool flipVertical = false;
};

struct SceneItem final {
    QString id;
    QString sourceId;
    Transform transform;
    bool visible = true;
    bool locked = false;
    int zOrder = 0;
};

struct Scene final {
    QString id;
    QString name;
    QVector<SceneItem> items;
};
