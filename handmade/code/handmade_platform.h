#ifndef HANDMADE_PLATFORM_H
#define HANDMADE_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

//
// NOTE: Compilers
//
#ifndef COMPILER_MSVC 
#define COMPILER_MSVC 0
#endif

#ifndef COMPILER_LLVM
#define COMPILER_LLVM 0
#endif

#if !COMPILER_MSVC && !COMPILER_LLVM
// Determine compiler
	#if _MSC_VER 
		#undef COMPILER_MSVC
		#define COMPILER_MSVC 1
	#else
		// TODO: MOAR COMPILERS
		#undef COMPILER_LLVM
		#define COMPILER_LLVM 1
	#endif
#endif

#if COMPILER_MSVC
#include <intrin.h>
#endif

#define internal static
#define local_persist static
#define global_variable static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef size_t memory_index;

typedef float real32;
typedef double real64;

typedef struct
{
	int Placeholder;
} thread_context;

/*
    TODO: Services the platform layer provides to the game
*/

#if HANDMADE_INTERNAL
/*
	IMPORTANT
	
	These are not for doing anything in the shipping game -- DEBUG ONLY!
	They are blocking and write not lost data protected
*/
typedef struct 
{
	uint32 ContentsSize;
	void *Contents;	
} debug_read_file_result;

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(thread_context *Thread, void *Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(thread_context *Thread, char *Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name(thread_context *Thread, char *Filename, uint32 MemorySize, void *Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);
#endif


// Structures for game generics
typedef struct
{	
    // Pixels are 32 bits, memory order is BB GG RR XX
	void *Memory;
	int Width;
	int Height;
	int Pitch;	
	int BytesPerPixel;
} game_offscreen_buffer;

typedef struct 
{	
	int SamplesPerSecond;
	int SampleCount;
	int16 *Samples;
} game_sound_output_buffer;

typedef struct 
{
	int HalfTransitionCount;
	bool32 EndedDown;
} game_button_state;

typedef struct 
{
	bool32 IsAnalog;
	bool32 IsConnected;

	real32 StickAverageX;
	real32 StickAverageY;

	union
	{
		game_button_state Buttons[12];
		struct
		{
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

			// NOTE: Do not add buttons below start, will wreck assertion			
			game_button_state Start; 
		};
	};
} game_controller_input;

typedef struct 
{
	game_button_state MouseButtons[5];
	int32 MouseX, MouseY, MouseZ;

	real32 dtForFrame;
	game_controller_input Controllers[5];
} game_input;

typedef struct
{
	bool32 IsInitialized;

	uint64 PermanentStorageSize;
	void *PermanentStorage;

	uint64 TransientStorageSize;
	void *TransientStorage;

	debug_platform_free_file_memory* DEBUGPlatformFreeFileMemory;
	debug_platform_read_entire_file* DEBUGPlatformReadEntireFile;	
	debug_platform_write_entire_file* DEBUGPlatformWriteEntireFile;
} game_memory;

/*
    Services the game provides to platfrom layer
*/

#define GAME_UPDATE_AND_RENDER(name) void name(thread_context *Thread, game_memory *Memory, game_input *Input, game_offscreen_buffer *Buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define GAME_GET_SOUND_SAMPLES(name) void name(thread_context *Thread, game_memory *Memory, game_sound_output_buffer *SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

#ifdef __cplusplus
}
#endif

#endif