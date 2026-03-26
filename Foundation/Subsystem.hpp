#pragma once

#include <functional>
#include <string>

namespace elysium {

// Inspired by RavEngine: explicit system lifecycle boundaries make headless/server
// and editor runtime initialization predictable without hard-coding startup order.
class Subsystem {
public:
    using LogFn = std::function<void(const std::string&)>;

    virtual ~Subsystem() = default;

    virtual const char* name() const = 0;
    virtual bool initialize(LogFn logger) = 0;
    virtual void shutdown() = 0;
    virtual void preUpdate(float dt) = 0;
    virtual void update(float dt) = 0;
    virtual void postUpdate(float dt) = 0;
};

} // namespace elysium
