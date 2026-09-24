#include "ProgramPreview.h"
#include "core/capture/windows/DxgiDesktopCapture.h"

#include <QColor>
#include <QFile>
#include <QImage>
#include <QMetaObject>
#include <QPointer>
#include <QVariantMap>
#include <QtMath>

#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

#include <algorithm>
#include <array>
#include <memory>

namespace {
struct Vertex final {
    float x;
    float y;
    float r;
    float g;
    float b;
    float a;
    float u;
    float v;
};

struct PreviewLayer final {
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
    QColor color;
    QString itemId;
    QImage image;
    qint64 timestampNs = 0;
    QString sourceType;
    QString targetId;
    QSize textureSize;
    QRect sourceRect;
};

struct TextureEntry final {
    QRhiTexture *texture = nullptr;
    QRhiShaderResourceBindings *bindings = nullptr;
    QSize size;
    qint64 timestampNs = -1;
    void *nativeTexture = nullptr;
};

QShader shaderFromResource(const char *path)
{
    QFile file(QString::fromLatin1(path));
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QShader::fromSerialized(file.readAll());
}

QColor gradientColor(const QColor &base, bool horizontal, bool vertical)
{
    const qreal factor = (horizontal ? 1.12 : 0.88) * (vertical ? 1.06 : 0.94);
    return QColor::fromRgbF(qBound(0.0, base.redF() * factor, 1.0), qBound(0.0, base.greenF() * factor, 1.0), qBound(0.0, base.blueF() * factor, 1.0), base.alphaF());
}

class ProgramPreviewRenderer final : public QQuickRhiItemRenderer
{
public:
    ~ProgramPreviewRenderer() override
    {
        delete pipeline_;
        for (TextureEntry &entry : textures_) { delete entry.bindings; delete entry.texture; }
        delete whiteBindings_;
        delete whiteTexture_;
        delete sampler_;
        delete vertexBuffer_;
    }

    void initialize(QRhiCommandBuffer *) override
    {
        if (pipeline_) return;

        const QShader vertexShader = shaderFromResource(
            ":/stax/shaders/core/render/shaders/compositor.vert.qsb");
        const QShader fragmentShader = shaderFromResource(
            ":/stax/shaders/core/render/shaders/compositor.frag.qsb");
        if (!vertexShader.isValid() || !fragmentShader.isValid()) {
            publishState(QStringLiteral("Renderer initialization failed: shader resources unavailable"));
            return;
        }

        sampler_ = rhi()->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                     QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
        if (!sampler_->create()) {
            publishState(QStringLiteral("Renderer initialization failed: texture sampler unavailable"));
            return;
        }
        whiteTexture_ = rhi()->newTexture(QRhiTexture::RGBA8, QSize(1, 1));
        if (!whiteTexture_->create()) { publishState(QStringLiteral("Renderer initialization failed: fallback texture unavailable")); return; }
        whiteBindings_ = rhi()->newShaderResourceBindings();
        whiteBindings_->setBindings({QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, whiteTexture_, sampler_)});
        if (!whiteBindings_->create()) { publishState(QStringLiteral("Renderer initialization failed: texture bindings unavailable")); return; }

        pipeline_ = rhi()->newGraphicsPipeline();
        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({QRhiVertexInputBinding(sizeof(Vertex))});
        inputLayout.setAttributes({
            QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float2, 0),
            QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float4, sizeof(float) * 2),
            QRhiVertexInputAttribute(0, 2, QRhiVertexInputAttribute::Float2, sizeof(float) * 6)
        });
        QRhiGraphicsPipeline::TargetBlend blend;
        blend.enable = true;
        blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
        blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        blend.srcAlpha = QRhiGraphicsPipeline::One;
        blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        pipeline_->setShaderStages({QRhiShaderStage(QRhiShaderStage::Vertex, vertexShader), QRhiShaderStage(QRhiShaderStage::Fragment, fragmentShader)});
        pipeline_->setVertexInputLayout(inputLayout);
        pipeline_->setShaderResourceBindings(whiteBindings_);
        pipeline_->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
        pipeline_->setTargetBlends({blend});
        if (!pipeline_->create()) {
            delete pipeline_;
            pipeline_ = nullptr;
            publishState(QStringLiteral("Renderer initialization failed: graphics pipeline unavailable"));
            return;
        }
        publishState(QStringLiteral("Ready"));
    }

    void synchronize(QQuickRhiItem *item) override
    {
        preview_ = static_cast<ProgramPreview *>(item);
        programWidth_ = qMax(1, preview_->programWidth());
        programHeight_ = qMax(1, preview_->programHeight());
        layers_.clear();
        for (const QVariant &entry : preview_->layers()) {
            const QVariantMap values = entry.toMap();
            PreviewLayer layer;
            layer.x = values.value(QStringLiteral("x")).toDouble();
            layer.y = values.value(QStringLiteral("y")).toDouble();
            layer.width = values.value(QStringLiteral("width"), 1920.0).toDouble();
            layer.height = values.value(QStringLiteral("height"), 1080.0).toDouble();
            layer.scaleX = values.value(QStringLiteral("scaleX"), 1.0).toDouble();
            layer.scaleY = values.value(QStringLiteral("scaleY"), 1.0).toDouble();
            layer.rotation = values.value(QStringLiteral("rotation")).toDouble();
            layer.cropLeft = values.value(QStringLiteral("cropLeft")).toDouble();
            layer.cropTop = values.value(QStringLiteral("cropTop")).toDouble();
            layer.cropRight = values.value(QStringLiteral("cropRight")).toDouble();
            layer.cropBottom = values.value(QStringLiteral("cropBottom")).toDouble();
            layer.flipHorizontal = values.value(QStringLiteral("flipHorizontal")).toBool();
            layer.flipVertical = values.value(QStringLiteral("flipVertical")).toBool();
            layer.color = values.value(QStringLiteral("color")).value<QColor>();
            layer.itemId = values.value(QStringLiteral("itemId")).toString();
            layer.image = values.value(QStringLiteral("image")).value<QImage>();
            layer.timestampNs = values.value(QStringLiteral("timestampNs")).toLongLong();
            layer.sourceType = values.value(QStringLiteral("sourceType")).toString();
            layer.targetId = values.value(QStringLiteral("targetId")).toString();
            if (layer.color.isValid()) layers_.append(layer);
        }
    }

    void render(QRhiCommandBuffer *commandBuffer) override
    {
        if (!pipeline_) return;
        QVector<Vertex> vertices;
        vertices.reserve(layers_.size() * 6);
        struct Draw final { int offset; int count; QRhiShaderResourceBindings *bindings; };
        QVector<Draw> draws;
        QRhiResourceUpdateBatch *updates = rhi()->nextResourceUpdateBatch();
        if (!whiteUploaded_) {
            QImage white(1, 1, QImage::Format_RGBA8888);
            white.fill(Qt::white);
            updates->uploadTexture(whiteTexture_, QRhiTextureUploadDescription(
                QRhiTextureUploadEntry(0, 0, QRhiTextureSubresourceUploadDescription(white))));
            whiteUploaded_ = true;
        }
        for (auto it = textures_.begin(); it != textures_.end();) {
            const bool stillActive = std::any_of(layers_.cbegin(), layers_.cend(), [&](const PreviewLayer &layer) { return layer.itemId == it.key(); });
            if (!stillActive) { desktopCapture_.remove(it.key()); delete it->bindings; delete it->texture; it = textures_.erase(it); } else ++it;
        }
        for (const PreviewLayer &layer : layers_) {
            PreviewLayer drawable = layer;
            QRhiShaderResourceBindings *bindings = whiteBindings_;
            if (!layer.image.isNull()) {
                bindings = textureBindings(layer, updates);
                drawable.color = Qt::white;
            } else if (isDesktopSource(layer)) {
                const DxgiDesktopCapture::Frame frame = desktopFrame(layer);
                if (frame.available) {
                    bindings = nativeTextureBindings(layer, frame.texture, frame.size);
                    drawable.color = Qt::white;
                    drawable.textureSize = frame.size;
                    drawable.sourceRect = frame.sourceRect;
                }
            } else if (isDesktopSource(layer)) {
                const DxgiDesktopCapture::Frame frame = desktopFrame(layer);
                if (frame.available) {
                    bindings = nativeTextureBindings(layer, frame.texture, frame.size);
                    drawable.color = Qt::white;
                    drawable.textureSize = frame.size;
                    drawable.sourceRect = frame.sourceRect;
                }
            }
            const int offset = vertices.size();
            appendLayer(vertices, drawable);
            draws.append({offset, 6, bindings ? bindings : whiteBindings_});
        }
        ensureVertexBuffer(vertices.size() * static_cast<int>(sizeof(Vertex)));
        if (!vertexBuffer_) return;
        if (!vertices.isEmpty()) updates->updateDynamicBuffer(vertexBuffer_, 0, vertices.size() * sizeof(Vertex), vertices.constData());
        commandBuffer->beginPass(renderTarget(), Qt::black, {1.0f, 0}, updates);
        if (!vertices.isEmpty()) {
            commandBuffer->setGraphicsPipeline(pipeline_);
            const QRhiCommandBuffer::VertexInput binding(vertexBuffer_, 0);
            commandBuffer->setVertexInput(0, 1, &binding);
            for (const Draw &draw : draws) { commandBuffer->setShaderResources(draw.bindings); commandBuffer->draw(draw.count, 1, draw.offset); }
        }
        commandBuffer->endPass();
    }

private:
    void publishState(const QString &state)
    {
        const QPointer<ProgramPreview> preview(preview_);
        if (!preview) return;
        QMetaObject::invokeMethod(preview, [preview, state] {
            if (preview) preview->publishRendererState(state);
        }, Qt::QueuedConnection);
    }

    void ensureVertexBuffer(int bytes)
    {
        if (bytes <= vertexBufferSize_) return;
        delete vertexBuffer_;
        vertexBufferSize_ = qMax(bytes, 6 * static_cast<int>(sizeof(Vertex)));
        vertexBuffer_ = rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, vertexBufferSize_);
        if (!vertexBuffer_->create()) {
            delete vertexBuffer_;
            vertexBuffer_ = nullptr;
            publishState(QStringLiteral("Renderer error: vertex buffer unavailable"));
        }
    }

    QRhiShaderResourceBindings *textureBindings(const PreviewLayer &layer, QRhiResourceUpdateBatch *updates)
    {
        TextureEntry &entry = textures_[layer.itemId];
        if (!entry.texture || entry.size != layer.image.size()) {
            delete entry.bindings;
            delete entry.texture;
            entry.texture = rhi()->newTexture(QRhiTexture::RGBA8, layer.image.size());
            if (!entry.texture->create()) { delete entry.texture; entry.texture = nullptr; return nullptr; }
            entry.bindings = rhi()->newShaderResourceBindings();
            entry.bindings->setBindings({QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, entry.texture, sampler_)});
            if (!entry.bindings->create()) { delete entry.bindings; entry.bindings = nullptr; delete entry.texture; entry.texture = nullptr; return nullptr; }
            entry.size = layer.image.size();
            entry.timestampNs = -1;
        }
        if (entry.timestampNs != layer.timestampNs) {
            updates->uploadTexture(entry.texture, QRhiTextureUploadDescription(
                QRhiTextureUploadEntry(0, 0, QRhiTextureSubresourceUploadDescription(layer.image))));
            entry.timestampNs = layer.timestampNs;
        }
        return entry.bindings;
    }

    static bool isDesktopSource(const PreviewLayer &layer)
    {
        return layer.sourceType == QStringLiteral("Display Capture") || layer.sourceType == QStringLiteral("Window Capture") || layer.sourceType == QStringLiteral("Game Capture");
    }

    DxgiDesktopCapture::Frame desktopFrame(const PreviewLayer &layer)
    {
        const auto *handles = static_cast<const QRhiD3D11NativeHandles *>(rhi()->nativeHandles());
        return handles ? desktopCapture_.acquire(handles->dev, handles->context, layer.itemId, layer.sourceType, layer.targetId) : DxgiDesktopCapture::Frame{};
    }

    QRhiShaderResourceBindings *nativeTextureBindings(const PreviewLayer &layer, void *texture, const QSize &size)
    {
        TextureEntry &entry = textures_[layer.itemId];
        if (!entry.texture || entry.nativeTexture != texture || entry.size != size) {
            delete entry.bindings; delete entry.texture;
            entry.texture = rhi()->newTexture(QRhiTexture::BGRA8, size);
            if (!entry.texture->createFrom({reinterpret_cast<quint64>(texture), 0})) { delete entry.texture; entry.texture = nullptr; return nullptr; }
            entry.bindings = rhi()->newShaderResourceBindings();
            entry.bindings->setBindings({QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, entry.texture, sampler_)});
            if (!entry.bindings->create()) { delete entry.bindings; entry.bindings = nullptr; delete entry.texture; entry.texture = nullptr; return nullptr; }
            entry.size = size; entry.nativeTexture = texture;
        }
        return entry.bindings;
    }
    void appendLayer(QVector<Vertex> &vertices, const PreviewLayer &layer) const
    {
        const double width = qMax(1.0, (layer.width - layer.cropLeft - layer.cropRight) * layer.scaleX);
        const double height = qMax(1.0, (layer.height - layer.cropTop - layer.cropBottom) * layer.scaleY);
        const double left = layer.x + layer.cropLeft * layer.scaleX;
        const double top = layer.y + layer.cropTop * layer.scaleY;
        const double centerX = left + width / 2.0;
        const double centerY = top + height / 2.0;
        const double radians = qDegreesToRadians(layer.rotation);
        const double cosine = qCos(radians);
        const double sine = qSin(radians);

        const auto vertex = [&](double x, double y, bool horizontal, bool vertical) {
            const double translatedX = x - centerX;
            const double translatedY = y - centerY;
            const double rotatedX = centerX + translatedX * cosine - translatedY * sine;
            const double rotatedY = centerY + translatedX * sine + translatedY * cosine;
            const QColor color = gradientColor(layer.color, layer.flipHorizontal ? !horizontal : horizontal, layer.flipVertical ? !vertical : vertical);
            const QRect source = layer.sourceRect.isEmpty() ? QRect(QPoint(0, 0), layer.textureSize) : layer.sourceRect;
            const float minU = layer.textureSize.isEmpty() ? 0.0f : static_cast<float>(source.left()) / layer.textureSize.width();
            const float maxU = layer.textureSize.isEmpty() ? 1.0f : static_cast<float>(source.right() + 1) / layer.textureSize.width();
            const float minV = layer.textureSize.isEmpty() ? 0.0f : static_cast<float>(source.top()) / layer.textureSize.height();
            const float maxV = layer.textureSize.isEmpty() ? 1.0f : static_cast<float>(source.bottom() + 1) / layer.textureSize.height();
            const float u = layer.flipHorizontal ? (horizontal ? minU : maxU) : (horizontal ? maxU : minU);
            const float v = layer.flipVertical ? (vertical ? maxV : minV) : (vertical ? minV : maxV);
            return Vertex{static_cast<float>(rotatedX / programWidth_ * 2.0 - 1.0), static_cast<float>(1.0 - rotatedY / programHeight_ * 2.0), static_cast<float>(color.redF()), static_cast<float>(color.greenF()), static_cast<float>(color.blueF()), static_cast<float>(color.alphaF()), u, v};
        };

        const std::array<Vertex, 4> corners{vertex(left, top, false, false), vertex(left + width, top, true, false), vertex(left + width, top + height, true, true), vertex(left, top + height, false, true)};
        for (const int index : {0, 1, 2, 0, 2, 3}) vertices.append(corners[index]);
    }

    ProgramPreview *preview_ = nullptr;
    QVector<PreviewLayer> layers_;
    int programWidth_ = 1920;
    int programHeight_ = 1080;
    QRhiBuffer *vertexBuffer_ = nullptr;
    QRhiTexture *whiteTexture_ = nullptr;
    QRhiShaderResourceBindings *whiteBindings_ = nullptr;
    QRhiSampler *sampler_ = nullptr;
    QHash<QString, TextureEntry> textures_;
    bool whiteUploaded_ = false;
    QRhiGraphicsPipeline *pipeline_ = nullptr;
    DxgiDesktopCapture desktopCapture_;
    int vertexBufferSize_ = 0;
};
}

ProgramPreview::ProgramPreview(QQuickItem *parent) : QQuickRhiItem(parent)
{
    setAlphaBlending(false);
    setFixedColorBufferWidth(programWidth_);
    setFixedColorBufferHeight(programHeight_);
}

QVariantList ProgramPreview::layers() const { return layers_; }
void ProgramPreview::setLayers(const QVariantList &layers)
{
    if (layers_ == layers) return;
    layers_ = layers;
    emit layersChanged();
    update();
}
int ProgramPreview::programWidth() const { return programWidth_; }
void ProgramPreview::setProgramWidth(int width)
{
    if (width <= 0 || programWidth_ == width) return;
    programWidth_ = width;
    setFixedColorBufferWidth(width);
    emit programSizeChanged();
    update();
}
int ProgramPreview::programHeight() const { return programHeight_; }
void ProgramPreview::setProgramHeight(int height)
{
    if (height <= 0 || programHeight_ == height) return;
    programHeight_ = height;
    setFixedColorBufferHeight(height);
    emit programSizeChanged();
    update();
}
QString ProgramPreview::rendererState() const { return rendererState_; }
void ProgramPreview::publishRendererState(const QString &state)
{
    if (rendererState_ == state) return;
    rendererState_ = state;
    emit rendererStateChanged();
}
QQuickRhiItemRenderer *ProgramPreview::createRenderer() { return new ProgramPreviewRenderer; }
