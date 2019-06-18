#pragma once

#include <stdint.h>

// TODO: Swap, min, max ... MACROS???


#define Kilobytes(Value) ((Value) * 1024)
#define Megabytes(Value) (Kilobytes(Value) * 1024)
#define Gigabytes(Value) (Megabytes(Value) * 1024)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

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

// TODO: In the future , rendering _specifically_ will become a three-tire	d abstraction!!!
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

struct game_button_state {
	int HalfTransitionCount;
	bool EndedDown;
};

struct game_controller_input {
	
	bool IsAnalog;
	float StartX;
	float StartY;
	float MinX;
	float MinY;
	float MaxX;
	float MaxY;
	float EndX;
	float EndY;

	union {
		game_button_state Buttons[6];
		struct {
			game_button_state Up;
			game_button_state Down;
			game_button_state Left;
			game_button_state Right;
			game_button_state LeftShoulder;
			game_button_state RightShoulder;
		};
	};
};

struct game_input {
	game_controller_input Controllers[4];
};


struct game_memory{
	bool 		IsInitialized;
	uint64_t 	PermanentStorageSize;
	void*		PermanentStorage;		// NOTE: Required to be cleared to zero at startup

	uint64_t 	TransientStorageSize;
	void*		TransientStorage;		// NOTE: Required to be cleared to zero at startup


};

// Three things - timing, Controller/keyboard input, bitmap buffer to use, sound buffer to use
static void GameUpdateAndRender(game_memory *GameMemory, game_input* Input, game_offscreen_buffer* Buffer, game_sound_output_buffer* SoundBuffer);

////
///
//

struct game_state{
	int ToneHz;
	int GreenOffset;
	int BlueOffset;
};