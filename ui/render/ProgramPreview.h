#pragma once

#include <QPointer>
#include <QQuickRhiItem>
#include <QTimer>

class ProgramRenderEngine;

// Optional presentation consumer of ProgramRenderEngine's program texture.
class ProgramPreview : public QQuickRhiItem {
    Q_OBJECT
    Q_PROPERTY(QObject *engine READ engine WRITE setEngine NOTIFY engineChanged FINAL)
    Q_PROPERTY(QString rendererState READ rendererState NOTIFY engineChanged FINAL)
public:
    explicit ProgramPreview(QQuickItem *parent = nullptr);
    QObject *engine() const;
    void setEngine(QObject *engine);
    QString rendererState() const;
signals:
    void engineChanged();
protected:
    QQuickRhiItemRenderer *createRenderer() override;
private:
    QPointer<ProgramRenderEngine> engine_;
    QTimer previewTimer_;
};
