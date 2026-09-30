#pragma once

#include <QString>

enum class StreamingState { Idle, Connecting, Live, Reconnecting, Stopping, Error };

struct StreamSettings final {
    QString serverUrl;
    QString streamKey;
    int videoBitrateKbps = 6000;
    int audioBitrateKbps = 160;
    int frameRate = 60;
    int maxReconnectAttempts = 3;
};

[[nodiscard]] QString streamingStateName(StreamingState state);
[[nodiscard]] QString redactedStreamUrl(const StreamSettings &settings);

