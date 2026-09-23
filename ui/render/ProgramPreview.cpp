#include "ProgramPreview.h"

#include <QColor>
#include <QFile>
#include <QMetaObject>
#include <QPointer>
#include <QVariantMap>
#include <QtMath>

#include <rhi/qrhi.h>

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
        delete bindings_;
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

        bindings_ = rhi()->newShaderResourceBindings();
        if (!bindings_->create()) {
            publishState(QStringLiteral("Renderer initialization failed: shader bindings unavailable"));
            return;
        }

        pipeline_ = rhi()->newGraphicsPipeline();
        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({QRhiVertexInputBinding(sizeof(Vertex))});
        inputLayout.setAttributes({
            QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float2, 0),
            QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float4, sizeof(float) * 2)
        });
        QRhiGraphicsPipeline::TargetBlend blend;
        blend.enable = true;
        blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
        blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        blend.srcAlpha = QRhiGraphicsPipeline::One;
        blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        pipeline_->setShaderStages({QRhiShaderStage(QRhiShaderStage::Vertex, vertexShader), QRhiShaderStage(QRhiShaderStage::Fragment, fragmentShader)});
        pipeline_->setVertexInputLayout(inputLayout);
        pipeline_->setShaderResourceBindings(bindings_);
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
            if (layer.color.isValid()) layers_.append(layer);
        }
    }

    void render(QRhiCommandBuffer *commandBuffer) override
    {
        if (!pipeline_) return;
        QVector<Vertex> vertices;
        vertices.reserve(layers_.size() * 6);
        for (const PreviewLayer &layer : layers_) appendLayer(vertices, layer);
        ensureVertexBuffer(vertices.size() * static_cast<int>(sizeof(Vertex)));
        if (!vertexBuffer_) return;

        QRhiResourceUpdateBatch *updates = rhi()->nextResourceUpdateBatch();
        if (!vertices.isEmpty()) updates->updateDynamicBuffer(vertexBuffer_, 0, vertices.size() * sizeof(Vertex), vertices.constData());
        commandBuffer->beginPass(renderTarget(), Qt::black, {1.0f, 0}, updates);
        if (!vertices.isEmpty()) {
            commandBuffer->setGraphicsPipeline(pipeline_);
            const QRhiCommandBuffer::VertexInput binding(vertexBuffer_, 0);
            commandBuffer->setVertexInput(0, 1, &binding);
            commandBuffer->draw(vertices.size());
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
            return Vertex{static_cast<float>(rotatedX / programWidth_ * 2.0 - 1.0), static_cast<float>(1.0 - rotatedY / programHeight_ * 2.0), static_cast<float>(color.redF()), static_cast<float>(color.greenF()), static_cast<float>(color.blueF()), static_cast<float>(color.alphaF())};
        };

        const std::array<Vertex, 4> corners{vertex(left, top, false, false), vertex(left + width, top, true, false), vertex(left + width, top + height, true, true), vertex(left, top + height, false, true)};
        for (const int index : {0, 1, 2, 0, 2, 3}) vertices.append(corners[index]);
    }

    ProgramPreview *preview_ = nullptr;
    QVector<PreviewLayer> layers_;
    int programWidth_ = 1920;
    int programHeight_ = 1080;
    QRhiBuffer *vertexBuffer_ = nullptr;
    QRhiShaderResourceBindings *bindings_ = nullptr;
    QRhiGraphicsPipeline *pipeline_ = nullptr;
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
