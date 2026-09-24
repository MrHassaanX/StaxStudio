#pragma once

#include "RecordingTypes.h"
#include "core/audio/AudioTypes.h"

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <deque>
#include <memory>
#include <thread>

// FFmpeg owns all codec and muxer state on the worker thread. The UI only hands
// over completed program frames and normalized (48 kHz float) audio blocks.
class LocalRecorder final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString state READ stateName NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString outputPath READ outputPath NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY stateChanged FINAL)
public:
    explicit LocalRecorder(QObject *parent = nullptr);
    ~LocalRecorder() override;

    bool start(const RecordingSettings &settings, QSize programSize);
    void stop();
    void submitVideoFrame(RecordedVideoFrame frame);
    void submitAudioBlock(AudioBlock block);
    QString stateName() const;
    QString outputPath() const;
    QString errorMessage() const;
    RecordingState state() const;

signals:
    void stateChanged();

private:
    struct Worker;
    void setState(RecordingState state, QString error = {});
    void run(RecordingSettings settings, QSize size);

    mutable QMutex mutex_;
    QWaitCondition wake_;
    RecordingState state_ = RecordingState::Idle;
    QString outputPath_;
    QString errorMessage_;
    QSize programSize_;
    bool stopRequested_ = false;
    std::deque<RecordedVideoFrame> videoQueue_;
    std::deque<AudioBlock> audioQueue_;
    std::jthread workerThread_;
};
