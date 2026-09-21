#pragma once

enum class TransitionType {
    Cut,
    Fade
};

struct TransitionSettings final {
    TransitionType type = TransitionType::Fade;
    int durationMs = 300;
};
