#pragma once

#include <QSize>
#include <QString>

struct ProgramResolution final {
    int width = 1920;
    int height = 1080;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] QSize size() const;
    [[nodiscard]] QString displayName() const;

    static ProgramResolution hd1080();
    static ProgramResolution qhd1440();
    static ProgramResolution uhd4k();
};

bool operator==(const ProgramResolution &left, const ProgramResolution &right);
bool operator!=(const ProgramResolution &left, const ProgramResolution &right);
