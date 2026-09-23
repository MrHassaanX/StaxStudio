#pragma once

#include <QQuickRhiItem>
#include <QVariantList>

class ProgramPreview : public QQuickRhiItem
{
    Q_OBJECT
    Q_PROPERTY(QVariantList layers READ layers WRITE setLayers NOTIFY layersChanged FINAL)
    Q_PROPERTY(int programWidth READ programWidth WRITE setProgramWidth NOTIFY programSizeChanged FINAL)
    Q_PROPERTY(int programHeight READ programHeight WRITE setProgramHeight NOTIFY programSizeChanged FINAL)
    Q_PROPERTY(QString rendererState READ rendererState NOTIFY rendererStateChanged FINAL)
public:
    explicit ProgramPreview(QQuickItem *parent = nullptr);

    QVariantList layers() const;
    void setLayers(const QVariantList &layers);
    int programWidth() const;
    void setProgramWidth(int width);
    int programHeight() const;
    void setProgramHeight(int height);
    QString rendererState() const;
    void publishRendererState(const QString &state);

signals:
    void layersChanged();
    void programSizeChanged();
    void rendererStateChanged();

protected:
    QQuickRhiItemRenderer *createRenderer() override;

private:
    QVariantList layers_;
    int programWidth_ = 1920;
    int programHeight_ = 1080;
    QString rendererState_ = QStringLiteral("Initializing renderer");
};
