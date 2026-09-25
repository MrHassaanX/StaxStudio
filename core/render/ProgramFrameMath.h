#pragma once

#include "core/scene/Scene.h"

#include <QSize>
#include <QRectF>

namespace ProgramFrameMath {

// Program-space coordinates and the program texture always use a top-left
// origin. This is the only geometry contract shared by preview and recorder.
[[nodiscard]] Transform fitToCanvas(const QSize &sourcePixels, const QSize &programPixels);

// Returns a V coordinate for one compositor vertex. Imported DXGI textures are
// top-left row ordered; QRhi's imported-D3D sampling convention needs the
// inverse mapping from CPU-uploaded QImages.
[[nodiscard]] float textureV(bool bottomVertex, bool importedDxgiTexture, bool flipVertical);

[[nodiscard]] QRectF bestFitPreviewRect(const QSize &programPixels, const QSizeF &availablePixels);

} // namespace ProgramFrameMath
