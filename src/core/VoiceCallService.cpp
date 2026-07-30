#include "core/VoiceCallService.h"

#include "platform/api/PlatformServices.h"

#include <Arduino.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
constexpr uint32_t PROFILE_MAGIC = 0x31434D53UL;  // SMC1
constexpr uint16_t PROFILE_VERSION = 2;
constexpr char NVS_NAMESPACE[] = "stickmon";
constexpr char PROFILE_KEY[] = "voice";
constexpr char TEMP_KEY[] = "voice_tmp";
constexpr size_t FFT_SIZE = 512;
constexpr size_t FRAME_SAMPLES = 400;
constexpr size_t FRAME_HOP = 320;
constexpr uint8_t MEL_FILTERS = 14;
constexpr uint8_t TABLE_BASE_FEATURES = 8;
constexpr uint32_t TABLE_SAMPLE_RATE = 16000;
constexpr float PI_F = 3.14159265358979323846f;

float hzToMel(float hz) {
    return 2595.0f * log10f(1.0f + hz / 700.0f);
}

float melToHz(float mel) {
    return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f);
}

struct FeatureTables {
    bool initialized = false;
    float window[FRAME_SAMPLES]{};
    uint16_t melBins[MEL_FILTERS + 2]{};
    float dct[TABLE_BASE_FEATURES][MEL_FILTERS]{};
    float fftStepReal[9]{};
    float fftStepImag[9]{};
};

FeatureTables& featureTables() {
    static FeatureTables tables;
    if (tables.initialized) return tables;
    for (size_t i = 0; i < FRAME_SAMPLES; ++i) {
        tables.window[i] = 0.54f -
            0.46f * cosf(2.0f * PI_F * i / (FRAME_SAMPLES - 1));
    }
    float melMin = hzToMel(300.0f);
    float melMax = hzToMel(7600.0f);
    for (uint8_t i = 0; i < MEL_FILTERS + 2; ++i) {
        float mel = melMin + (melMax - melMin) * i / (MEL_FILTERS + 1);
        size_t bin = static_cast<size_t>(
            floorf((FFT_SIZE + 1) * melToHz(mel) / TABLE_SAMPLE_RATE));
        tables.melBins[i] = static_cast<uint16_t>(
            std::min<size_t>(FFT_SIZE / 2, bin));
    }
    for (uint8_t c = 0; c < TABLE_BASE_FEATURES; ++c) {
        for (uint8_t m = 0; m < MEL_FILTERS; ++m) {
            tables.dct[c][m] = cosf(PI_F * (c + 1) * (m + 0.5f) / MEL_FILTERS);
        }
    }
    uint8_t stage = 0;
    for (size_t len = 2; len <= FFT_SIZE; len <<= 1, ++stage) {
        float angle = -2.0f * PI_F / static_cast<float>(len);
        tables.fftStepReal[stage] = cosf(angle);
        tables.fftStepImag[stage] = sinf(angle);
    }
    tables.initialized = true;
    return tables;
}

void fft(float* real, float* imag) {
    for (size_t i = 1, j = 0; i < FFT_SIZE; ++i) {
        size_t bit = FFT_SIZE >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            std::swap(real[i], real[j]);
            std::swap(imag[i], imag[j]);
        }
    }
    const FeatureTables& tables = featureTables();
    uint8_t stage = 0;
    for (size_t len = 2; len <= FFT_SIZE; len <<= 1, ++stage) {
        float wLenReal = tables.fftStepReal[stage];
        float wLenImag = tables.fftStepImag[stage];
        for (size_t i = 0; i < FFT_SIZE; i += len) {
            float wr = 1.0f;
            float wi = 0.0f;
            for (size_t j = 0; j < len / 2; ++j) {
                size_t u = i + j;
                size_t v = u + len / 2;
                float vr = real[v] * wr - imag[v] * wi;
                float vi = real[v] * wi + imag[v] * wr;
                real[v] = real[u] - vr;
                imag[v] = imag[u] - vi;
                real[u] += vr;
                imag[u] += vi;
                float nextWr = wr * wLenReal - wi * wLenImag;
                wi = wr * wLenImag + wi * wLenReal;
                wr = nextWr;
            }
        }
    }
}
}  // namespace

VoiceCallService& VoiceCallService::ins() {
    static VoiceCallService instance;
    return instance;
}

bool VoiceCallService::begin() {
    if (initialized_) return true;
    initialized_ = true;
    profileReady_ = loadProfile();
    Serial.printf("[VoiceCall] profile=%s bytes=%u\n",
                  profileReady_ ? "ready" : "missing",
                  static_cast<unsigned>(sizeof(VoiceProfile)));
    return true;
}

bool VoiceCallService::ensureBuffers() {
    if (!capture_) {
        capture_ = static_cast<int16_t*>(Platform::memory().allocate(
            ENROLL_SAMPLES * sizeof(int16_t), true));
    }
    if (!rolling_) {
        rolling_ = static_cast<int16_t*>(Platform::memory().allocate(
            ROLLING_SAMPLES * sizeof(int16_t), true));
    }
    return capture_ && rolling_;
}

uint32_t VoiceCallService::checksum(const uint8_t* data, size_t length) {
    uint32_t hash = 2166136261UL;
    for (size_t i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619UL;
    }
    return hash;
}

bool VoiceCallService::validProfile(const VoiceProfile& profile) const {
    if (profile.magic != PROFILE_MAGIC || profile.version != PROFILE_VERSION ||
        profile.templateCount != TEMPLATE_COUNT || profile.threshold == 0) {
        return false;
    }
    for (const auto& item : profile.templates) {
        if (item.frameCount < 4 || item.frameCount > MAX_FRAMES ||
            item.durationMs < 200 || item.durationMs > 1600 ||
            item.dynamicsQ == 0 || item.speechLikePercent > 100) {
            return false;
        }
    }
    return profile.checksum == checksum(reinterpret_cast<const uint8_t*>(&profile),
                                        offsetof(VoiceProfile, checksum));
}

bool VoiceCallService::loadProfile() {
    if (Platform::blobs().blobSize(NVS_NAMESPACE, PROFILE_KEY) !=
        sizeof(VoiceProfile)) return false;
    VoiceProfile loaded{};
    bool ok = Platform::blobs().readBlob(
        NVS_NAMESPACE, PROFILE_KEY, &loaded, sizeof(loaded));
    if (!ok || !validProfile(loaded)) return false;
    profile_ = loaded;
    return true;
}

bool VoiceCallService::saveCandidate() {
    candidate_.magic = PROFILE_MAGIC;
    candidate_.version = PROFILE_VERSION;
    candidate_.templateCount = TEMPLATE_COUNT;
    candidate_.reserved = 0;
    candidate_.checksum = checksum(reinterpret_cast<const uint8_t*>(&candidate_),
                                   offsetof(VoiceProfile, checksum));

    // A blob update is already atomic after nvs_commit(). Keeping a temporary
    // copy made re-enrollment require space for three profiles at once.
    Platform::blobs().removeBlob(NVS_NAMESPACE, TEMP_KEY);
    bool written = Platform::blobs().writeBlob(
        NVS_NAMESPACE, PROFILE_KEY, &candidate_, sizeof(candidate_));
    VoiceProfile verify{};
    size_t stored = Platform::blobs().blobSize(NVS_NAMESPACE, PROFILE_KEY);
    bool read = stored == sizeof(verify) && Platform::blobs().readBlob(
        NVS_NAMESPACE, PROFILE_KEY, &verify, sizeof(verify));
    bool valid = read && validProfile(verify);
    if (!written || stored != sizeof(verify) || !valid) {
        Serial.printf(
            "[VoiceCall] save failed stage=profile written=%u stored=%u read=%u valid=%u\n",
            written ? static_cast<unsigned>(sizeof(candidate_)) : 0U,
            static_cast<unsigned>(stored),
            read ? static_cast<unsigned>(sizeof(verify)) : 0U,
            valid ? 1 : 0);
        return false;
    }
    profile_ = candidate_;
    profileReady_ = true;
    Serial.printf("[VoiceCall] profile saved bytes=%u\n",
                  static_cast<unsigned>(sizeof(candidate_)));
    return true;
}

bool VoiceCallService::extractTemplate(const int16_t* samples, size_t count,
                                       VoiceTemplate& output, EnrollmentError& error,
                                       bool requireTrailingSilence) {
    error = EnrollmentError::NONE;
    if (!samples || count < FRAME_SAMPLES) {
        error = EnrollmentError::TOO_SHORT;
        return false;
    }

    constexpr size_t VAD_FRAME = 160;
    size_t vadCount = std::min<size_t>(count / VAD_FRAME, 200);
    static uint32_t rms[200];
    static uint32_t sorted[200];
    for (size_t f = 0; f < vadCount; ++f) {
        uint64_t sum = 0;
        for (size_t i = 0; i < VAD_FRAME; ++i) {
            int32_t value = samples[f * VAD_FRAME + i];
            sum += static_cast<uint64_t>(value * value);
        }
        rms[f] = static_cast<uint32_t>(sqrtf(static_cast<float>(sum / VAD_FRAME)));
        sorted[f] = rms[f];
    }
    std::sort(sorted, sorted + vadCount);
    uint32_t noise = sorted[vadCount / 4];
    uint32_t threshold = std::max<uint32_t>(450, noise * 5 / 2 + 120);
    size_t firstActive = vadCount;
    size_t lastActive = 0;
    uint32_t peak = 0;
    for (size_t f = 0; f < vadCount; ++f) {
        peak = std::max(peak, rms[f]);
        if (rms[f] >= threshold) {
            if (firstActive == vadCount) firstActive = f;
            lastActive = f;
        }
    }
    if (firstActive == vadCount || peak < 700) {
        error = EnrollmentError::TOO_QUIET;
        return false;
    }
    if (lastActive <= firstActive || (lastActive - firstActive + 1) * VAD_FRAME < 2400) {
        error = EnrollmentError::TOO_SHORT;
        return false;
    }
    if (requireTrailingSilence && lastActive + 10 >= vadCount) return false;
    if (noise > 5000 || threshold >= peak * 9 / 10) {
        error = EnrollmentError::TOO_NOISY;
        return false;
    }

    size_t start = firstActive > 4 ? (firstActive - 4) * VAD_FRAME : 0;
    size_t end = std::min(count, (lastActive + 5) * VAD_FRAME);
    if (end - start > ENROLL_SAMPLES) {
        size_t center = (start + end) / 2;
        start = center > ENROLL_SAMPLES / 2 ? center - ENROLL_SAMPLES / 2 : 0;
        end = std::min(count, start + ENROLL_SAMPLES);
    }
    size_t availableFrames = 1 + (end - start - FRAME_SAMPLES) / FRAME_HOP;
    uint8_t frameCount = static_cast<uint8_t>(std::min<size_t>(availableFrames, MAX_FRAMES));
    if (frameCount < 4) {
        error = EnrollmentError::TOO_SHORT;
        return false;
    }

    static float coefficients[MAX_FRAMES][BASE_FEATURE_COUNT];
    static float real[FFT_SIZE];
    static float imag[FFT_SIZE];
    static float power[FFT_SIZE / 2 + 1];
    memset(coefficients, 0, sizeof(coefficients));
    uint8_t speechLikeFrames = 0;
    const FeatureTables& tables = featureTables();

    for (uint8_t frame = 0; frame < frameCount; ++frame) {
        memset(real, 0, sizeof(real));
        memset(imag, 0, sizeof(imag));
        size_t frameStart = start + frame * FRAME_HOP;
        for (size_t i = 0; i < FRAME_SAMPLES; ++i) {
            real[i] = static_cast<float>(samples[frameStart + i]) *
                      tables.window[i] / 32768.0f;
        }
        fft(real, imag);
        for (size_t i = 0; i <= FFT_SIZE / 2; ++i) {
            power[i] = real[i] * real[i] + imag[i] * imag[i];
        }
        float logMel[MEL_FILTERS]{};
        float melEnergySum = 0.0f;
        float melLogSum = 0.0f;
        for (uint8_t m = 0; m < MEL_FILTERS; ++m) {
            float energy = 0.0f;
            uint16_t left = tables.melBins[m];
            uint16_t center = std::max<uint16_t>(left + 1, tables.melBins[m + 1]);
            uint16_t right = std::max<uint16_t>(center + 1, tables.melBins[m + 2]);
            right = std::min<uint16_t>(FFT_SIZE / 2, right);
            for (uint16_t k = left; k < center && k <= FFT_SIZE / 2; ++k) {
                energy += power[k] * static_cast<float>(k - left) / (center - left);
            }
            for (uint16_t k = center; k <= right; ++k) {
                energy += power[k] * static_cast<float>(right - k) / (right - center);
            }
            energy = std::max(energy, 1.0e-9f);
            logMel[m] = logf(energy);
            melEnergySum += energy;
            melLogSum += logMel[m];
        }
        float flatness = expf(melLogSum / MEL_FILTERS) /
                         std::max(melEnergySum / MEL_FILTERS, 1.0e-9f);
        if (flatness < 0.55f) speechLikeFrames++;
        for (uint8_t c = 0; c < BASE_FEATURE_COUNT; ++c) {
            float value = 0.0f;
            for (uint8_t m = 0; m < MEL_FILTERS; ++m) {
                value += logMel[m] * tables.dct[c][m];
            }
            coefficients[frame][c] = value;
        }
    }

    memset(&output, 0, sizeof(output));
    output.frameCount = frameCount;
    output.durationMs = static_cast<uint16_t>((end - start) * 1000UL / SAMPLE_RATE);
    output.speechLikePercent = static_cast<uint8_t>(
        static_cast<uint16_t>(speechLikeFrames) * 100U / frameCount);
    float dynamics = 0.0f;
    for (uint8_t f = 1; f < frameCount; ++f) {
        for (uint8_t c = 0; c < BASE_FEATURE_COUNT; ++c) {
            dynamics += fabsf(coefficients[f][c] - coefficients[f - 1][c]);
        }
    }
    dynamics /= std::max<uint16_t>(1, (frameCount - 1) * BASE_FEATURE_COUNT);
    output.dynamicsQ = static_cast<uint16_t>(std::min<float>(65535.0f,
                                                             dynamics * 16.0f));
    if (output.speechLikePercent < 25 || output.dynamicsQ < 8) {
        error = EnrollmentError::TOO_NOISY;
        return false;
    }

    for (uint8_t c = 0; c < BASE_FEATURE_COUNT; ++c) {
        float mean = 0.0f;
        for (uint8_t f = 0; f < frameCount; ++f) mean += coefficients[f][c];
        mean /= frameCount;
        output.spectralMeanQ[c] = static_cast<int16_t>(std::max<int>(
            -32760, std::min<int>(32760, static_cast<int>(lroundf(mean * 8.0f)))));
        for (uint8_t f = 0; f < frameCount; ++f) {
            int value = static_cast<int>(lroundf((coefficients[f][c] - mean) * 3.0f));
            output.features[f][c] = static_cast<int8_t>(std::max(-120, std::min(120, value)));
        }
    }
    for (uint8_t c = 0; c < FEATURE_COUNT - BASE_FEATURE_COUNT; ++c) {
        for (uint8_t f = 0; f < frameCount; ++f) {
            uint8_t previous = f == 0 ? 0 : f - 1;
            uint8_t next = f + 1 < frameCount ? f + 1 : frameCount - 1;
            float delta = (coefficients[next][c] - coefficients[previous][c]) * 3.0f;
            int value = static_cast<int>(lroundf(delta));
            output.features[f][BASE_FEATURE_COUNT + c] =
                static_cast<int8_t>(std::max(-120, std::min(120, value)));
        }
    }
    return true;
}

uint32_t VoiceCallService::dtwDistance(const VoiceTemplate& a, const VoiceTemplate& b) const {
    constexpr uint32_t INF = 0x3FFFFFFFUL;
    uint8_t shorter = std::min(a.frameCount, b.frameCount);
    uint8_t longer = std::max(a.frameCount, b.frameCount);
    if (static_cast<uint16_t>(longer) * 100U >
        static_cast<uint16_t>(shorter) * 145U) {
        return INF;
    }
    uint16_t quieterDynamics = std::min(a.dynamicsQ, b.dynamicsQ);
    uint16_t strongerDynamics = std::max(a.dynamicsQ, b.dynamicsQ);
    if (static_cast<uint32_t>(strongerDynamics) * 100UL >
        static_cast<uint32_t>(quieterDynamics) * 220UL) {
        return INF;
    }
    uint32_t previous[MAX_FRAMES + 1];
    uint32_t current[MAX_FRAMES + 1];
    std::fill(previous, previous + MAX_FRAMES + 1, INF);
    previous[0] = 0;
    uint8_t band = std::max<uint8_t>(5, longer / 6);
    for (uint8_t i = 1; i <= a.frameCount; ++i) {
        std::fill(current, current + MAX_FRAMES + 1, INF);
        uint8_t from = i > band ? i - band : 1;
        uint8_t to = std::min<uint8_t>(b.frameCount, i + band);
        for (uint8_t j = from; j <= to; ++j) {
            uint32_t frameCost = 0;
            for (uint8_t c = 0; c < FEATURE_COUNT; ++c) {
                int delta = static_cast<int>(a.features[i - 1][c]) - b.features[j - 1][c];
                frameCost += static_cast<uint32_t>(delta * delta);
            }
            uint32_t best = std::min(previous[j], std::min(current[j - 1], previous[j - 1]));
            if (best < INF - frameCost) current[j] = best + frameCost;
        }
        memcpy(previous, current, sizeof(previous));
    }
    uint32_t total = previous[b.frameCount];
    if (total >= INF) return INF;
    uint32_t normalized = total / std::max<uint16_t>(1, a.frameCount + b.frameCount);
    uint32_t meanCost = 0;
    for (uint8_t c = 0; c < BASE_FEATURE_COUNT; ++c) {
        int delta = (static_cast<int>(a.spectralMeanQ[c]) - b.spectralMeanQ[c]) / 4;
        meanCost += static_cast<uint32_t>(delta * delta);
    }
    int speechLikeDelta = static_cast<int>(a.speechLikePercent) - b.speechLikePercent;
    return normalized + meanCost / BASE_FEATURE_COUNT +
           static_cast<uint32_t>(speechLikeDelta * speechLikeDelta) * 2U;
}

bool VoiceCallService::profileMatches(const VoiceTemplate& sample) const {
    if (!profileReady_) return false;
    uint32_t distance[TEMPLATE_COUNT]{};
    for (uint8_t i = 0; i < TEMPLATE_COUNT; ++i) {
        distance[i] = dtwDistance(sample, profile_.templates[i]);
    }
    std::sort(distance, distance + TEMPLATE_COUNT);
    uint32_t score = (distance[0] + distance[1]) / 2;
    Serial.printf("[VoiceCall] score=%lu threshold=%lu distances=%lu,%lu,%lu "
                  "frames=%u dynamics=%u speech=%u%%\n",
                  static_cast<unsigned long>(score),
                  static_cast<unsigned long>(profile_.threshold),
                  static_cast<unsigned long>(distance[0]),
                  static_cast<unsigned long>(distance[1]),
                  static_cast<unsigned long>(distance[2]),
                  sample.frameCount, sample.dynamicsQ, sample.speechLikePercent);
    return score <= profile_.threshold &&
           distance[1] <= profile_.threshold * 6UL / 5UL;
}

bool VoiceCallService::beginEnrollment(uint32_t nowMs) {
    begin();
    stopListening();
    if (!ensureBuffers()) {
        enrollmentState_ = EnrollmentState::ERROR;
        enrollmentError_ = EnrollmentError::NO_MEMORY;
        return false;
    }
    if (!Platform::audio().beginMicrophone()) {
        enrollmentState_ = EnrollmentState::ERROR;
        enrollmentError_ = EnrollmentError::MICROPHONE;
        return false;
    }
    memset(&candidate_, 0, sizeof(candidate_));
    enrollmentTake_ = 0;
    enrollmentError_ = EnrollmentError::NONE;
    enrollmentState_ = EnrollmentState::PREPARING;
    stateStartedMs_ = nowMs;
    memset(waveform_, 0, sizeof(waveform_));
    return true;
}

void VoiceCallService::startEnrollmentTake(uint32_t nowMs) {
    memset(capture_, 0, ENROLL_SAMPLES * sizeof(int16_t));
    memset(waveform_, 0, sizeof(waveform_));
    if (!Platform::audio().recordMicrophone(capture_, ENROLL_SAMPLES, SAMPLE_RATE)) {
        enrollmentState_ = EnrollmentState::ERROR;
        enrollmentError_ = EnrollmentError::MICROPHONE;
        finishAudioMode();
        return;
    }
    enrollmentState_ = EnrollmentState::RECORDING;
    stateStartedMs_ = nowMs;
}

void VoiceCallService::updateWaveform(uint32_t nowMs) {
    size_t filled = std::min<size_t>(ENROLL_SAMPLES,
        static_cast<size_t>(nowMs - stateStartedMs_) * SAMPLE_RATE / 1000);
    if (filled < WAVEFORM_POINTS) return;
    size_t stride = std::max<size_t>(1, filled / WAVEFORM_POINTS);
    for (uint8_t p = 0; p < WAVEFORM_POINTS; ++p) {
        size_t begin = p * stride;
        size_t end = std::min(filled, begin + stride);
        int peak = 0;
        for (size_t i = begin; i < end; i += 4) peak = std::max(peak, abs(capture_[i]));
        waveform_[p] = static_cast<int8_t>(std::min(30, peak / 800));
    }
}

void VoiceCallService::updateEnrollment(uint32_t nowMs) {
    switch (enrollmentState_) {
    case EnrollmentState::PREPARING:
        if (nowMs - stateStartedMs_ >= 650) startEnrollmentTake(nowMs);
        return;
    case EnrollmentState::RECORDING:
        updateWaveform(nowMs);
        if (Platform::audio().microphoneRecording()) return;
        break;
    case EnrollmentState::BETWEEN_TAKES:
        if (nowMs - stateStartedMs_ >= 650) {
            enrollmentState_ = EnrollmentState::PREPARING;
            stateStartedMs_ = nowMs;
        }
        return;
    case EnrollmentState::ERROR:
        if (nowMs - stateStartedMs_ >= 1400 && Platform::audio().microphoneActive()) {
            enrollmentError_ = EnrollmentError::NONE;
            enrollmentState_ = EnrollmentState::PREPARING;
            stateStartedMs_ = nowMs;
        }
        return;
    default:
        return;
    }

    EnrollmentError error = EnrollmentError::NONE;
    if (!extractTemplate(capture_, ENROLL_SAMPLES, candidate_.templates[enrollmentTake_], error)) {
        Serial.printf("[VoiceCall] enrollment take=%u rejected error=%u\n",
                      enrollmentTake_ + 1, static_cast<unsigned>(error));
        enrollmentError_ = error;
        enrollmentState_ = EnrollmentState::ERROR;
        stateStartedMs_ = nowMs;
        return;
    }
    Serial.printf("[VoiceCall] enrollment take=%u accepted frames=%u duration=%u "
                  "dynamics=%u speech=%u%%\n",
                  enrollmentTake_ + 1,
                  candidate_.templates[enrollmentTake_].frameCount,
                  candidate_.templates[enrollmentTake_].durationMs,
                  candidate_.templates[enrollmentTake_].dynamicsQ,
                  candidate_.templates[enrollmentTake_].speechLikePercent);
    enrollmentTake_++;
    if (enrollmentTake_ < TEMPLATE_COUNT) {
        enrollmentState_ = EnrollmentState::BETWEEN_TAKES;
        stateStartedMs_ = nowMs;
        return;
    }

    uint32_t pairwise[3] = {
        dtwDistance(candidate_.templates[0], candidate_.templates[1]),
        dtwDistance(candidate_.templates[0], candidate_.templates[2]),
        dtwDistance(candidate_.templates[1], candidate_.templates[2]),
    };
    std::sort(pairwise, pairwise + 3);
    if (pairwise[2] >= 25000) {
        enrollmentTake_ = 0;
        memset(&candidate_, 0, sizeof(candidate_));
        enrollmentError_ = EnrollmentError::INCONSISTENT;
        enrollmentState_ = EnrollmentState::ERROR;
        stateStartedMs_ = nowMs;
        return;
    }
    candidate_.threshold = std::min<uint32_t>(25000,
        std::max<uint32_t>(500, pairwise[2] * 13 / 10 + 150));
    if (!saveCandidate()) {
        enrollmentError_ = EnrollmentError::SAVE_FAILED;
        enrollmentState_ = EnrollmentState::ERROR;
        stateStartedMs_ = nowMs;
        finishAudioMode();
        return;
    }
    Serial.printf("[VoiceCall] enrolled threshold=%lu pair=%lu,%lu,%lu\n",
                  static_cast<unsigned long>(candidate_.threshold),
                  static_cast<unsigned long>(pairwise[0]),
                  static_cast<unsigned long>(pairwise[1]),
                  static_cast<unsigned long>(pairwise[2]));
    enrollmentState_ = EnrollmentState::SUCCESS;
    enrollmentError_ = EnrollmentError::NONE;
    stateStartedMs_ = nowMs;
    finishAudioMode();
}

void VoiceCallService::finishAudioMode() {
    Platform::audio().endMicrophone();
}

void VoiceCallService::cancelEnrollment() {
    if (enrollmentState_ == EnrollmentState::IDLE) return;
    finishAudioMode();
    enrollmentState_ = EnrollmentState::IDLE;
    enrollmentError_ = EnrollmentError::NONE;
    enrollmentTake_ = 0;
    memset(waveform_, 0, sizeof(waveform_));
}

void VoiceCallService::resetRolling() {
    rollingCount_ = 0;
    if (rolling_) memset(rolling_, 0, ROLLING_SAMPLES * sizeof(int16_t));
}

void VoiceCallService::resetListeningVad() {
    vadNoiseRms_ = 450;
    vadActiveFrames_ = 0;
    vadSilenceFrames_ = 0;
    vadSpeechSeen_ = false;
}

int8_t VoiceCallService::updateListeningVad(const int16_t* samples, size_t count) {
    constexpr size_t VAD_FRAME_SAMPLES = 160;
    constexpr uint16_t MIN_ACTIVE_FRAMES = 15;
    constexpr uint16_t END_SILENCE_FRAMES = 10;
    if (!samples || count < VAD_FRAME_SAMPLES) return 0;

    size_t frameCount = count / VAD_FRAME_SAMPLES;
    for (size_t frame = 0; frame < frameCount; ++frame) {
        uint64_t sum = 0;
        for (size_t i = 0; i < VAD_FRAME_SAMPLES; ++i) {
            int32_t value = samples[frame * VAD_FRAME_SAMPLES + i];
            sum += static_cast<uint64_t>(value * value);
        }
        uint32_t rms = static_cast<uint32_t>(
            sqrtf(static_cast<float>(sum / VAD_FRAME_SAMPLES)));
        uint32_t threshold = std::max<uint32_t>(450, vadNoiseRms_ * 5 / 2 + 120);
        if (rms >= threshold) {
            if (vadActiveFrames_ < 0xFFFF) vadActiveFrames_++;
            vadSilenceFrames_ = 0;
            if (vadActiveFrames_ >= 3) vadSpeechSeen_ = true;
            continue;
        }

        if (!vadSpeechSeen_) {
            vadActiveFrames_ = 0;
            vadNoiseRms_ = (vadNoiseRms_ * 15UL + rms) / 16UL;
            continue;
        }
        if (vadSilenceFrames_ < 0xFFFF) vadSilenceFrames_++;
        if (vadSilenceFrames_ >= END_SILENCE_FRAMES) {
            return vadActiveFrames_ >= MIN_ACTIVE_FRAMES ? 1 : -1;
        }
    }
    return 0;
}

bool VoiceCallService::startListening(uint32_t nowMs) {
    begin();
    if (listening_) return true;
    if (!profileReady_ || !ensureBuffers() || enrollmentState_ != EnrollmentState::IDLE ||
        !Platform::audio().beginMicrophone()) {
        return false;
    }
    resetRolling();
    resetListeningVad();
    matchPending_ = false;
    listening_ = true;
    listenCapturePending_ = Platform::audio().recordMicrophone(
        capture_, LISTEN_CHUNK_SAMPLES, SAMPLE_RATE);
    listenCaptureStartedMs_ = nowMs;
    if (!listenCapturePending_) stopListening();
    cooldownUntilMs_ = nowMs + 900;
    return listening_;
}

void VoiceCallService::appendListeningChunk() {
    if (rollingCount_ + LISTEN_CHUNK_SAMPLES > ROLLING_SAMPLES) {
        size_t keep = ROLLING_SAMPLES - LISTEN_CHUNK_SAMPLES;
        memmove(rolling_, rolling_ + (rollingCount_ - keep), keep * sizeof(int16_t));
        rollingCount_ = keep;
    }
    memcpy(rolling_ + rollingCount_, capture_, LISTEN_CHUNK_SAMPLES * sizeof(int16_t));
    rollingCount_ += LISTEN_CHUNK_SAMPLES;
}

void VoiceCallService::updateListening(uint32_t nowMs) {
    if (!listening_ || matchPending_) return;
    // Mic_Class marks its queue active from a worker task. Waiting for most of
    // the block duration avoids mistaking that short scheduling gap for EOF.
    if (listenCapturePending_ && nowMs - listenCaptureStartedMs_ < LISTEN_CHUNK_MS - 40) return;
    if (listenCapturePending_ && Platform::audio().microphoneRecording()) return;
    if (listenCapturePending_) {
        appendListeningChunk();
        listenCapturePending_ = false;
        int8_t vadResult = updateListeningVad(capture_, LISTEN_CHUNK_SAMPLES);
        if (vadResult > 0 && (int32_t)(nowMs - cooldownUntilMs_) >= 0 &&
            rollingCount_ >= LISTEN_CHUNK_SAMPLES * 2) {
            VoiceTemplate sample{};
            EnrollmentError error = EnrollmentError::NONE;
            uint32_t recognizeStartedMs = Platform::clock().millis();
            if (extractTemplate(rolling_, rollingCount_, sample, error, true)) {
                bool matched = profileMatches(sample);
                Serial.printf("[VoiceCall] recognize cost=%lums matched=%u\n",
                              static_cast<unsigned long>(Platform::clock().millis() -
                                                         recognizeStartedMs),
                              matched ? 1 : 0);
                if (matched) {
                    matchPending_ = true;
                    cooldownUntilMs_ = nowMs + 5000;
                    stopListening();
                    return;
                }
            }
        }
        if (vadResult != 0) {
            // Completed and rejected utterances, as well as short transients,
            // must not remain available for a second recognition pass.
            resetRolling();
            resetListeningVad();
        }
    }
    memset(capture_, 0, LISTEN_CHUNK_SAMPLES * sizeof(int16_t));
    listenCapturePending_ = Platform::audio().recordMicrophone(
        capture_, LISTEN_CHUNK_SAMPLES, SAMPLE_RATE);
    listenCaptureStartedMs_ = nowMs;
    if (!listenCapturePending_) stopListening();
}

void VoiceCallService::stopListening() {
    if (!listening_) return;
    listening_ = false;
    listenCapturePending_ = false;
    finishAudioMode();
}

bool VoiceCallService::consumeMatch() {
    bool matched = matchPending_;
    matchPending_ = false;
    return matched;
}

void VoiceCallService::clearDetectionQueue(uint32_t nowMs) {
    stopListening();
    matchPending_ = false;
    listenCapturePending_ = false;
    resetRolling();
    resetListeningVad();
    if (capture_) memset(capture_, 0, ENROLL_SAMPLES * sizeof(int16_t));
    cooldownUntilMs_ = nowMs + 900;
}

void VoiceCallService::clearCachedProfile() {
    stopListening();
    cancelEnrollment();
    memset(&profile_, 0, sizeof(profile_));
    memset(&candidate_, 0, sizeof(candidate_));
    profileReady_ = false;
    initialized_ = true;
}
