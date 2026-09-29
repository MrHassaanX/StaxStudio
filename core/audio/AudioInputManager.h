#pragma once
#include "core/audio/wasapi/WasapiAudioSource.h"
#include "core/source/Source.h"
#include "ProgramAudioMixer.h"
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QWaitCondition>

#include <functional>
#include <atomic>
#include <thread>

class AudioInputManager final : public QObject {
    Q_OBJECT
public:
    explicit AudioInputManager(QObject *parent = nullptr);
    ~AudioInputManager() override;
    void synchronizeSources(const QVector<Source> &sources);
    QVariantList targets(SourceType type) const;
    AudioRuntime runtime(const QString &sourceId) const;
    float levelDb(const QString &sourceId) const;
    QHash<QString, AudioBlock> latestBlocks() const;
    QHash<QString, QVector<AudioBlock>> takePendingBlocks();
    void discardPendingBlocks();
    QVariantMap diagnostics() const;
    void setMixControls(const QString &sourceId, double gain, bool muted);
    void setRecordingSink(std::function<void(ProgramMixedAudioBlock)> sink);
    void flushRecordingSink();
signals:
    void metersChanged();
private:
    struct Entry { QString endpointId; std::shared_ptr<WasapiAudioSource> source; };
    QVector<Entry> entriesSnapshot() const;
    void wakeMixer();
    void mixerLoop();
    void drainToSink(bool flush = false);
    mutable QMutex processingMutex_;
    ProgramAudioMixer programMixer_;
    mutable QMutex entriesMutex_;
    QHash<QString, Entry> entries_;
    QTimer meterTimer_;
    QMutex mixerMutex_;
    QWaitCondition mixerWake_;
    std::function<void(ProgramMixedAudioBlock)> recordingSink_;
    bool mixerDirty_ = false;
    std::jthread mixerThread_;
    std::atomic_bool mixerRunning_ = true;
};
