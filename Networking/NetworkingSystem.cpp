#include "Networking/NetworkingSystem.hpp"

#include <algorithm>
#include <utility>

namespace elysium {

bool NetworkingSystem::initialize(LogFn logger) {
    m_logger = std::move(logger);
    m_initialized = true;
    if (m_logger) {
        m_logger("NetworkingSystem initialized (skeleton).");
    }
    return true;
}

void NetworkingSystem::shutdown() {
    m_channels.clear();
    m_initialized = false;
}

void NetworkingSystem::preUpdate(float) {
}

void NetworkingSystem::update(float) {
}

void NetworkingSystem::postUpdate(float) {
}

void NetworkingSystem::setMode(NetworkMode mode) {
    m_mode = mode;
}

void NetworkingSystem::setTickRateHz(float tickRateHz) {
    m_tickRateHz = std::max(1.0f, tickRateHz);
}

void NetworkingSystem::addChannel(const ReplicationChannel& channel) {
    m_channels.push_back(channel);
}

} // namespace elysium
