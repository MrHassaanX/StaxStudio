#pragma once

#include "RecordingTypes.h"
#include "core/audio/AudioTypes.h"

#include <QObject>
#include <QMutex>
#include <QTimer>
#include <QVariantMap>
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
    Q_PROPERTY(qint64 elapsedMs READ elapsedMs NOTIFY elapsedChanged FINAL)
    Q_PROPERTY(int frameRate READ frameRate NOTIFY stateChanged FINAL)
    Q_PROPERTY(QVariantMap diagnostics READ diagnostics NOTIFY diagnosticsChanged FINAL)
public:
    explicit LocalRecorder(QObject *parent = nullptr);
    ~LocalRecorder() override;

    bool start(const RecordingSettings &settings, QSize programSize);
    void stop();
    void fail(const QString &message);
    void submitVideoFrame(RecordedVideoFrame frame);
    void submitAudioBlock(AudioBlock block);
    QString stateName() const;
    QString outputPath() const;
    QString errorMessage() const;
    RecordingState state() const;
    qint64 elapsedMs() const;
    int frameRate() const;
    QVariantMap diagnostics() const;
    void setProgramDiagnostics(QVariantMap values);

signals:
    void stateChanged();
    void elapsedChanged();
    void diagnosticsChanged();

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
    int frameRate_ = 60;
    bool stopRequested_ = false;
    qint64 recordingStartedNs_ = 0;
    quint64 submittedVideoFrames_ = 0;
    quint64 encodedVideoFrames_ = 0;
    qint64 lastEncodedPts_ = -1;
    QVariantMap programDiagnostics_;
    quint64 droppedVideoFrames_ = 0;
    quint64 submittedAudioBlocks_ = 0;
    quint64 droppedAudioBlocks_ = 0;
    quint64 encodedAudioFrames_ = 0;
    qint64 firstAudioTimestampNs_ = 0;
    qint64 lastAudioTimestampNs_ = 0;
    qint64 firstVideoTimestampNs_ = 0;
    qint64 lastVideoTimestampNs_ = 0;
    std::deque<RecordedVideoFrame> videoQueue_;
    std::deque<AudioBlock> audioQueue_;
    std::jthread workerThread_;
    QTimer elapsedTimer_;
};
