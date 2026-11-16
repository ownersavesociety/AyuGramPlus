// This is part of MediaTok (AyuGramPlus fork).
#include "ayu/features/mediatok/mediatok_voicemod.h"

#include "ayu/ayu_settings.h"

#include <algorithm>
#include <cmath>

namespace MediaTok::Voicemod {
namespace {

constexpr double kTwoPi = 6.28318530717958647692;
constexpr int kGrainSize = 2048;

inline float ReadLinear(const std::array<float, State::kPitchBufSize> &buf, double pos) {
	while (pos < 0.0) {
		pos += State::kPitchBufSize;
	}
	const int idx0 = static_cast<int>(pos) & State::kPitchBufMask;
	const int idx1 = (idx0 + 1) & State::kPitchBufMask;
	const float frac = static_cast<float>(pos - std::floor(pos));
	return buf[idx0] * (1.0f - frac) + buf[idx1] * frac;
}

inline float ProcessPitchShift(float in, double ratio, State &s) {
	s.pitchBuf[s.pitchWritePos & State::kPitchBufMask] = in;

	double phase1 = s.pitchPhase;
	double phase2 = phase1 + 0.5;
	if (phase2 >= 1.0) {
		phase2 -= 1.0;
	}

	const double d1 = phase1 * kGrainSize;
	const double d2 = phase2 * kGrainSize;

	const float s1 = ReadLinear(s.pitchBuf, static_cast<double>(s.pitchWritePos) - d1);
	const float s2 = ReadLinear(s.pitchBuf, static_cast<double>(s.pitchWritePos) - d2);

	const float w1 = static_cast<float>(1.0 - std::abs(2.0 * phase1 - 1.0));
	const float w2 = 1.0f - w1;

	s.pitchWritePos = (s.pitchWritePos + 1) & State::kPitchBufMask;
	s.pitchPhase += (1.0 - ratio) / static_cast<double>(kGrainSize);
	while (s.pitchPhase >= 1.0) {
		s.pitchPhase -= 1.0;
	}
	while (s.pitchPhase < 0.0) {
		s.pitchPhase += 1.0;
	}

	return s1 * w1 + s2 * w2;
}

} // namespace

void ProcessSamples(short *samples, int count, int sampleRate, State &state) {
	if (!samples || count <= 0 || sampleRate <= 0) {
		return;
	}
	const int presetInt = AyuSettings::getInstance().voicemodPreset();
	const auto preset = static_cast<Preset>(presetInt);
	if (preset == Preset::Off) {
		return;
	}

	const int echoDelay = std::clamp(sampleRate / 6, 1024, State::kEchoBufSize - 1);

	for (int i = 0; i < count; ++i) {
		const float in = static_cast<float>(samples[i]) / 32768.0f;
		float out = in;

		switch (preset) {
		case Preset::Deep: {
			out = ProcessPitchShift(in, 0.72, state);
		} break;

		case Preset::Helium: {
			out = ProcessPitchShift(in, 1.38, state);
		} break;

		case Preset::Robot: {
			const float carrier = static_cast<float>(std::sin(state.modPhase));
			state.modPhase += (kTwoPi * 55.0) / static_cast<double>(sampleRate);
			if (state.modPhase >= kTwoPi) {
				state.modPhase -= kTwoPi;
			}
			const int combDelay = std::clamp(sampleRate / 160, 32, 1024);
			const int readIdx = (state.echoPos - combDelay + State::kEchoBufSize) % State::kEchoBufSize;
			const float comb = in * 0.65f + state.echoBuf[readIdx] * 0.45f;
			state.echoBuf[state.echoPos] = comb;
			state.echoPos = (state.echoPos + 1) % State::kEchoBufSize;
			out = (comb * 0.65f + (comb * carrier) * 0.60f);
		} break;

		case Preset::Radio: {
			// High-pass ~450 Hz
			const float hpAlpha = 0.94f;
			const float hp = hpAlpha * (state.hpPrevOut + in - state.hpPrevIn);
			state.hpPrevIn = in;
			state.hpPrevOut = hp;
			// Low-pass ~2800 Hz
			const float lpAlpha = 0.30f;
			state.lpPrevOut = state.lpPrevOut + lpAlpha * (hp - state.lpPrevOut);
			// Soft overdrive saturation
			out = std::tanh(state.lpPrevOut * 3.2f) * 0.88f;
		} break;

		case Preset::Echo: {
			const int readIdx = (state.echoPos - echoDelay + State::kEchoBufSize) % State::kEchoBufSize;
			const float delayed = state.echoBuf[readIdx];
			out = in * 0.85f + delayed * 0.55f;
			state.echoBuf[state.echoPos] = in + delayed * 0.42f;
			state.echoPos = (state.echoPos + 1) % State::kEchoBufSize;
		} break;

		default:
			break;
		}

		const float clamped = std::clamp(out, -0.999f, 0.999f);
		samples[i] = static_cast<short>(clamped * 32767.0f);
	}
}

} // namespace MediaTok::Voicemod
