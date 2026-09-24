#pragma once
#include "core/audio/AudioTypes.h"
#include <QMutex>
#include <QString>
#include <atomic>
#include <memory>
#include <thread>

class WasapiAudioSource final {
public:
    WasapiAudioSource(QString endpointId, bool loopback);
    ~WasapiAudioSource();
    WasapiAudioSource(const WasapiAudioSource&) = delete;
    void start();
    void stop();
    AudioRuntime runtime() const;
    AudioBlock latestBlock() const;
    void setMixControls(double gain, bool muted);
private:
    void run();
    QString endpointId_;
    bool loopback_ = false;
    std::jthread thread_;
    mutable QMutex mutex_;
    AudioRuntime runtime_;
    AudioBlock block_;
    std::atomic_bool running_ = false;
    std::atomic<float> gain_ = 0.8f;
    std::atomic_bool muted_ = false;
};