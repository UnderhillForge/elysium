#pragma once

#include "Foundation/Subsystem.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace elysium {

enum class NetworkMode {
    Disabled,
    Client,
    Server
};

struct ReplicationChannel {
    std::string name;
    std::uint8_t qosClass{0};
    bool reliable{false};
};

// Inspired by RavEngine: replication transport and game-state authority should
// stay decoupled so editor/offline runtime can run with networking disabled.
class NetworkingSystem final : public Subsystem {
public:
    const char* name() const override { return "NetworkingSystem"; }
    bool initialize(LogFn logger) override;
    void shutdown() override;
    void preUpdate(float dt) override;
    void update(float dt) override;
    void postUpdate(float dt) override;

    void setMode(NetworkMode mode);
    NetworkMode mode() const { return m_mode; }

    void setTickRateHz(float tickRateHz);
    float tickRateHz() const { return m_tickRateHz; }

    void addChannel(const ReplicationChannel& channel);
    const std::vector<ReplicationChannel>& channels() const { return m_channels; }

private:
    LogFn m_logger;
    NetworkMode m_mode{NetworkMode::Disabled};
    float m_tickRateHz{20.0f};
    std::vector<ReplicationChannel> m_channels;
    bool m_initialized{false};
};

} // namespace elysium
