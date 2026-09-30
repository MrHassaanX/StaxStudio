#pragma once

#include "StreamingTypes.h"
#include "core/audio/AudioTypes.h"
#include "core/recorder/RecordingTypes.h"

#include <QObject>
#include <QMutex>
#include <QSize>
#include <QTimer>
#include <QVariantMap>
#include <QWaitCondition>

#include <deque>
#include <thread>

// Streams the authoritative program output to Custom RTMP. The worker owns all
// FFmpeg state; UI/render/audio threads only enqueue bounded program frames.
class StreamingOutput final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString state READ stateName NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY stateChanged FINAL)
    Q_PROPERTY(qint64 elapsedMs READ elapsedMs NOTIFY elapsedChanged FINAL)
    Q_PROPERTY(QVariantMap diagnostics READ diagnostics NOTIFY diagnosticsChanged FINAL)
public:
    explicit StreamingOutput(QObject *parent = nullptr);
    ~StreamingOutput() override;

    bool start(const StreamSettings &settings, QSize programSize);
    void stop();
    void fail(const QString &message);
    void submitVideoFrame(RecordedVideoFrame frame);
    void submitProgramAudio(ProgramMixedAudioBlock block);

    QString stateName() const;
    QString errorMessage() const;
    StreamingState state() const;
    qint64 elapsedMs() const;
    QVariantMap diagnostics() const;
    bool isAcceptingInput() const;

signals:
    void stateChanged();
    void elapsedChanged();
    void diagnosticsChanged();

private:
    void run(StreamSettings settings, QSize size);
    bool runMock(StreamSettings settings);
    bool runFfmpeg(StreamSettings settings, QSize size, QString *failure);
    void setState(StreamingState state, QString error = {});
    void waitBeforeReconnect(int attempt);

    mutable QMutex mutex_;
    QWaitCondition wake_;
    StreamingState state_ = StreamingState::Idle;
    QString errorMessage_;
    StreamSettings settings_;
    QSize programSize_;
    bool stopRequested_ = false;
    qint64 liveStartedNs_ = 0;
    quint64 submittedVideoFrames_ = 0;
    quint64 submittedAudioBlocks_ = 0;
    quint64 encodedVideoFrames_ = 0;
    quint64 encodedAudioFrames_ = 0;
    quint64 droppedVideoFrames_ = 0;
    quint64 droppedAudioBlocks_ = 0;
    quint64 bytesSent_ = 0;
    quint64 writeFailures_ = 0;
    quint64 reconnectAttempts_ = 0;
    qint64 firstVideoTimestampNs_ = 0;
    qint64 lastVideoTimestampNs_ = 0;
    qint64 firstAudioTimestampNs_ = 0;
    qint64 lastAudioTimestampNs_ = 0;
    std::deque<RecordedVideoFrame> videoQueue_;
    std::deque<AudioBlock> audioQueue_;
    std::jthread workerThread_;
    QTimer elapsedTimer_;
};

