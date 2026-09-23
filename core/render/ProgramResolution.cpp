#include "ProgramResolution.h"

bool ProgramResolution::isValid() const
{
    return width > 0 && height > 0 && width <= 7680 && height <= 4320;
}

QSize ProgramResolution::size() const { return {width, height}; }
QString ProgramResolution::displayName() const { return QStringLiteral("%1 x %2").arg(width).arg(height); }
ProgramResolution ProgramResolution::hd1080() { return {1920, 1080}; }
ProgramResolution ProgramResolution::qhd1440() { return {2560, 1440}; }
ProgramResolution ProgramResolution::uhd4k() { return {3840, 2160}; }
bool operator==(const ProgramResolution &left, const ProgramResolution &right) { return left.width == right.width && left.height == right.height; }
bool operator!=(const ProgramResolution &left, const ProgramResolution &right) { return !(left == right); }
