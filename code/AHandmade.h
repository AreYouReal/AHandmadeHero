#pragma once

#include <stdint.h>

/*
// NOTE: 
	HANDMADE_INTERNAL:
		0 - for public release
		1 - build for developer only

	HANDMADE_SLOW:
		0 - Not slow code allowed!
		1 - slow code welcome
 */

#if HANDMADE_SLOW
// TODO: Complete assertion macro - don't worry everyone !
#define Assert(Expression) if(!(Expression)){ * (int *) 0 = 0; }
#else
#define Assert(Expression)
#endif

#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
// TODO: Swap, min, max ... MACROS???

inline uint32_t 
SafeTruncateUInt64(uint64_t Value){
	// TODO: Defines for maximum values
	Assert(Value <= 0xFFFFFFFF);
	uint32_t Result = (uint32_t)Value;
	return(Result);
}

/*
	TODO: Services that the platform layer provides to the game
*/

#if HANDMADE_INTERNAL
/*
	IMPORTANT: 

	These are not for doing anything in the shipping coding - they are
	blocking and the write doesn't protect from the lost data!
 */

struct debug_read_file_result{
	uint32_t 	ContentsSize;
	void* 		Contents;
};

static debug_read_file_result 	DEBUGPlatformReadEntireFile(char* FileName);
static void 					DEBUGPlatformFreeFileMemory(void *Memory);
static bool 					DEBUGPlatformWriteEntireFile(char* FileName, uint32_t MemorySize, void* Memory);
#endif

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
	
	bool	IsConnected;
	bool 	IsAnalog;
	float 	StickAverageX;
	float 	StickAverageY;

	union {
		game_button_state Buttons[12];
		struct {
			game_button_state MoveUp;
			game_button_state MoveDown;
			game_button_state MoveLeft;
			game_button_state MoveRight;

			game_button_state ActionUp;
			game_button_state ActionDown;
			game_button_state ActionLeft;
			game_button_state ActionRight;
			
			game_button_state LeftShoulder;
			game_button_state RightShoulder;

			game_button_state Back;
			game_button_state Start;

			// NOTE: All buttons must be added above this line
			game_button_state Terminator;
		};
	};
};

struct game_input {
	// TODO: Insert clock values here.
	game_controller_input Controllers[5];
};

inline game_controller_input *GetController(game_input* Input, int unsigned ControllerIndex){
	Assert(ControllerIndex < ArrayCount(Input->Controllers));
	return(&Input->Controllers[ControllerIndex]);
}

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