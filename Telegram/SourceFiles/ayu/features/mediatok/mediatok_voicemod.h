// This is part of MediaTok (AyuGramPlus fork).
#pragma once

#include <array>
#include <cstdint>

namespace MediaTok::Voicemod {

enum class Preset : int {
	Off = 0,
	Deep = 1,
	Helium = 2,
	Robot = 3,
	Radio = 4,
	Echo = 5,
};

struct State {
	static constexpr int kPitchBufSize = 4096;
	static constexpr int kPitchBufMask = kPitchBufSize - 1;
	static constexpr int kEchoBufSize = 16384; // ~341ms @ 48kHz

	std::array<float, kPitchBufSize> pitchBuf{};
	int pitchWritePos = 0;
	double pitchPhase = 0.0;

	double modPhase = 0.0;
	float hpPrevIn = 0.0f;
	float hpPrevOut = 0.0f;
	float lpPrevOut = 0.0f;

	std::array<float, kEchoBufSize> echoBuf{};
	int echoPos = 0;

	void reset() {
		pitchBuf.fill(0.0f);
		pitchWritePos = 0;
		pitchPhase = 0.0;
		modPhase = 0.0;
		hpPrevIn = 0.0f;
		hpPrevOut = 0.0f;
		lpPrevOut = 0.0f;
		echoBuf.fill(0.0f);
		echoPos = 0;
	}
};

void ProcessSamples(short *samples, int count, int sampleRate, State &state);

} // namespace MediaTok::Voicemod
