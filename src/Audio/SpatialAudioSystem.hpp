#pragma once

#include "Foundation/Subsystem.hpp"

#include <glm/vec3.hpp>

#include <string>
#include <vector>

namespace elysium {

struct AudioListenerState {
    glm::vec3 position{0.0f};
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
};

struct AudioEmitterState {
    std::string sourceId;
    std::string clipPath;
    glm::vec3 position{0.0f};
    float gain{1.0f};
    float minDistance{1.0f};
    float maxDistance{40.0f};
    bool looping{false};
};

// Inspired by RavEngine: split listener/emitter data from playback backend so
// headless simulation can run without a hardware audio device.
class SpatialAudioSystem final : public Subsystem {
public:
    const char* name() const override { return "SpatialAudioSystem"; }
    bool initialize(LogFn logger) override;
    void shutdown() override;
    void preUpdate(float dt) override;
    void update(float dt) override;
    void postUpdate(float dt) override;

    void setListener(const AudioListenerState& listener);
    void submitEmitter(const AudioEmitterState& emitter);
    void clearEmitters();
    std::size_t submittedEmitterCount() const { return m_emitters.size(); }

private:
    LogFn m_logger;
    AudioListenerState m_listener;
    std::vector<AudioEmitterState> m_emitters;
    bool m_initialized{false};
};

} // namespace elysium
