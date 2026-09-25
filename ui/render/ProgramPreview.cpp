#include "ProgramPreview.h"
#include "core/render/ProgramRenderEngine.h"

#include <QFile>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

namespace {
struct Vertex { float x, y, r, g, b, a, u, v; };

QShader shader(const char *path)
{
    QFile file(QString::fromLatin1(path));
    return file.open(QIODevice::ReadOnly) ? QShader::fromSerialized(file.readAll()) : QShader{};
}

class PreviewRenderer final : public QQuickRhiItemRenderer {
public:
    ~PreviewRenderer() override
    {
        delete pipeline_;
        delete bindings_;
        delete texture_;
        delete sampler_;
        delete vertices_;
    }

    void initialize(QRhiCommandBuffer *) override
    {
        if (pipeline_) return;
        sampler_ = rhi()->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                     QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
        if (!sampler_->create()) return;
        vertices_ = rhi()->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, 6 * sizeof(Vertex));
        if (!vertices_->create()) return;
        createTexture(QSize(1920, 1080));
    }

    void synchronize(QQuickRhiItem *item) override
    {
        engine_ = qobject_cast<ProgramRenderEngine *>(static_cast<ProgramPreview *>(item)->engine());
    }

    void render(QRhiCommandBuffer *cb) override
    {
        if (!engine_) return;
        const QSize size = engine_->programSize();
        if (size != textureSize_) createTexture(size);
        if (!pipeline_ || !texture_) return;
        const auto *handles = static_cast<const QRhiD3D11NativeHandles *>(rhi()->nativeHandles());
        if (handles)
            engine_->copyLatestTo(handles->dev, handles->context,
                                  reinterpret_cast<void *>(texture_->nativeTexture().object));
        QRhiResourceUpdateBatch *updates = rhi()->nextResourceUpdateBatch();
        if (!verticesUploaded_) {
            const Vertex quad[6] = {
                {-1, 1, 1, 1, 1, 1, 0, 0}, {1, 1, 1, 1, 1, 1, 1, 0}, {1, -1, 1, 1, 1, 1, 1, 1},
                {-1, 1, 1, 1, 1, 1, 0, 0}, {1, -1, 1, 1, 1, 1, 1, 1}, {-1, -1, 1, 1, 1, 1, 0, 1}
            };
            updates->uploadStaticBuffer(vertices_, quad);
            verticesUploaded_ = true;
        }
        cb->beginPass(renderTarget(), Qt::black, {1.0f, 0}, updates);
        cb->setGraphicsPipeline(pipeline_);
        const QSize destination = renderTarget()->pixelSize();
        // A QQuickRhiItem pass must set its own viewport. Inherited Qt scene
        // graph state can be much larger than this item's offscreen target.
        cb->setViewport(QRhiViewport(0, 0, destination.width(), destination.height()));
        cb->setShaderResources(bindings_);
        const QRhiCommandBuffer::VertexInput input(vertices_, 0);
        cb->setVertexInput(0, 1, &input);
        cb->draw(6);
        cb->endPass();
    }

private:
    void createTexture(const QSize &size)
    {
        if (!size.isValid()) return;
        delete pipeline_; pipeline_ = nullptr;
        delete bindings_; bindings_ = nullptr;
        delete texture_; texture_ = nullptr;
        texture_ = rhi()->newTexture(QRhiTexture::BGRA8, size);
        if (!texture_->create()) return;
        textureSize_ = size;
        bindings_ = rhi()->newShaderResourceBindings();
        bindings_->setBindings({QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, texture_, sampler_)});
        if (!bindings_->create()) return;
        pipeline_ = rhi()->newGraphicsPipeline();
        QRhiVertexInputLayout layout;
        layout.setBindings({QRhiVertexInputBinding(sizeof(Vertex))});
        layout.setAttributes({QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float2, 0),
                              QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float4, sizeof(float) * 2),
                              QRhiVertexInputAttribute(0, 2, QRhiVertexInputAttribute::Float2, sizeof(float) * 6)});
        pipeline_->setShaderStages({QRhiShaderStage(QRhiShaderStage::Vertex, shader(":/stax/shaders/core/render/shaders/compositor.vert.qsb")),
                                    QRhiShaderStage(QRhiShaderStage::Fragment, shader(":/stax/shaders/core/render/shaders/compositor.frag.qsb"))});
        pipeline_->setVertexInputLayout(layout);
        pipeline_->setShaderResourceBindings(bindings_);
        pipeline_->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
        pipeline_->create();
    }

    ProgramRenderEngine *engine_ = nullptr;
    QRhiTexture *texture_ = nullptr;
    QRhiSampler *sampler_ = nullptr;
    QRhiBuffer *vertices_ = nullptr;
    QRhiShaderResourceBindings *bindings_ = nullptr;
    QRhiGraphicsPipeline *pipeline_ = nullptr;
    QSize textureSize_;
    bool verticesUploaded_ = false;
};
}

ProgramPreview::ProgramPreview(QQuickItem *parent) : QQuickRhiItem(parent)
{
    setAlphaBlending(false);
    previewTimer_.setInterval(33);
    connect(&previewTimer_, &QTimer::timeout, this, &ProgramPreview::update);
    previewTimer_.start();
}

QObject *ProgramPreview::engine() const { return engine_; }
void ProgramPreview::setEngine(QObject *engine)
{
    auto *value = qobject_cast<ProgramRenderEngine *>(engine);
    if (engine_ == value) return;
    engine_ = value;
    emit engineChanged();
    update();
}
QString ProgramPreview::rendererState() const { return engine_ ? QStringLiteral("Ready") : QStringLiteral("Program output unavailable"); }
QQuickRhiItemRenderer *ProgramPreview::createRenderer() { return new PreviewRenderer; }
