#pragma once

#include <cstddef>
#include <cstdint>

class VoiceCallService {
public:
    enum class EnrollmentState : uint8_t {
        IDLE,
        PREPARING,
        RECORDING,
        BETWEEN_TAKES,
        SUCCESS,
        ERROR,
    };

    enum class EnrollmentError : uint8_t {
        NONE,
        MICROPHONE,
        TOO_QUIET,
        TOO_SHORT,
        TOO_NOISY,
        INCONSISTENT,
        SAVE_FAILED,
        NO_MEMORY,
    };

    static VoiceCallService& ins();

    bool begin();
    bool profileReady() const { return profileReady_; }

    bool beginEnrollment(uint32_t nowMs);
    void cancelEnrollment();
    void updateEnrollment(uint32_t nowMs);
    EnrollmentState enrollmentState() const { return enrollmentState_; }
    EnrollmentError enrollmentError() const { return enrollmentError_; }
    uint8_t enrollmentTake() const { return enrollmentTake_ + 1; }
    const int8_t* waveform() const { return waveform_; }
    size_t waveformSize() const { return WAVEFORM_POINTS; }

    bool startListening(uint32_t nowMs);
    void stopListening();
    void updateListening(uint32_t nowMs);
    bool listening() const { return listening_; }
    bool consumeMatch();
    void clearDetectionQueue(uint32_t nowMs);

    void clearCachedProfile();

private:
    VoiceCallService() = default;

    static constexpr uint32_t SAMPLE_RATE = 16000;
    static constexpr size_t ENROLL_SAMPLES = 24000;
    static constexpr size_t LISTEN_CHUNK_SAMPLES = 4000;
    static constexpr uint32_t LISTEN_CHUNK_MS =
        LISTEN_CHUNK_SAMPLES * 1000 / SAMPLE_RATE;
    static constexpr size_t ROLLING_SAMPLES = 32000;
    static constexpr uint8_t TEMPLATE_COUNT = 3;
    static constexpr uint8_t BASE_FEATURE_COUNT = 8;
    static constexpr uint8_t FEATURE_COUNT = 12;
    static constexpr uint8_t MAX_FRAMES = 72;
    static constexpr uint8_t WAVEFORM_POINTS = 32;

    struct VoiceTemplate {
        uint8_t frameCount;
        uint8_t reserved;
        uint16_t durationMs;
        uint16_t dynamicsQ;
        uint8_t speechLikePercent;
        uint8_t reserved2;
        int16_t spectralMeanQ[BASE_FEATURE_COUNT];
        int8_t features[MAX_FRAMES][FEATURE_COUNT];
    };

    struct VoiceProfile {
        uint32_t magic;
        uint16_t version;
        uint8_t templateCount;
        uint8_t reserved;
        uint32_t threshold;
        VoiceTemplate templates[TEMPLATE_COUNT];
        uint32_t checksum;
    };

    bool ensureBuffers();
    bool loadProfile();
    bool saveCandidate();
    bool validProfile(const VoiceProfile& profile) const;
    static uint32_t checksum(const uint8_t* data, size_t length);
    bool extractTemplate(const int16_t* samples, size_t count, VoiceTemplate& output,
                         EnrollmentError& error, bool requireTrailingSilence = false);
    uint32_t dtwDistance(const VoiceTemplate& a, const VoiceTemplate& b) const;
    bool profileMatches(const VoiceTemplate& sample) const;
    void startEnrollmentTake(uint32_t nowMs);
    void updateWaveform(uint32_t nowMs);
    void appendListeningChunk();
    void resetRolling();
    void resetListeningVad();
    int8_t updateListeningVad(const int16_t* samples, size_t count);
    void finishAudioMode();

    VoiceProfile profile_{};
    VoiceProfile candidate_{};
    bool initialized_ = false;
    bool profileReady_ = false;
    int16_t* capture_ = nullptr;
    int16_t* rolling_ = nullptr;
    size_t rollingCount_ = 0;

    EnrollmentState enrollmentState_ = EnrollmentState::IDLE;
    EnrollmentError enrollmentError_ = EnrollmentError::NONE;
    uint8_t enrollmentTake_ = 0;
    uint32_t stateStartedMs_ = 0;
    int8_t waveform_[WAVEFORM_POINTS]{};

    bool listening_ = false;
    bool listenCapturePending_ = false;
    uint32_t listenCaptureStartedMs_ = 0;
    bool matchPending_ = false;
    uint32_t cooldownUntilMs_ = 0;
    uint32_t vadNoiseRms_ = 450;
    uint16_t vadActiveFrames_ = 0;
    uint16_t vadSilenceFrames_ = 0;
    bool vadSpeechSeen_ = false;
};
