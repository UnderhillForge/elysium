#include "Audio/SpatialAudioSystem.hpp"

#include <SDL.h>

#include <glm/geometric.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace elysium {

namespace {

constexpr int kDeviceSampleRate = 48000;
constexpr int kDeviceChannels = 2;
constexpr std::size_t kMixFramesPerChunk = 512;
constexpr std::size_t kTargetQueuedFrames = 4096;

struct DecodedClip {
    std::vector<float> monoSamples;
    int sampleRate{kDeviceSampleRate};
};

struct ActiveVoice {
    std::shared_ptr<DecodedClip> clip;
    std::size_t cursor{0};
    float gain{1.0f};
    float pan{0.0f};
    bool looping{false};
    bool seenThisFrame{false};
};

struct SpatialAudioBackendState {
    SDL_AudioDeviceID device{0};
    SDL_AudioSpec deviceSpec{};
    bool enabled{false};
    std::unordered_map<std::string, std::shared_ptr<DecodedClip>> clipCache;
    std::unordered_map<std::string, ActiveVoice> activeVoices;
    std::unordered_set<std::string> failedClipPaths;
};

SpatialAudioBackendState& backendState() {
    static SpatialAudioBackendState s_state;
    return s_state;
}

std::string toLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string normalizeClipPath(const std::string& clipPath) {
    if (clipPath.empty()) {
        return std::string{};
    }

    std::filesystem::path path{clipPath};
    if (path.is_relative()) {
        path = std::filesystem::current_path() / path;
    }
    return path.lexically_normal().string();
}

bool decodeWavToDeviceMono(const std::string& path,
                          const SDL_AudioSpec& deviceSpec,
                          std::shared_ptr<DecodedClip>& outClip)
{
    SDL_AudioSpec srcSpec{};
    Uint8* srcBuffer = nullptr;
    Uint32 srcLength = 0;
    if (SDL_LoadWAV(path.c_str(), &srcSpec, &srcBuffer, &srcLength) == nullptr) {
        return false;
    }

    SDL_AudioCVT cvt{};
    const int cvtResult = SDL_BuildAudioCVT(
        &cvt,
        srcSpec.format,
        srcSpec.channels,
        srcSpec.freq,
        AUDIO_F32SYS,
        static_cast<Uint8>(deviceSpec.channels),
        deviceSpec.freq);

    if (cvtResult < 0) {
        SDL_FreeWAV(srcBuffer);
        return false;
    }

    cvt.len = static_cast<int>(srcLength);
    cvt.buf = static_cast<Uint8*>(SDL_malloc(static_cast<std::size_t>(cvt.len) * static_cast<std::size_t>(cvt.len_mult)));
    if (cvt.buf == nullptr) {
        SDL_FreeWAV(srcBuffer);
        return false;
    }

    std::memcpy(cvt.buf, srcBuffer, srcLength);
    SDL_FreeWAV(srcBuffer);

    if (SDL_ConvertAudio(&cvt) < 0) {
        SDL_free(cvt.buf);
        return false;
    }

    const int channels = std::max(1, static_cast<int>(deviceSpec.channels));
    const std::size_t floatCount = static_cast<std::size_t>(cvt.len_cvt) / sizeof(float);
    if (floatCount < static_cast<std::size_t>(channels)) {
        SDL_free(cvt.buf);
        return false;
    }

    const float* interleaved = reinterpret_cast<const float*>(cvt.buf);
    const std::size_t frameCount = floatCount / static_cast<std::size_t>(channels);
    auto clip = std::make_shared<DecodedClip>();
    clip->sampleRate = deviceSpec.freq;
    clip->monoSamples.resize(frameCount);

    for (std::size_t frame = 0; frame < frameCount; ++frame) {
        float mono = 0.0f;
        for (int channel = 0; channel < channels; ++channel) {
            mono += interleaved[frame * static_cast<std::size_t>(channels) + static_cast<std::size_t>(channel)];
        }
        clip->monoSamples[frame] = mono / static_cast<float>(channels);
    }

    SDL_free(cvt.buf);
    outClip = std::move(clip);
    return true;
}

float computePan(const AudioListenerState& listener, const glm::vec3& emitterPos) {
    const glm::vec3 delta = emitterPos - listener.position;
    const float lenSq = glm::dot(delta, delta);
    if (lenSq <= 1e-6f) {
        return 0.0f;
    }

    glm::vec3 right = glm::cross(listener.forward, listener.up);
    const float rightLenSq = glm::dot(right, right);
    if (rightLenSq <= 1e-6f) {
        return 0.0f;
    }

    right = right / std::sqrt(rightLenSq);
    const glm::vec3 dir = delta / std::sqrt(lenSq);
    return std::clamp(glm::dot(right, dir), -1.0f, 1.0f);
}

float computeDistanceAttenuation(const AudioEmitterState& emitter, const AudioListenerState& listener) {
    const glm::vec3 delta = emitter.position - listener.position;
    const float distance = std::sqrt(std::max(0.0f, glm::dot(delta, delta)));
    const float minDist = std::max(0.01f, emitter.minDistance);
    const float maxDist = std::max(minDist + 0.01f, emitter.maxDistance);

    if (distance <= minDist) {
        return 1.0f;
    }
    if (distance >= maxDist) {
        return 0.0f;
    }

    const float t = (distance - minDist) / (maxDist - minDist);
    return 1.0f - t;
}

void mixChunk(SpatialAudioBackendState& state,
              std::vector<float>& scratch,
              std::size_t frames)
{
    scratch.assign(frames * kDeviceChannels, 0.0f);

    std::vector<std::string> ended;
    for (auto& [sourceId, voice] : state.activeVoices) {
        if (!voice.clip || voice.clip->monoSamples.empty()) {
            ended.push_back(sourceId);
            continue;
        }

        const float pan = std::clamp(voice.pan, -1.0f, 1.0f);
        const float leftGain = 0.5f * (1.0f - pan) * voice.gain;
        const float rightGain = 0.5f * (1.0f + pan) * voice.gain;

        for (std::size_t frame = 0; frame < frames; ++frame) {
            if (voice.cursor >= voice.clip->monoSamples.size()) {
                if (voice.looping) {
                    voice.cursor = 0;
                } else {
                    ended.push_back(sourceId);
                    break;
                }
            }

            const float sample = voice.clip->monoSamples[voice.cursor++];
            scratch[frame * 2U + 0U] += sample * leftGain;
            scratch[frame * 2U + 1U] += sample * rightGain;
        }
    }

    for (const auto& id : ended) {
        state.activeVoices.erase(id);
    }

    for (float& sample : scratch) {
        sample = std::clamp(sample, -1.0f, 1.0f);
    }
}

} // namespace

bool SpatialAudioSystem::initialize(LogFn logger) {
    m_logger = std::move(logger);
    auto& state = backendState();

    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0U) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            if (m_logger) {
                m_logger(std::string("SpatialAudioSystem fallback to no-op (SDL audio init failed): ") + SDL_GetError());
            }
            m_initialized = true;
            return true;
        }
    }

    SDL_AudioSpec desired{};
    desired.freq = kDeviceSampleRate;
    desired.format = AUDIO_F32SYS;
    desired.channels = kDeviceChannels;
    desired.samples = 1024;
    desired.callback = nullptr;

    state.device = SDL_OpenAudioDevice(nullptr, 0, &desired, &state.deviceSpec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (state.device == 0) {
        if (m_logger) {
            m_logger(std::string("SpatialAudioSystem fallback to no-op (SDL audio unavailable): ") + SDL_GetError());
        }
    } else {
        SDL_PauseAudioDevice(state.device, 0);
        state.enabled = true;
        if (m_logger) {
            m_logger("SpatialAudioSystem initialized (SDL backend). WAV playback enabled.");
        }
    }

    m_initialized = true;
    return true;
}

void SpatialAudioSystem::shutdown() {
    auto& state = backendState();
    if (state.device != 0) {
        SDL_ClearQueuedAudio(state.device);
        SDL_CloseAudioDevice(state.device);
        state.device = 0;
    }
    state.enabled = false;
    state.clipCache.clear();
    state.activeVoices.clear();
    state.failedClipPaths.clear();

    m_emitters.clear();
    m_initialized = false;
}

void SpatialAudioSystem::preUpdate(float) {
}

void SpatialAudioSystem::update(float) {
    auto& state = backendState();
    if (!m_initialized || !state.enabled || state.device == 0) {
        return;
    }

    for (auto& [sourceId, voice] : state.activeVoices) {
        voice.seenThisFrame = false;
    }

    std::size_t fallbackCounter = 0;
    for (const auto& emitter : m_emitters) {
        if (emitter.clipPath.find("://") != std::string::npos) {
            continue;
        }

        const std::string normalizedPath = normalizeClipPath(emitter.clipPath);
        if (normalizedPath.empty()) {
            continue;
        }

        const std::string extLower = toLowerAscii(std::filesystem::path(normalizedPath).extension().string());
        if (extLower != ".wav") {
            if (state.failedClipPaths.insert(normalizedPath).second && m_logger) {
                m_logger("SpatialAudioSystem: only .wav playback is supported by the built-in backend, skipping: " + normalizedPath);
            }
            continue;
        }

        std::shared_ptr<DecodedClip> clip;
        auto clipIt = state.clipCache.find(normalizedPath);
        if (clipIt != state.clipCache.end()) {
            clip = clipIt->second;
        } else {
            if (!decodeWavToDeviceMono(normalizedPath, state.deviceSpec, clip)) {
                if (state.failedClipPaths.insert(normalizedPath).second && m_logger) {
                    m_logger("SpatialAudioSystem: failed to decode WAV clip: " + normalizedPath);
                }
                continue;
            }
            state.clipCache.emplace(normalizedPath, clip);
            state.failedClipPaths.erase(normalizedPath);
        }

        std::string voiceKey = emitter.sourceId;
        if (voiceKey.empty()) {
            voiceKey = normalizedPath + "#" + std::to_string(fallbackCounter++);
        }

        auto voiceIt = state.activeVoices.find(voiceKey);
        if (voiceIt == state.activeVoices.end()) {
            ActiveVoice voice{};
            voice.clip = clip;
            voice.looping = emitter.looping;
            voice.gain = 0.0f;
            voice.pan = 0.0f;
            voice.seenThisFrame = true;
            voiceIt = state.activeVoices.emplace(voiceKey, std::move(voice)).first;
        }

        ActiveVoice& voice = voiceIt->second;
        voice.clip = clip;
        voice.looping = emitter.looping;
        voice.gain = std::max(0.0f, emitter.gain) * computeDistanceAttenuation(emitter, m_listener);
        voice.pan = computePan(m_listener, emitter.position);
        voice.seenThisFrame = true;
    }

    std::vector<std::string> staleVoices;
    for (const auto& [sourceId, voice] : state.activeVoices) {
        if (!voice.seenThisFrame) {
            staleVoices.push_back(sourceId);
        }
    }
    for (const auto& sourceId : staleVoices) {
        state.activeVoices.erase(sourceId);
    }

    const std::size_t bytesPerFrame = sizeof(float) * kDeviceChannels;
    const std::size_t queuedBytes = static_cast<std::size_t>(SDL_GetQueuedAudioSize(state.device));
    std::size_t queuedFrames = queuedBytes / bytesPerFrame;

    std::vector<float> mixScratch;
    while (queuedFrames < kTargetQueuedFrames && !state.activeVoices.empty()) {
        mixChunk(state, mixScratch, kMixFramesPerChunk);
        if (!mixScratch.empty()) {
            const std::size_t byteCount = mixScratch.size() * sizeof(float);
            SDL_QueueAudio(state.device, mixScratch.data(), static_cast<Uint32>(byteCount));
        }
        queuedFrames += kMixFramesPerChunk;
    }
}

void SpatialAudioSystem::postUpdate(float) {
    // Frame-local emitter list mirrors a command-buffer style submit path.
    m_emitters.clear();
}

void SpatialAudioSystem::setListener(const AudioListenerState& listener) {
    m_listener = listener;
}

void SpatialAudioSystem::submitEmitter(const AudioEmitterState& emitter) {
    m_emitters.push_back(emitter);
}

void SpatialAudioSystem::clearEmitters() {
    m_emitters.clear();
}

} // namespace elysium
