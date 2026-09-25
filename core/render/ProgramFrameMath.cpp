#include "ProgramFrameMath.h"

#include <QtGlobal>

namespace ProgramFrameMath {

Transform fitToCanvas(const QSize &sourcePixels, const QSize &programPixels)
{
    Transform transform;
    if (!sourcePixels.isValid() || !programPixels.isValid()) return transform;

    const double scale = qMin(programPixels.width() / static_cast<double>(sourcePixels.width()),
                              programPixels.height() / static_cast<double>(sourcePixels.height()));
    transform.width = sourcePixels.width() * scale;
    transform.height = sourcePixels.height() * scale;
    transform.x = (programPixels.width() - transform.width) / 2.0;
    transform.y = (programPixels.height() - transform.height) / 2.0;
    return transform;
}

float textureV(const bool bottomVertex, const bool importedDxgiTexture, const bool flipVertical)
{
    const bool topLeftRow = importedDxgiTexture ? !bottomVertex : bottomVertex;
    const bool useTopRow = flipVertical ? !topLeftRow : topLeftRow;
    return useTopRow ? 0.0f : 1.0f;
}

QRectF bestFitPreviewRect(const QSize &programPixels, const QSizeF &availablePixels)
{
    if (!programPixels.isValid() || availablePixels.width() <= 0.0 || availablePixels.height() <= 0.0) return {};
    const double scale = qMin(availablePixels.width() / programPixels.width(), availablePixels.height() / programPixels.height());
    const QSizeF size(programPixels.width() * scale, programPixels.height() * scale);
    return {{(availablePixels.width() - size.width()) / 2.0, (availablePixels.height() - size.height()) / 2.0}, size};
}

} // namespace ProgramFrameMath
