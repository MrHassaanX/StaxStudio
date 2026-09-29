#pragma once
#include "AudioTypes.h"
#include <QHash>
#include <QVariantMap>
#include <deque>
#include <algorithm>
#include <cmath>

// Single-owner timeline mixer. Sources keep independent queues; arrival order
// never decides which source owns a sample. Caller supplies the playout deadline.
class ProgramAudioMixer final {
public:
    static constexpr int Rate = 48000, Quantum = 480;
    static constexpr qint64 LatencyNs = 40'000'000;
    void reset(qint64 originNs) { sources_.clear(); origin_ = originNs; cursor_ = 0; received_ = produced_ = late_ = rejected_ = 0; }
    void push(const QString &id, AudioBlock block) {
        ++received_;
        if (!block.isValid() || block.sampleRate != Rate || block.frameCount()>2*Rate) { ++rejected_; return; }
        const qint64 start = std::llround((block.timestampNs-origin_) * (double(Rate)/1e9));
        if (start + block.frameCount() <= cursor_) { ++late_; return; }
        if (start > cursor_ + 2*Rate) { ++rejected_; return; }
        auto &queue = sources_[id];
        if (queue.size() >= 256) { ++rejected_; return; }
        auto where = std::upper_bound(queue.begin(),queue.end(),start,[](qint64 frame,const Segment &s){return frame<s.start;});
        queue.insert(where, {start,std::move(block)});
    }
    template<class Sink> void produceUntil(qint64 deadlineNs, Sink sink) {
        const qint64 available = std::llround((deadlineNs-origin_) * (double(Rate)/1e9));
        while (cursor_ + Quantum <= available) {
            AudioBlock mix{origin_ + cursor_*1'000'000'000LL/Rate,Rate,2,QVector<float>(Quantum*2),3};
            for (auto it=sources_.begin();it!=sources_.end();) {
                auto &queue=it.value();
                qint64 consumed=cursor_;
                for (const auto &segment:queue) {
                    const qint64 begin=qMax(consumed,segment.start);
                    const qint64 end=qMin(cursor_+Quantum,segment.start+segment.block.frameCount());
                    for (qint64 frame=begin;frame<end;++frame) {
                        const int src=int(frame-segment.start)*segment.block.channelCount;
                        const int dst=int(frame-cursor_)*2;
                        mix.samples[dst]+=segment.block.samples[src];
                        // Preserve the existing front-L/R mapping for >2 channels;
                        // mono is duplicated at unity, not divided between ears.
                        mix.samples[dst+1]+=segment.block.samples[src+qMin(1,segment.block.channelCount-1)];
                    }
                    // Deduplicate overlaps only within THIS source, never across sources.
                    consumed=qMax(consumed,end);
                }
                while (!queue.empty() && queue.front().start+queue.front().block.frameCount()<=cursor_+Quantum) queue.pop_front();
                if(queue.empty()) it=sources_.erase(it); else ++it;
            }
            for(float &sample:mix.samples) sample=std::isfinite(sample)?std::clamp(sample,-1.0f,1.0f):0.0f;
            cursor_+=Quantum; ++produced_;
            sink(ProgramMixedAudioBlock{std::move(mix)});
        }
    }
    QVariantMap diagnostics() const {
        return {{"sourceBlocksReceived",received_},{"mixedBlocksProduced",produced_},
            {"mixedFramesProduced",produced_*Quantum},{"lateSourceBlocks",late_},{"rejectedSourceBlocks",rejected_}};
    }
private:
    struct Segment { qint64 start; AudioBlock block; };
    QHash<QString,std::deque<Segment>> sources_;
    qint64 origin_=0,cursor_=0;
    quint64 received_=0,produced_=0,late_=0,rejected_=0;
};
