#pragma once
#include "core/audio/AudioTypes.h"
#include <QMutex>
#include <QString>
#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <thread>

class WasapiAudioSource final {
public:
    WasapiAudioSource(QString endpointId, bool loopback, std::function<void()> blockReady = {});
    ~WasapiAudioSource();
    WasapiAudioSource(const WasapiAudioSource&) = delete;
    void start();
    void stop();
    AudioRuntime runtime() const;
    AudioBlock latestBlock() const;
    QVector<AudioBlock> takePendingBlocks();
    void discardPendingBlocks();
    QVariantMap diagnostics() const;
    void setMixControls(double gain, bool muted);
private:
    void run();
    QString endpointId_;
    bool loopback_ = false;
    std::function<void()> blockReady_;
    std::jthread thread_;
    mutable QMutex mutex_;
    AudioRuntime runtime_;
    AudioBlock block_;
    std::deque<AudioBlock> pendingBlocks_;
    quint64 capturedBlocks_ = 0;
    quint64 droppedPendingBlocks_ = 0;
    qint64 firstTimestampNs_ = 0;
    qint64 lastTimestampNs_ = 0;
    std::atomic_bool running_ = false;
    std::atomic<float> gain_ = 1.0f;
    std::atomic_bool muted_ = false;
};
