#pragma once

#include <QString>

struct MixerChannel final {
    QString id;
    QString name;
    double volume = 0.8;
    bool muted = false;
};
