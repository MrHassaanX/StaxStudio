#pragma once

#include <QColor>
#include <QSize>

#include <cstdint>

enum class VisualFrameKind {
    SolidColor,
    NativeTexture
};

// Native texture handles remain opaque to the compositor core. Platform adapters own their lifetime.
struct VisualSourceFrame final {
    VisualFrameKind kind = VisualFrameKind::SolidColor;
    QColor color = QColor("#355B66");
    QSize pixelSize;
    std::uintptr_t nativeTextureHandle = 0;
};
