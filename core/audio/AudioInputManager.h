#pragma once
#include "core/audio/wasapi/WasapiAudioSource.h"
#include "core/source/Source.h"
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariantList>

class AudioInputManager final : public QObject {
    Q_OBJECT
public:
    explicit AudioInputManager(QObject *parent = nullptr);
    void synchronizeSources(const QVector<Source> &sources);
    QVariantList targets(SourceType type) const;
    AudioRuntime runtime(const QString &sourceId) const;
    float levelDb(const QString &sourceId) const;
    QHash<QString, AudioBlock> latestBlocks() const;
    void setMixControls(const QString &sourceId, double gain, bool muted);
signals:
    void metersChanged();
private:
    struct Entry { QString endpointId; std::shared_ptr<WasapiAudioSource> source; };
    QHash<QString, Entry> entries_;
    QTimer meterTimer_;
};
