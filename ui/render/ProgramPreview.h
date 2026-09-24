#pragma once

#include <QQuickRhiItem>
#include <QPointer>
#include <QTimer>
#include <QVariantList>

class LocalRecorder;

class ProgramPreview : public QQuickRhiItem
{
    Q_OBJECT
    Q_PROPERTY(QVariantList layers READ layers WRITE setLayers NOTIFY layersChanged FINAL)
    Q_PROPERTY(int programWidth READ programWidth WRITE setProgramWidth NOTIFY programSizeChanged FINAL)
    Q_PROPERTY(int programHeight READ programHeight WRITE setProgramHeight NOTIFY programSizeChanged FINAL)
    Q_PROPERTY(QString rendererState READ rendererState NOTIFY rendererStateChanged FINAL)
    Q_PROPERTY(QObject *recorder READ recorder WRITE setRecorder NOTIFY recorderChanged FINAL)
    Q_PROPERTY(bool recordingActive READ recordingActive WRITE setRecordingActive NOTIFY recordingActiveChanged FINAL)
public:
    explicit ProgramPreview(QQuickItem *parent = nullptr);

    QVariantList layers() const;
    void setLayers(const QVariantList &layers);
    int programWidth() const;
    void setProgramWidth(int width);
    int programHeight() const;
    void setProgramHeight(int height);
    QString rendererState() const;
    QObject *recorder() const;
    void setRecorder(QObject *recorder);
    bool recordingActive() const;
    void setRecordingActive(bool active);
    void publishRendererState(const QString &state);

signals:
    void layersChanged();
    void programSizeChanged();
    void rendererStateChanged();
    void recorderChanged();
    void recordingActiveChanged();

protected:
    QQuickRhiItemRenderer *createRenderer() override;

private:
    QVariantList layers_;
    int programWidth_ = 1920;
    int programHeight_ = 1080;
    QString rendererState_ = QStringLiteral("Initializing renderer");
    QPointer<LocalRecorder> recorder_;
    bool recordingActive_ = false;
    QTimer recordingTimer_;
};
