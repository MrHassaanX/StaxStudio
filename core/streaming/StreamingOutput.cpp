#include "StreamingOutput.h"

#include <QImage>
#include <QScopeGuard>
#include <QThread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
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

QString redactedError(QString text, const StreamSettings &settings)
{
    const QString key = settings.streamKey.trimmed();
    if (!key.isEmpty()) text.replace(key, QStringLiteral("<stream-key>"), Qt::CaseSensitive);
    return text;
}

QString fullStreamUrl(const StreamSettings &settings)
{
    QString url = settings.serverUrl.trimmed();
    const QString key = settings.streamKey.trimmed();
    if (key.isEmpty()) return url;
    if (!url.endsWith(QLatin1Char('/'))) url.append(QLatin1Char('/'));
    return url + key;
}

int drain(AVCodecContext *context, AVFormatContext *format, AVStream *stream, quint64 *bytesSent = nullptr)
{
    AVPacket *packet = av_packet_alloc();
    if (!packet) return AVERROR(ENOMEM);
    int count = 0, result = 0;
    while ((result = avcodec_receive_packet(context, packet)) == 0) {
        av_packet_rescale_ts(packet, context->time_base, stream->time_base);
        packet->stream_index = stream->index;
        const int size = packet->size;
        result = av_interleaved_write_frame(format, packet);
        av_packet_unref(packet);
        if (result < 0) break;
        if (bytesSent) *bytesSent += qMax(0, size);
        ++count;
    }
    av_packet_free(&packet);
    return result == AVERROR(EAGAIN) || result == AVERROR_EOF ? count : result;
}

bool isMockUrl(const QString &url)
{
    return url.startsWith(QStringLiteral("staxmock://"), Qt::CaseInsensitive);
}
}

StreamingOutput::StreamingOutput(QObject *parent) : QObject(parent)
{
    elapsedTimer_.setInterval(250);
    connect(&elapsedTimer_, &QTimer::timeout, this, &StreamingOutput::elapsedChanged);
}

StreamingOutput::~StreamingOutput()
{
    stop();
}

bool StreamingOutput::start(const StreamSettings &settings, const QSize size)
{
    if (!size.isValid()) return false;
    if (settings.serverUrl.trimmed().isEmpty()) {
        setState(StreamingState::Error, QStringLiteral("Enter an RTMP server URL."));
        return false;
    }
    {
        QMutexLocker lock(&mutex_);
        if (state_ != StreamingState::Idle && state_ != StreamingState::Error) return false;
        settings_ = settings;
        programSize_ = size;
        stopRequested_ = false;
        liveStartedNs_ = 0;
        submittedVideoFrames_ = submittedAudioBlocks_ = encodedVideoFrames_ = encodedAudioFrames_ = 0;
        droppedVideoFrames_ = droppedAudioBlocks_ = bytesSent_ = writeFailures_ = reconnectAttempts_ = 0;
        firstVideoTimestampNs_ = lastVideoTimestampNs_ = firstAudioTimestampNs_ = lastAudioTimestampNs_ = 0;
        videoQueue_.clear();
        audioQueue_.clear();
        errorMessage_.clear();
        state_ = StreamingState::Connecting;
    }
    emit stateChanged();
    emit elapsedChanged();
    workerThread_ = std::jthread([this, settings, size] { run(settings, size); });
    return true;
}

void StreamingOutput::stop()
{
    {
        QMutexLocker lock(&mutex_);
        if (state_ == StreamingState::Idle) return;
        if (state_ != StreamingState::Error) state_ = StreamingState::Stopping;
        stopRequested_ = true;
        wake_.wakeAll();
    }
    emit stateChanged();
    if (workerThread_.joinable()) workerThread_.join();
    if (state() != StreamingState::Error) setState(StreamingState::Idle);
}

void StreamingOutput::fail(const QString &message)
{
    {
        QMutexLocker lock(&mutex_);
        stopRequested_ = true;
        wake_.wakeAll();
    }
    setState(StreamingState::Error, message);
}

void StreamingOutput::submitVideoFrame(RecordedVideoFrame frame)
{
    if (frame.image.isNull()) return;
    QMutexLocker lock(&mutex_);
    if (state_ != StreamingState::Live) return;
    ++submittedVideoFrames_;
    if (submittedVideoFrames_ == 1) firstVideoTimestampNs_ = frame.timestampNs;
    lastVideoTimestampNs_ = frame.timestampNs;
    if (videoQueue_.size() >= 3) {
        videoQueue_.pop_front();
        ++droppedVideoFrames_;
    }
    videoQueue_.push_back(std::move(frame));
    wake_.wakeOne();
}

void StreamingOutput::submitProgramAudio(ProgramMixedAudioBlock program)
{
    AudioBlock block = std::move(program.audio);
    if (!block.isValid() || block.sampleRate != kSampleRate || block.channelCount != kOutputChannels) return;
    QMutexLocker lock(&mutex_);
    if (state_ != StreamingState::Live) return;
    ++submittedAudioBlocks_;
    if (submittedAudioBlocks_ == 1) firstAudioTimestampNs_ = block.timestampNs;
    lastAudioTimestampNs_ = block.timestampNs;
    if (audioQueue_.size() >= 256) {
        audioQueue_.pop_front();
        ++droppedAudioBlocks_;
    }
    audioQueue_.push_back(std::move(block));
    wake_.wakeOne();
}

QString StreamingOutput::stateName() const
{
    QMutexLocker lock(&mutex_);
    return streamingStateName(state_);
}

QString StreamingOutput::errorMessage() const
{
    QMutexLocker lock(&mutex_);
    return errorMessage_;
}

StreamingState StreamingOutput::state() const
{
    QMutexLocker lock(&mutex_);
    return state_;
}

qint64 StreamingOutput::elapsedMs() const
{
    QMutexLocker lock(&mutex_);
    return liveStartedNs_ > 0 && (state_ == StreamingState::Live || state_ == StreamingState::Reconnecting || state_ == StreamingState::Stopping)
        ? qMax<qint64>(0, (mediaTimestampNs() - liveStartedNs_) / 1'000'000) : 0;
}

QVariantMap StreamingOutput::diagnostics() const
{
    QMutexLocker lock(&mutex_);
    const qint64 now = mediaTimestampNs();
    const qint64 uptimeMs = liveStartedNs_ > 0 && (state_ == StreamingState::Live || state_ == StreamingState::Reconnecting || state_ == StreamingState::Stopping)
        ? qMax<qint64>(0, (now - liveStartedNs_) / 1'000'000) : 0;
    const qint64 elapsed = uptimeMs > 0 ? qMax<qint64>(1, uptimeMs / 1000) : 0;
    return {{"state", streamingStateName(state_)},
            {"uptimeMs", uptimeMs},
            {"serverUrl", redactedStreamUrl(settings_)},
            {"videoBitrateKbps", settings_.videoBitrateKbps},
            {"audioBitrateKbps", settings_.audioBitrateKbps},
            {"frameRate", settings_.frameRate},
            {"bytesSent", bytesSent_},
            {"estimatedBitrateKbps", elapsed > 0 ? static_cast<qulonglong>(bytesSent_ * 8 / elapsed / 1000) : 0},
            {"reconnectCount", reconnectAttempts_},
            {"queuedVideoFrames", static_cast<qulonglong>(videoQueue_.size())},
            {"queuedAudioBlocks", static_cast<qulonglong>(audioQueue_.size())},
            {"droppedStreamFrames", droppedVideoFrames_},
            {"droppedAudioBlocks", droppedAudioBlocks_},
            {"writeFailures", writeFailures_},
            {"lastNetworkError", errorMessage_},
            {"submittedVideoFrames", submittedVideoFrames_},
            {"submittedAudioBlocks", submittedAudioBlocks_},
            {"encodedVideoFrames", encodedVideoFrames_},
            {"encodedAudioFrames", encodedAudioFrames_},
            {"firstVideoTimestampNs", firstVideoTimestampNs_},
            {"lastVideoTimestampNs", lastVideoTimestampNs_},
            {"firstAudioTimestampNs", firstAudioTimestampNs_},
            {"lastAudioTimestampNs", lastAudioTimestampNs_}};
}

bool StreamingOutput::isAcceptingInput() const
{
    const StreamingState value = state();
    return value == StreamingState::Connecting || value == StreamingState::Live || value == StreamingState::Reconnecting;
}

void StreamingOutput::setState(const StreamingState state, QString error)
{
    {
        QMutexLocker lock(&mutex_);
        state_ = state;
        if (!error.isEmpty()) errorMessage_ = redactedError(std::move(error), settings_);
        if (state == StreamingState::Live && liveStartedNs_ == 0) liveStartedNs_ = mediaTimestampNs();
        if (state == StreamingState::Idle) liveStartedNs_ = 0;
    }
    QMetaObject::invokeMethod(this, [this, state] {
        if (state == StreamingState::Live) elapsedTimer_.start();
        if (state == StreamingState::Idle || state == StreamingState::Error) {
            elapsedTimer_.stop();
            emit elapsedChanged();
        }
        emit diagnosticsChanged();
        emit stateChanged();
    }, Qt::QueuedConnection);
}

void StreamingOutput::waitBeforeReconnect(const int attempt)
{
    const int delayMs = qMin(5000, 500 * qMax(1, attempt));
    for (int waited = 0; waited < delayMs; waited += 50) {
        {
            QMutexLocker lock(&mutex_);
            if (stopRequested_) return;
        }
        QThread::msleep(50);
    }
}

void StreamingOutput::run(StreamSettings settings, const QSize size)
{
    const int maxAttempts = qMax(0, settings.maxReconnectAttempts);
    for (int attempt = 0;; ++attempt) {
        {
            QMutexLocker lock(&mutex_);
            if (stopRequested_) break;
        }
        setState(attempt == 0 ? StreamingState::Connecting : StreamingState::Reconnecting);
        QString failure;
        const bool ok = isMockUrl(settings.serverUrl) ? runMock(settings) : runFfmpeg(settings, size, &failure);
        {
            QMutexLocker lock(&mutex_);
            if (stopRequested_) break;
        }
        if (ok) break;
        {
            QMutexLocker lock(&mutex_);
            ++writeFailures_;
            errorMessage_ = redactedError(failure.isEmpty() ? QStringLiteral("Streaming connection failed.") : failure, settings_);
        }
        if (attempt >= maxAttempts) {
            setState(StreamingState::Error, failure.isEmpty() ? QStringLiteral("Streaming connection failed.") : failure);
            return;
        }
        {
            QMutexLocker lock(&mutex_);
            ++reconnectAttempts_;
            videoQueue_.clear();
            audioQueue_.clear();
        }
        setState(StreamingState::Reconnecting, failure);
        waitBeforeReconnect(attempt + 1);
    }
    if (state() != StreamingState::Error) setState(StreamingState::Idle);
}

bool StreamingOutput::runMock(const StreamSettings settings)
{
    if (settings.serverUrl.contains(QStringLiteral("fail"), Qt::CaseInsensitive))
        return false;
    setState(StreamingState::Live);
    int blocks = 0;
    while (true) {
        std::deque<RecordedVideoFrame> video;
        std::deque<AudioBlock> audio;
        {
            QMutexLocker lock(&mutex_);
            if (videoQueue_.empty() && audioQueue_.empty() && !stopRequested_) wake_.wait(&mutex_, 20);
            video.swap(videoQueue_);
            audio.swap(audioQueue_);
            if (stopRequested_) break;
        }
        if (settings.serverUrl.contains(QStringLiteral("disconnect"), Qt::CaseInsensitive) && ++blocks > 3)
            return false;
        if (settings.serverUrl.contains(QStringLiteral("slow"), Qt::CaseInsensitive))
            QThread::msleep(50);
        QMutexLocker lock(&mutex_);
        encodedVideoFrames_ += video.size();
        encodedAudioFrames_ += audio.size();
        bytesSent_ += static_cast<quint64>(video.size() * 2048 + audio.size() * 512);
    }
    return true;
}

bool StreamingOutput::runFfmpeg(StreamSettings settings, const QSize size, QString *failure)
{
    AVFormatContext *format = nullptr;
    AVCodecContext *video = nullptr;
    AVCodecContext *audio = nullptr;
    SwsContext *scaler = nullptr;
    AVFrame *videoFrame = nullptr;
    AVFrame *audioFrame = nullptr;
    AVAudioFifo *audioFifo = nullptr;
    bool headerWritten = false;
    int result = 0;
    auto cleanup = qScopeGuard([&] {
        if (video && headerWritten) { avcodec_send_frame(video, nullptr); drain(video, format, format->streams[0], &bytesSent_); }
        if (audio && headerWritten) { avcodec_send_frame(audio, nullptr); drain(audio, format, format->streams[1], &bytesSent_); }
        if (format && headerWritten) av_write_trailer(format);
        if (format && !(format->oformat->flags & AVFMT_NOFILE) && format->pb) avio_closep(&format->pb);
        av_audio_fifo_free(audioFifo);
        av_frame_free(&audioFrame);
        av_frame_free(&videoFrame);
        sws_freeContext(scaler);
        avcodec_free_context(&audio);
        avcodec_free_context(&video);
        avformat_free_context(format);
    });

    const QString url = fullStreamUrl(settings);
    result = avformat_alloc_output_context2(&format, nullptr, "flv", url.toUtf8().constData());
    if (result < 0 || !format) { if (failure) *failure = QStringLiteral("Unable to create RTMP/FLV output: %1").arg(ffmpegError(result)); return false; }

    const int fps = qBound(1, settings.frameRate, 60);
    const AVCodec *videoCodec = avcodec_find_encoder_by_name("libx264");
    if (!videoCodec) videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!videoCodec) { if (failure) *failure = QStringLiteral("No H.264 encoder is available in FFmpeg."); return false; }
    AVStream *videoStream = avformat_new_stream(format, nullptr);
    video = avcodec_alloc_context3(videoCodec);
    video->codec_id = videoCodec->id;
    video->width = size.width() & ~1;
    video->height = size.height() & ~1;
    video->pix_fmt = AV_PIX_FMT_YUV420P;
    video->time_base = AVRational{1, fps};
    video->framerate = AVRational{fps, 1};
    video->bit_rate = qMax(300, settings.videoBitrateKbps) * 1000LL;
    video->gop_size = fps * 2;
    video->max_b_frames = 0;
    if (format->oformat->flags & AVFMT_GLOBALHEADER) video->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    av_opt_set(video->priv_data, "preset", "veryfast", 0);
    av_opt_set(video->priv_data, "tune", "zerolatency", 0);
    av_opt_set(video->priv_data, "bf", "0", 0);
    if ((result = avcodec_open2(video, videoCodec, nullptr)) < 0) { if (failure) *failure = QStringLiteral("H.264 streaming initialization failed: %1").arg(ffmpegError(result)); return false; }
    avcodec_parameters_from_context(videoStream->codecpar, video);
    videoStream->time_base = video->time_base;
    videoFrame = av_frame_alloc();
    videoFrame->format = video->pix_fmt;
    videoFrame->width = video->width;
    videoFrame->height = video->height;
    if (av_frame_get_buffer(videoFrame, 32) < 0) { if (failure) *failure = QStringLiteral("Unable to allocate stream video frame."); return false; }
    scaler = sws_getContext(video->width, video->height, AV_PIX_FMT_BGRA, video->width, video->height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!scaler) { if (failure) *failure = QStringLiteral("Unable to initialize stream video conversion."); return false; }

    const AVCodec *audioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!audioCodec) { if (failure) *failure = QStringLiteral("No AAC encoder is available in FFmpeg."); return false; }
    AVStream *audioStream = avformat_new_stream(format, nullptr);
    audio = avcodec_alloc_context3(audioCodec);
    audio->sample_rate = kSampleRate;
    audio->sample_fmt = AV_SAMPLE_FMT_FLTP;
    av_channel_layout_default(&audio->ch_layout, kOutputChannels);
    audio->time_base = AVRational{1, kSampleRate};
    audio->bit_rate = qMax(64, settings.audioBitrateKbps) * 1000LL;
    if (format->oformat->flags & AVFMT_GLOBALHEADER) audio->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if ((result = avcodec_open2(audio, audioCodec, nullptr)) < 0) { if (failure) *failure = QStringLiteral("AAC streaming initialization failed: %1").arg(ffmpegError(result)); return false; }
    avcodec_parameters_from_context(audioStream->codecpar, audio);
    audioStream->time_base = audio->time_base;
    audioFrame = av_frame_alloc();
    audioFrame->format = audio->sample_fmt;
    audioFrame->sample_rate = audio->sample_rate;
    audioFrame->nb_samples = audio->frame_size;
    av_channel_layout_copy(&audioFrame->ch_layout, &audio->ch_layout);
    if (av_frame_get_buffer(audioFrame, 0) < 0) { if (failure) *failure = QStringLiteral("Unable to allocate stream audio frame."); return false; }
    audioFifo = av_audio_fifo_alloc(audio->sample_fmt, kOutputChannels, audio->frame_size * 4);

    AVDictionary *options = nullptr;
    av_dict_set(&options, "rw_timeout", "5000000", 0);
    result = avio_open2(&format->pb, url.toUtf8().constData(), AVIO_FLAG_WRITE, nullptr, &options);
    av_dict_free(&options);
    if (result < 0) { if (failure) *failure = QStringLiteral("Unable to connect to RTMP server %1: %2").arg(redactedStreamUrl(settings), ffmpegError(result)); return false; }
    if ((result = avformat_write_header(format, nullptr)) < 0) { if (failure) *failure = QStringLiteral("Unable to start RTMP stream: %1").arg(ffmpegError(result)); return false; }
    headerWritten = true;
    setState(StreamingState::Live);

    qint64 originNs = -1, nextVideoPts = 0, nextAudioPts = 0, audioTimelineEnd = 0, audioOriginNs = -1;
    const auto encodeAudio = [&](const AudioBlock &block) -> bool {
        if (block.samples.isEmpty()) return true;
        if (audioOriginNs < 0) audioOriginNs = block.timestampNs;
        const qint64 desiredPts = av_rescale_q(qMax<qint64>(0, block.timestampNs - audioOriginNs), AVRational{1, 1'000'000'000}, audio->time_base);
        if (desiredPts > audioTimelineEnd) {
            const int gap = static_cast<int>(qMin<qint64>(desiredPts - audioTimelineEnd, 48'000 * 2LL));
            QVector<float> silence(gap, 0.0f);
            void *silentPlanes[] = {silence.data(), silence.data()};
            if (av_audio_fifo_realloc(audioFifo, av_audio_fifo_size(audioFifo) + gap) < 0) return false;
            av_audio_fifo_write(audioFifo, silentPlanes, gap);
            audioTimelineEnd += gap;
        }
        const int totalFrames = block.frameCount();
        const int trim = static_cast<int>(qMin<qint64>(totalFrames, qMax<qint64>(0, audioTimelineEnd - desiredPts)));
        const int inFrames = totalFrames - trim;
        if (inFrames <= 0) return true;
        QVector<float> left(inFrames), right(inFrames);
        for (int frame = 0; frame < inFrames; ++frame) {
            const int sourceFrame = frame + trim;
            left[frame] = block.samples[sourceFrame * block.channelCount];
            right[frame] = block.samples[sourceFrame * block.channelCount + 1];
        }
        void *data[] = {left.data(), right.data()};
        if (av_audio_fifo_realloc(audioFifo, av_audio_fifo_size(audioFifo) + inFrames) < 0) return false;
        av_audio_fifo_write(audioFifo, data, inFrames);
        audioTimelineEnd += inFrames;
        while (av_audio_fifo_size(audioFifo) >= audio->frame_size) {
            av_frame_make_writable(audioFrame);
            av_audio_fifo_read(audioFifo, reinterpret_cast<void **>(audioFrame->data), audio->frame_size);
            audioFrame->pts = nextAudioPts;
            nextAudioPts += audio->frame_size;
            if (avcodec_send_frame(audio, audioFrame) < 0) return false;
            const int packets = drain(audio, format, audioStream, &bytesSent_);
            if (packets < 0) return false;
            QMutexLocker lock(&mutex_);
            encodedAudioFrames_ += packets;
        }
        return true;
    };

    while (true) {
        RecordedVideoFrame image;
        std::deque<AudioBlock> audioBlocks;
        {
            QMutexLocker lock(&mutex_);
            if (videoQueue_.empty() && audioQueue_.empty() && !stopRequested_) wake_.wait(&mutex_, 100);
            if (!videoQueue_.empty()) {
                image = std::move(videoQueue_.front());
                videoQueue_.pop_front();
            }
            audioBlocks.swap(audioQueue_);
            if (stopRequested_ && image.image.isNull() && audioBlocks.empty()) break;
        }
        if (!image.image.isNull()) {
            if (originNs < 0) originNs = image.timestampNs;
            const qint64 desiredPts = qMax(nextVideoPts, av_rescale_q(qMax<qint64>(0, image.timestampNs - originNs), AVRational{1, 1'000'000'000}, video->time_base));
            QImage bgra = image.image.convertToFormat(QImage::Format_ARGB32);
            if (bgra.size() != QSize(video->width, video->height)) bgra = bgra.scaled(video->width, video->height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            av_frame_make_writable(videoFrame);
            const uint8_t *planes[] = {bgra.constBits(), nullptr, nullptr, nullptr};
            const int strides[] = {static_cast<int>(bgra.bytesPerLine()), 0, 0, 0};
            sws_scale(scaler, planes, strides, 0, video->height, videoFrame->data, videoFrame->linesize);
            videoFrame->pts = desiredPts;
            nextVideoPts = desiredPts + 1;
            if (avcodec_send_frame(video, videoFrame) < 0) { if (failure) *failure = QStringLiteral("H.264 stream encoding failed."); return false; }
            const int packets = drain(video, format, videoStream, &bytesSent_);
            if (packets < 0) { if (failure) *failure = QStringLiteral("RTMP video write failed: %1").arg(ffmpegError(packets)); return false; }
            QMutexLocker lock(&mutex_);
            encodedVideoFrames_ += packets;
        }
        for (const AudioBlock &block : audioBlocks) {
            if (!encodeAudio(block)) { if (failure) *failure = QStringLiteral("RTMP audio write failed."); return false; }
        }
    }
    return true;
}

