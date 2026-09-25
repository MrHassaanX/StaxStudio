#include "LocalRecorder.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QJsonDocument>
#include <QScopeGuard>
#include <QStandardPaths>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

namespace {
constexpr int kSampleRate = 48000;
constexpr int kOutputChannels = 2;

QString ffmpegError(const int error)
{
    char text[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(error, text, sizeof(text));
    return QString::fromUtf8(text);
}

QString defaultDirectory()
{
    const QString videos = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    return QDir(videos.isEmpty() ? QDir::homePath() : videos).filePath(QStringLiteral("StaxStudio"));
}

QString outputName(const QString &directory)
{
    QDir dir(directory);
    const QString stem = QStringLiteral("StaxStudio-%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss"));
    QString candidate = dir.filePath(stem + QStringLiteral(".mkv"));
    for (int suffix = 2; QFileInfo::exists(candidate); ++suffix)
        candidate = dir.filePath(QStringLiteral("%1-%2.mkv").arg(stem).arg(suffix));
    return candidate;
}

int drain(AVCodecContext *context, AVFormatContext *format, AVStream *stream, qint64 *lastPts=nullptr)
{
    AVPacket *packet = av_packet_alloc();
    if(!packet) return AVERROR(ENOMEM);
    int count=0,result=0;
    while ((result=avcodec_receive_packet(context, packet)) == 0) {
        const qint64 pts=packet->pts;
        av_packet_rescale_ts(packet, context->time_base, stream->time_base);
        packet->stream_index = stream->index;
        result=av_interleaved_write_frame(format, packet);
        av_packet_unref(packet);
        if(result<0) break;
        ++count;
        if(lastPts) *lastPts=pts;
    }
    av_packet_free(&packet);
    return result==AVERROR(EAGAIN) || result==AVERROR_EOF ? count : result;
}
}

LocalRecorder::LocalRecorder(QObject *parent) : QObject(parent)
{
    elapsedTimer_.setInterval(250);
    connect(&elapsedTimer_, &QTimer::timeout, this, &LocalRecorder::elapsedChanged);
}
LocalRecorder::~LocalRecorder() { stop(); }

bool LocalRecorder::start(const RecordingSettings &settings, const QSize size)
{
    if (!size.isValid()) return false;
    {
        QMutexLocker lock(&mutex_);
        if (state_ != RecordingState::Idle && state_ != RecordingState::Error) return false;
        stopRequested_ = false;
        videoQueue_.clear();
        audioQueue_.clear();
        recordingStartedNs_ = 0;
        encodedVideoFrames_ = 0; lastEncodedPts_ = -1;
        submittedVideoFrames_ = droppedVideoFrames_ = submittedAudioBlocks_ = droppedAudioBlocks_ = encodedAudioFrames_ = 0;
        firstAudioTimestampNs_ = lastAudioTimestampNs_ = firstVideoTimestampNs_ = lastVideoTimestampNs_ = 0;
        errorMessage_.clear();
        programSize_ = size;
        frameRate_ = qBound(1, settings.frameRate, 60);
        state_ = RecordingState::Starting;
    }
    emit stateChanged();
    emit elapsedChanged();
    workerThread_ = std::jthread([this, settings, size] { run(settings, size); });
    return true;
}

void LocalRecorder::stop()
{
    {
        QMutexLocker lock(&mutex_);
        if (state_ == RecordingState::Idle) return;
        if (state_ != RecordingState::Error) state_ = RecordingState::Stopping;
        stopRequested_ = true;
        wake_.wakeAll();
    }
    emit stateChanged();
    if (workerThread_.joinable()) workerThread_.join();
    qInfo().noquote() << QStringLiteral("Recorder stop diagnostics: %1").arg(QString::fromUtf8(QJsonDocument::fromVariant(diagnostics()).toJson(QJsonDocument::Compact)));
    QFile log(outputPath()+QStringLiteral(".diagnostics.jsonl"));
    if(log.open(QIODevice::WriteOnly|QIODevice::Append)) {
        auto summary=diagnostics(); summary.insert("event","stop");
        log.write(QJsonDocument::fromVariant(summary).toJson(QJsonDocument::Compact)); log.write("\n");
    }
}

void LocalRecorder::setProgramDiagnostics(QVariantMap values)
{
    QMutexLocker lock(&mutex_); programDiagnostics_=std::move(values);
}

void LocalRecorder::fail(const QString &message)
{
    { QMutexLocker lock(&mutex_); stopRequested_ = true; wake_.wakeAll(); }
    setState(RecordingState::Error, message);
}

void LocalRecorder::submitVideoFrame(RecordedVideoFrame frame)
{
    if (frame.image.isNull()) return;
    QMutexLocker lock(&mutex_);
    if (state_ != RecordingState::Recording) return;
    // Software encoding cannot block the render thread: retain only a tiny live queue.
    ++submittedVideoFrames_;
    if (submittedVideoFrames_ == 1) firstVideoTimestampNs_ = frame.timestampNs;
    lastVideoTimestampNs_ = frame.timestampNs;
    if (videoQueue_.size() >= 3) { videoQueue_.pop_front(); ++droppedVideoFrames_; }
    videoQueue_.push_back(std::move(frame));
    wake_.wakeOne();
}

void LocalRecorder::submitAudioBlock(AudioBlock block)
{
    if (block.samples.isEmpty()) return;
    QMutexLocker lock(&mutex_);
    if (state_ != RecordingState::Recording) return;
    ++submittedAudioBlocks_;
    if (submittedAudioBlocks_ == 1) firstAudioTimestampNs_ = block.timestampNs;
    lastAudioTimestampNs_ = block.timestampNs;
    // Audio blocks are tiny compared with video. Keep several seconds so a
    // temporary encoder stall cannot truncate the recording's audio tail.
    if (audioQueue_.size() >= 512) { audioQueue_.pop_front(); ++droppedAudioBlocks_; }
    audioQueue_.push_back(std::move(block));
    wake_.wakeOne();
}

QString LocalRecorder::stateName() const { QMutexLocker lock(&mutex_); return recordingStateName(state_); }
QString LocalRecorder::outputPath() const { QMutexLocker lock(&mutex_); return outputPath_; }
QString LocalRecorder::errorMessage() const { QMutexLocker lock(&mutex_); return errorMessage_; }
RecordingState LocalRecorder::state() const { QMutexLocker lock(&mutex_); return state_; }
qint64 LocalRecorder::elapsedMs() const
{
    QMutexLocker lock(&mutex_);
    return recordingStartedNs_ > 0 && (state_ == RecordingState::Recording || state_ == RecordingState::Stopping)
        ? qMax<qint64>(0, (mediaTimestampNs() - recordingStartedNs_) / 1'000'000) : 0;
}
int LocalRecorder::frameRate() const { QMutexLocker lock(&mutex_); return frameRate_; }
QVariantMap LocalRecorder::diagnostics() const
{
    QMutexLocker lock(&mutex_);
    return {{"receivedVideoFrames", submittedVideoFrames_}, {"queuedVideoFrames", static_cast<qulonglong>(videoQueue_.size())},
            {"encodedVideoFrames",encodedVideoFrames_},{"lastEncodedPts",lastEncodedPts_},
            {"program",programDiagnostics_},
            {"droppedVideoFrames", droppedVideoFrames_},
            {"receivedAudioBlocks", submittedAudioBlocks_}, {"queuedAudioBlocks", static_cast<qulonglong>(audioQueue_.size())},
            {"droppedAudioBlocks", droppedAudioBlocks_}, {"encodedAacFrames", encodedAudioFrames_},
            {"firstAudioTimestampNs", firstAudioTimestampNs_}, {"lastAudioTimestampNs", lastAudioTimestampNs_},
            {"firstVideoTimestampNs", firstVideoTimestampNs_}, {"lastVideoTimestampNs", lastVideoTimestampNs_}};
}

void LocalRecorder::setState(const RecordingState state, QString error)
{
    { QMutexLocker lock(&mutex_); state_ = state; errorMessage_ = std::move(error); if (state == RecordingState::Idle) recordingStartedNs_ = 0; }
    QMetaObject::invokeMethod(this, [this, state] {
        if (state == RecordingState::Idle) { elapsedTimer_.stop(); emit elapsedChanged(); }
        emit diagnosticsChanged();
        emit stateChanged();
    }, Qt::QueuedConnection);
}

void LocalRecorder::run(RecordingSettings settings, const QSize size)
{
    const QString directory = settings.outputDirectory.isEmpty() ? defaultDirectory() : settings.outputDirectory;
    if (!QDir().mkpath(directory)) { setState(RecordingState::Error, QStringLiteral("Unable to create recording folder.")); return; }
    const QString path = outputName(directory);
    AVFormatContext *format = nullptr;
    AVCodecContext *video = nullptr;
    AVCodecContext *audio = nullptr;
    SwsContext *scaler = nullptr;
    AVFrame *videoFrame = nullptr;
    AVFrame *audioFrame = nullptr;
    AVAudioFifo *audioFifo = nullptr;
    bool headerWritten = false;
    auto cleanup = qScopeGuard([&] {
        if (video && headerWritten) { avcodec_send_frame(video, nullptr); drain(video, format, format->streams[0]); }
        if (audio && headerWritten) { avcodec_send_frame(audio, nullptr); drain(audio, format, format->streams[1]); }
        if (format && headerWritten) av_write_trailer(format);
        if (format && !(format->oformat->flags & AVFMT_NOFILE) && format->pb) avio_closep(&format->pb);
        av_audio_fifo_free(audioFifo); av_frame_free(&audioFrame); av_frame_free(&videoFrame);
        sws_freeContext(scaler); avcodec_free_context(&audio); avcodec_free_context(&video); avformat_free_context(format);
    });

    int result = avformat_alloc_output_context2(&format, nullptr, "matroska", path.toUtf8().constData());
    if (result < 0 || !format) { setState(RecordingState::Error, QStringLiteral("Unable to create MKV output: %1").arg(ffmpegError(result))); return; }
    const AVCodec *videoCodec = avcodec_find_encoder_by_name("libx264");
    if (!videoCodec) videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!videoCodec) { setState(RecordingState::Error, QStringLiteral("No H.264 encoder is available in FFmpeg.")); return; }
    AVStream *videoStream = avformat_new_stream(format, nullptr);
    video = avcodec_alloc_context3(videoCodec);
    video->codec_id = videoCodec->id; video->width = size.width() & ~1; video->height = size.height() & ~1;
    video->pix_fmt = AV_PIX_FMT_YUV420P; video->time_base = AVRational{1, qBound(1, settings.frameRate, 60)};
    video->framerate = AVRational{qBound(1, settings.frameRate, 60), 1}; video->bit_rate = 8'000'000;
    if (format->oformat->flags & AVFMT_GLOBALHEADER) video->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    av_opt_set(video->priv_data, "preset", "veryfast", 0);
    av_opt_set(video->priv_data, "crf", "20", 0);
    av_opt_set(video->priv_data, "tune", "zerolatency", 0);
    av_opt_set(video->priv_data, "bf", "0", 0);
    if ((result = avcodec_open2(video, videoCodec, nullptr)) < 0) { setState(RecordingState::Error, QStringLiteral("H.264 initialization failed: %1").arg(ffmpegError(result))); return; }
    avcodec_parameters_from_context(videoStream->codecpar, video);
    videoStream->time_base = video->time_base;
    videoFrame = av_frame_alloc(); videoFrame->format = video->pix_fmt; videoFrame->width = video->width; videoFrame->height = video->height;
    if (av_frame_get_buffer(videoFrame, 32) < 0) { setState(RecordingState::Error, QStringLiteral("Unable to allocate video frame.")); return; }
    scaler = sws_getContext(video->width, video->height, AV_PIX_FMT_BGRA, video->width, video->height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!scaler) { setState(RecordingState::Error, QStringLiteral("Unable to initialize video conversion.")); return; }

    const AVCodec *audioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    AVStream *audioStream = nullptr;
    if (audioCodec) {
        audioStream = avformat_new_stream(format, nullptr);
        audio = avcodec_alloc_context3(audioCodec); audio->sample_rate = kSampleRate; audio->sample_fmt = AV_SAMPLE_FMT_FLTP;
        av_channel_layout_default(&audio->ch_layout, kOutputChannels); audio->time_base = AVRational{1, kSampleRate}; audio->bit_rate = 192000;
        if (format->oformat->flags & AVFMT_GLOBALHEADER) audio->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        if ((result = avcodec_open2(audio, audioCodec, nullptr)) >= 0) {
            avcodec_parameters_from_context(audioStream->codecpar, audio); audioStream->time_base = audio->time_base;
            audioFrame = av_frame_alloc(); audioFrame->format = audio->sample_fmt; audioFrame->sample_rate = audio->sample_rate; av_channel_layout_copy(&audioFrame->ch_layout, &audio->ch_layout); audioFrame->nb_samples = audio->frame_size;
            if (av_frame_get_buffer(audioFrame, 0) < 0) { setState(RecordingState::Error, QStringLiteral("Unable to allocate audio frame.")); return; }
            audioFifo = av_audio_fifo_alloc(audio->sample_fmt, kOutputChannels, audio->frame_size * 4);
        } else { avcodec_free_context(&audio); audioStream = nullptr; }
    }
    if (!(format->oformat->flags & AVFMT_NOFILE) && (result = avio_open(&format->pb, path.toUtf8().constData(), AVIO_FLAG_WRITE)) < 0) { setState(RecordingState::Error, QStringLiteral("Unable to create recording file: %1").arg(ffmpegError(result))); return; }
    if ((result = avformat_write_header(format, nullptr)) < 0) { setState(RecordingState::Error, QStringLiteral("Unable to write MKV header: %1").arg(ffmpegError(result))); return; }
    headerWritten = true;
    {
        QMutexLocker lock(&mutex_);
        outputPath_ = path;
        state_ = RecordingState::Recording;
        recordingStartedNs_ = mediaTimestampNs();
    }
    QMetaObject::invokeMethod(this, [this] { elapsedTimer_.start(); emit elapsedChanged(); emit stateChanged(); }, Qt::QueuedConnection);

    qint64 originNs = -1, nextVideoPts = 0, nextAudioPts = 0;
    const auto encodeAudioBlock = [&](const AudioBlock &audioBlock) {
        if (!audio || audioBlock.samples.isEmpty()) return;
        const int inFrames = audioBlock.samples.size() / qMax(1, audioBlock.channelCount);
        if (inFrames <= 0) return;
        QVector<float> left(inFrames), right(inFrames);
        for (int frame = 0; frame < inFrames; ++frame) {
            left[frame] = audioBlock.samples[frame * audioBlock.channelCount];
            right[frame] = audioBlock.samples[frame * audioBlock.channelCount + qMin(1, audioBlock.channelCount - 1)];
        }
        void *data[] = {left.data(), right.data()};
        if (av_audio_fifo_realloc(audioFifo, av_audio_fifo_size(audioFifo) + inFrames) < 0) return;
        av_audio_fifo_write(audioFifo, data, inFrames);
        while (av_audio_fifo_size(audioFifo) >= audio->frame_size) {
            av_frame_make_writable(audioFrame);
            av_audio_fifo_read(audioFifo, reinterpret_cast<void **>(audioFrame->data), audio->frame_size);
            audioFrame->pts = nextAudioPts; nextAudioPts += audio->frame_size;
            if (avcodec_send_frame(audio, audioFrame) >= 0) {
                const int packets=drain(audio, format, audioStream);
                if(packets<0) { fail(QStringLiteral("AAC/MKV write failed: %1").arg(ffmpegError(packets))); return; }
                QMutexLocker lock(&mutex_); ++encodedAudioFrames_;
            } else { fail(QStringLiteral("AAC encoding failed")); return; }
        }
    };
    while (true) {
        RecordedVideoFrame image;
        std::deque<AudioBlock> audioBlocks;
        {
            QMutexLocker lock(&mutex_);
            if (videoQueue_.empty() && audioQueue_.empty() && !stopRequested_) wake_.wait(&mutex_, 100);
            if (!videoQueue_.empty()) { image = std::move(videoQueue_.front()); videoQueue_.pop_front(); }
            audioBlocks.swap(audioQueue_);
            if (stopRequested_ && image.image.isNull() && audioBlocks.empty()) break;
        }
        if (!image.image.isNull()) {
            if (originNs < 0) originNs = image.timestampNs;
            const qint64 desiredPts = qMax(nextVideoPts, av_rescale_q(qMax<qint64>(0, image.timestampNs - originNs), AVRational{1, 1000000000}, video->time_base));
            QImage bgra = image.image.convertToFormat(QImage::Format_ARGB32);
            if (bgra.size() != QSize(video->width, video->height)) bgra = bgra.scaled(video->width, video->height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            av_frame_make_writable(videoFrame);
            const uint8_t *planes[] = {bgra.constBits(), nullptr, nullptr, nullptr}; const int strides[] = {static_cast<int>(bgra.bytesPerLine()), 0, 0, 0};
            sws_scale(scaler, planes, strides, 0, video->height, videoFrame->data, videoFrame->linesize);
            videoFrame->pts = desiredPts; nextVideoPts = desiredPts + 1;
            if (avcodec_send_frame(video, videoFrame) >= 0) {
                qint64 encodedPts=-1;
                const int packets=drain(video, format, videoStream,&encodedPts);
                if(packets<0) { fail(QStringLiteral("H.264/MKV write failed: %1").arg(ffmpegError(packets))); break; }
                QMutexLocker lock(&mutex_); encodedVideoFrames_+=packets; if(encodedPts>=0) lastEncodedPts_=encodedPts;
            } else { fail(QStringLiteral("H.264 encoding failed")); break; }
        }
        for (const AudioBlock &audioBlock : audioBlocks) encodeAudioBlock(audioBlock);
    }
    if (audio && audioFifo && av_audio_fifo_size(audioFifo) > 0) {
        av_frame_make_writable(audioFrame);
        const int available = av_audio_fifo_size(audioFifo);
        av_samples_set_silence(audioFrame->data, 0, audio->frame_size, kOutputChannels, audio->sample_fmt);
        av_audio_fifo_read(audioFifo, reinterpret_cast<void **>(audioFrame->data), available);
        audioFrame->pts = nextAudioPts;
        nextAudioPts += audio->frame_size;
        if (avcodec_send_frame(audio, audioFrame) >= 0) { drain(audio, format, audioStream); QMutexLocker lock(&mutex_); ++encodedAudioFrames_; }
    }
    // Stop accepting input before this point. Drain both encoders and write
    // delayed packets before exposing the recorder as Idle to the UI.
    if (video && headerWritten) { avcodec_send_frame(video, nullptr); drain(video, format, videoStream); }
    if (audio && headerWritten) { avcodec_send_frame(audio, nullptr); drain(audio, format, audioStream); }
    if (format && headerWritten && (result=av_write_trailer(format))<0)
        fail(QStringLiteral("MKV finalization failed: %1").arg(ffmpegError(result)));
    if (format && !(format->oformat->flags & AVFMT_NOFILE) && format->pb) avio_closep(&format->pb);
    headerWritten = false;
    if (state()!=RecordingState::Error) setState(RecordingState::Idle);
}
