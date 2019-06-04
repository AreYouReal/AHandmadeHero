#include <stdint.h>
// TODO: Implement sine ourselves
#include <math.h>
// Handmade headers

#include "AHandmade.h"

static void
GameOutputSound(game_sound_output_buffer* SoundBuffer ) {
	static float tSine;
	int16_t ToneVolume = 3000; 
	int ToneHz = 256;
	int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

	int16_t* SampleOut = SoundBuffer->Samples;
	for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex) {
		real32 SineValue = sinf(tSine);
		int16_t SampleValue = (int16_t)(SineValue * ToneVolume);
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;
		tSine += 2.0f * Pi32 * 1.0f / (float)WavePeriod;
	}
}

static void
RenderWeirdGradinent(game_offscreen_buffer* Buffer, int BlueOffset, int GreenOffset) {
	// TODO: Let's see want the optimizer does
	uint8_t* Row = (uint8_t*)Buffer->memory;
	for (int y = 0; y < Buffer->height; ++y) {
		uint32_t* pixel = (uint32_t*)Row;
		for (int x = 0; x < Buffer->width; ++x) {
			/*
				Pixel in memory: RR GG BB xx
				LITTLE ENDIAN: 0x xxBBGGRR
				BIG ENDIAN: 0x RRGGBBxx
			*/
			uint8_t blue = x + BlueOffset;
			uint8_t green = y + GreenOffset;
			/*
				Memory: 	BB GG RR xx
				Register:	xx RR GG BB (little endian)
				Pixel (32-bits)
			*/

			*pixel++ = (green << 8 | blue);
		}
		Row += Buffer->pitch;
	}
}

void GameUpdateAndRender(game_offscreen_buffer* Buffer, int BlueOffset, int GreenOffset, game_sound_output_buffer* SoundBuffer){
	// TODO: Allow sample offsets here for more robust platform options
	GameOutputSound(SoundBuffer);
	RenderWeirdGradinent(Buffer, BlueOffset, GreenOffset);
}
