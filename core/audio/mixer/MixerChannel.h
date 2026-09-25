#pragma once

#include <QString>

struct MixerChannel final {
    QString id;
    QString name;
    double volume = 1.0;
    bool muted = false;
};
