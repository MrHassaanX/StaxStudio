#include "AudioProcessing.h"
#include <QtMath>
#include <stdexcept>
extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
}

namespace AudioProcessing {
static AudioBlock convert(AudioBlock block, AudioResampleState *state,
                          const uint8_t *input, int inputFrames, AVSampleFormat inputFormat)
{
    if (block.sampleRate <= 0 || block.channelCount <= 0)
        throw std::runtime_error("Invalid audio format");
    AudioResampleState temporary;
    auto &s = state ? *state : temporary;
    if (!s.context || s.inputSampleRate != block.sampleRate || s.channelCount != block.channelCount
        || s.inputFormat != inputFormat || s.channelMask != block.channelMask) {
        s = {};
        s.inputSampleRate = block.sampleRate;
        s.channelCount = block.channelCount;
        s.inputFormat = inputFormat;
        s.channelMask = block.channelMask;
        s.originNs = block.timestampNs;
        AVChannelLayout layout{};
        if (block.channelMask) av_channel_layout_from_mask(&layout,block.channelMask);
        else av_channel_layout_default(&layout, block.channelCount);
        if(layout.nb_channels!=block.channelCount) { av_channel_layout_uninit(&layout); throw std::runtime_error("Invalid audio channel mask"); }
        SwrContext *context = nullptr;
        const int result = swr_alloc_set_opts2(&context, &layout, AV_SAMPLE_FMT_FLT, InternalSampleRate,
                                              &layout, inputFormat, block.sampleRate, 0, nullptr);
        av_channel_layout_uninit(&layout);
        s.context.reset(context, [](SwrContext *value) { swr_free(&value); });
        if (result < 0 || !context || swr_init(context) < 0)
            throw std::runtime_error("Audio resampler initialization failed");
    }
    const int capacity = swr_get_out_samples(s.context.get(), inputFrames);
    QVector<float> output(capacity * block.channelCount);
    uint8_t *destination = reinterpret_cast<uint8_t *>(output.data());
    int frames = swr_convert(s.context.get(), &destination, capacity, inputFrames ? &input : nullptr, inputFrames);
    if (frames < 0) throw std::runtime_error("Audio resampling failed");
    // A one-shot conversion drains its filter tail; streaming callers retain
    // SwrContext history and drain explicitly with an empty block at shutdown.
    if (!state) {
        destination += frames * block.channelCount * sizeof(float);
        const int tail = swr_convert(s.context.get(), &destination, capacity - frames, nullptr, 0);
        if (tail > 0) frames += tail;
    }
    output.resize(frames * block.channelCount);
    block.timestampNs = s.originNs + s.outputFrames * 1'000'000'000LL / InternalSampleRate;
    s.outputFrames += frames;
    block.samples = std::move(output);
    block.sampleRate = InternalSampleRate;
    return block;
}

AudioBlock normalizeToInternalFormat(AudioBlock block, AudioResampleState *state)
{
    if(block.sampleRate==InternalSampleRate) { if(state) *state={}; return block; }
    const int frames=block.channelCount>0 ? block.samples.size()/block.channelCount : 0;
    const auto *input=reinterpret_cast<const uint8_t *>(block.samples.constData());
    return convert(std::move(block),state,input,frames,AV_SAMPLE_FMT_FLT);
}

AudioBlock convertNativePcm(const void *data,int frames,int rate,int channels,int bits,bool floatingPoint,
                           quint64 channelMask,qint64 timestampNs,AudioResampleState &state)
{
    AudioBlock block{timestampNs,rate,channels,{},channelMask};
    if(floatingPoint && bits==32 && rate==InternalSampleRate) {
        state={};
        block.samples.resize(frames*channels);
        if(data) memcpy(block.samples.data(),data,block.samples.size()*sizeof(float));
        return block;
    }
    AVSampleFormat format=AV_SAMPLE_FMT_NONE;
    if(floatingPoint) format=bits==32 ? AV_SAMPLE_FMT_FLT : bits==64 ? AV_SAMPLE_FMT_DBL : AV_SAMPLE_FMT_NONE;
    else format=bits==8 ? AV_SAMPLE_FMT_U8 : bits==16 ? AV_SAMPLE_FMT_S16 : (bits==24 || bits==32) ? AV_SAMPLE_FMT_S32 : AV_SAMPLE_FMT_NONE;
    if(format==AV_SAMPLE_FMT_NONE) throw std::runtime_error("Unsupported WASAPI sample format");
    QByteArray scratch;
    const uint8_t *input=static_cast<const uint8_t *>(data);
    if(!data || (!floatingPoint && bits==24)) {
        const int bytes=av_get_bytes_per_sample(format);
        scratch.resize(frames*channels*bytes);
        scratch.fill(format==AV_SAMPLE_FMT_U8 ? char(128) : 0);
        if(data) {
            // FFmpeg S32 expects packed PCM24 left-aligned in a 32-bit word.
            for(int i=0;i<frames*channels;++i) {
                const quint32 sample=(quint32(input[i*3])<<8)|(quint32(input[i*3+1])<<16)|(quint32(input[i*3+2])<<24);
                memcpy(scratch.data()+i*4,&sample,4);
            }
        }
        input=reinterpret_cast<const uint8_t *>(scratch.constData());
    }
    return convert(std::move(block),&state,input,frames,format);
}

void applyGainAndMute(AudioBlock &block, const float gain, const bool muted)
{
    const float effectiveGain = muted ? 0.0f : qBound(0.0f, gain, 10.0f); // +20 dB maximum, 1 = 0 dB.
    if (effectiveGain == 1.0f) return;
    for (float &sample : block.samples) sample *= effectiveGain;
}

AudioBlock mixToStereo(const QVector<AudioBlock> &blocks)
{
    int frames = 0;
    qint64 timestampNs = 0;
    for (const AudioBlock &block : blocks) {
        if (block.samples.isEmpty() || block.channelCount <= 0) continue;
        frames = qMax(frames, block.samples.size() / block.channelCount);
        if (timestampNs == 0 || block.timestampNs < timestampNs) timestampNs = block.timestampNs;
    }
    AudioBlock mixed{timestampNs, InternalSampleRate, 2, QVector<float>(frames * 2)};
    for (const AudioBlock &block : blocks) {
        if (block.channelCount <= 0) continue;
        const int sourceFrames = block.samples.size() / block.channelCount;
        for (int frame = 0; frame < sourceFrames; ++frame) {
            mixed.samples[frame * 2] += block.samples[frame * block.channelCount];
            mixed.samples[frame * 2 + 1] += block.samples[frame * block.channelCount + qMin(1, block.channelCount - 1)];
        }
    }
    // A single unity-gain source stays untouched. Multiple sources are only
    // constrained when their sum would otherwise clip the recorder input.
    for (float &sample : mixed.samples) sample = qBound(-1.0f, sample, 1.0f);
    return mixed;
}

float peakDb(const AudioBlock &block)
{
    float peak = 0.0f;
    for (const float sample : block.samples) peak = qMax(peak, qAbs(sample));
    return peak > 0.0f ? qMax(-90.0f, 20.0f * qLn(peak) / qLn(10.0f)) : -90.0f;
}
}
