#pragma once


/*
	TODO: Services that the platform layer provides to the game
*/

/* 
	NOTE: Services that the game provides to the platform layer
	(this may expand in the future - sound on separate thread, etc.)
*/

#define ever (;;) // Just for fun %)
#define Pi32 3.1459265359f

typedef float real32;
typedef double real64;

// TODO: In the future , rendering _specifically_ will become a three-tired abstraction!!!
struct game_offscreen_buffer {
	// Pixels are always 32 bits wide. Little endian 0x xx RR GG BB
	void*		memory;
	int			width;
	int			height;
	int			pitch;
};

struct game_sound_output_buffer {
	int SamplesPerSecond;
	int SampleCount;
	int16_t* Samples;

};

// Three things - timing, Controller/keyboard input, bitmap buffer to use, sound buffer to use
static void GameUpdateAndRender(game_offscreen_buffer* Buffer, int BlueOffset, int GreenOffset, game_sound_output_buffer* SoundBuffer, int ToneHz);
