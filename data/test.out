#include <stdint.h>
// TODO: Implement sine ourselves
#include <math.h>
// Handmade headers

#include "AHandmade.h"

static void
GameOutputSound(game_sound_output_buffer* SoundBuffer, int ToneHz ) {
	static float tSine;
	int16_t ToneVolume = 3000; 
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
			uint8_t blue = uint8_t(x + BlueOffset);
			uint8_t green = uint8_t(y + GreenOffset);
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

void GameUpdateAndRender(game_memory* GameMemory, game_input* Input, game_offscreen_buffer* Buffer, game_sound_output_buffer* SoundBuffer){

	Assert(sizeof(game_state) <= GameMemory->PermanentStorageSize);

	game_state* GameState = (game_state*)GameMemory->PermanentStorage;
	if(!GameMemory->IsInitialized){
		char* FileName = __FILE__;

		debug_read_file_result Result = DEBUGPlatformReadEntireFile(FileName);
		if(Result.ContentsSize > 0 && Result.Contents){
			DEBUGPlatformWriteEntireFile( "test.out", Result.ContentsSize, Result.Contents );
			DEBUGPlatformFreeFileMemory(Result.Contents);
		}

		GameState->ToneHz 			= 256;
		GameMemory->IsInitialized 	= true;
	}

	game_controller_input* Input0 = &Input->Controllers[0];
	if (Input0->IsAnalog) {
		// TODO: Use analog movement tuning
		GameState->BlueOffset += (int)(4.0f * (Input0->EndX));
		GameState->ToneHz = 256 + (int)(128.0f * (Input0->EndY));
	} else {
		// TODO: Use digital movement tuning
	}

	if (Input0->Down.EndedDown) {
		++GameState->GreenOffset;
	}




	// TODO: Allow sample offsets here for more robust platform options
	GameOutputSound(SoundBuffer, GameState->ToneHz);
	RenderWeirdGradinent(Buffer, GameState->BlueOffset, GameState->GreenOffset);
}
