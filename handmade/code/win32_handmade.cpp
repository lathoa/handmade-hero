/*
	Windows platform for handmade hero stream by Casey Muratori.
	Walkthrough build for learning about game engine platforms.

	Author: Andrew Frank
	Created: 3/13/2023
*/

/*
	TODO: Not a final platform layer

	- Saved game locations
	- Getting a handle to own executable file
	- Asset loading path
	- Threading (launch a thread)
	- RawInput (support multiple keyboards)	
	- ClipCursor() for multimonitor support
	- WM_SETCURSOR (control cursor visibility)
	- QueryCancelAutoplay
	- WM_ACTIVATEAPP
	- Blit speed improvements
	- Hardware acceleration (OpenGL or Direct3D)
	- GetKeyboardLayout
	- ChangeDislpaySettings if we detect a slow blit

	Partial list of stuff to do...

*/
#include <windows.h>
#include <malloc.h>
#include <xinput.h>
#include <dsound.h>

#include "handmade.h"
#include "win32_handmade.h"

global_variable bool GlobalRunning;
global_variable bool GlobalPause;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCountFrequency;
global_variable bool32 DEBUGGlobalShowCursor;
global_variable WINDOWPLACEMENT GlobalWindowPosition = {sizeof(GlobalWindowPosition)};

internal void ToggleFullscreen(HWND Window)
{
	// NOTE: This follows Raymond Chen's prescription for fullscreen toggling
	// From TheOldNewThing
	DWORD Style = GetWindowLong(Window, GWL_STYLE);
	if(Style & WS_OVERLAPPEDWINDOW)
	{
		MONITORINFO MonitorInfo = {sizeof(MonitorInfo)};
		if(GetWindowPlacement(Window, &GlobalWindowPosition) && 
			GetMonitorInfo(MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY), &MonitorInfo))
		{
			SetWindowLong(Window, GWL_STYLE, Style & ~WS_OVERLAPPEDWINDOW);
			SetWindowPos(Window, HWND_TOP, 
						MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
						MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
						MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
						SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}
	}
	else
	{
		SetWindowLong(Window, GWL_STYLE, Style | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(Window, &GlobalWindowPosition);
		SetWindowPos(Window, NULL, 0, 0, 0, 0, 
					SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | 
					SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	}
}

/*
  Process for loading XInput get and set state functions.
  1. #define a function definition macro
  2. typedef the function so a pointer to it can be created
  3. Create a function stub that does nothing
  4. Create a global variable that is the pointer to the funtion
  5. #define the original function name to our pointer name
*/
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// Same process done with XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
	if (Memory)
	{
		VirtualFree(Memory, 0, MEM_RELEASE);
	}
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
	debug_read_file_result Result = {};

	HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0,
									OPEN_EXISTING, 0, 0);
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER FileSize;
		if (GetFileSizeEx(FileHandle, &FileSize))
		{
			uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
			Result.Contents = VirtualAlloc(0, FileSize32, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
			if (Result.Contents)
			{
				DWORD BytesRead;
				if (ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) && (FileSize32 == BytesRead))
				{
					Result.ContentsSize = BytesRead;
				}
				else
				{
					DEBUGPlatformFreeFileMemory(Thread, Result.Contents);
					Result.Contents = 0;
				}
			}
			else
			{
				// TODO logging
			}
		}
		else
		{
			// TODO logging
		}
		CloseHandle(FileHandle);
	}
	else
	{
		// TODO Logging
	}

	return Result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{

	bool32 Result = false;
	HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0,
									CREATE_ALWAYS, 0, 0);
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		DWORD BytesWritten;
		if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
		{
			Result = (BytesWritten == MemorySize);
		}
		else
		{
			// TODO Logging
		}
		CloseHandle(FileHandle);
	}
	else
	{
		// TODO Logging
	}

	return Result;
}

inline FILETIME Win32GetLastWriteTime(char *Filename)
{
	FILETIME LastWriteTime = {};
	WIN32_FILE_ATTRIBUTE_DATA Data;
	if(GetFileAttributesEx(Filename, GetFileExInfoStandard, &Data))
	{
		LastWriteTime = Data.ftLastWriteTime;
	}

	return LastWriteTime;
}

internal win32_game_code Win32LoadGameCode(char *SourceDLLName, char *TempDLLName)
{
	win32_game_code Result = {};		

	Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceDLLName);
	CopyFileA(SourceDLLName, TempDLLName, FALSE);
	Result.GameCodeDLL = LoadLibraryA(TempDLLName);
	if(Result.GameCodeDLL)
	{
		Result.UpdateAndRender = (game_update_and_render *)GetProcAddress(Result.GameCodeDLL, "GameUpdateAndRender");
		Result.GetSoundSamples = (game_get_sound_samples *)GetProcAddress(Result.GameCodeDLL, "GameGetSoundSamples");
		
		Result.IsValid = (Result.UpdateAndRender && Result.GetSoundSamples);
	}

	if(!Result.IsValid)
	{
		Result.UpdateAndRender = 0;
		Result.GetSoundSamples = 0;
	}

	return Result;
}

internal void Win32UnloadGameCode(win32_game_code *GameCode)
{
	if(GameCode->GameCodeDLL)
	{
		FreeLibrary(GameCode->GameCodeDLL);
		GameCode->GameCodeDLL = 0;
	}	
	GameCode->IsValid = false;
	GameCode->UpdateAndRender = 0;
	GameCode->GetSoundSamples = 0;
}

internal void Win32LoadXInput(void)
{
	// TODO test this on Windows 8
	// TODO diagnostic on which version is running
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
	if (!XInputLibrary)
	{
		XInputLibrary = LoadLibraryA("xinput1_3.dll");
	}
	if (!XInputLibrary)
	{
		XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
	}

	if (XInputLibrary)
	{
		XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
		XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
	}
}

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
	// Load the library
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

	if (DSoundLibrary)
	{
		// TODO double check that this works on XP, directsound 8 or 7
		direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
		LPDIRECTSOUND DirectSound;
		if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
		{
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplesPerSecond;
			WaveFormat.wBitsPerSample = 16;
			WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nBlockAlign * SamplesPerSecond;
			WaveFormat.cbSize = 0;

			if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
			{
				// "Create" a primary buffer
				DSBUFFERDESC BufferDescription = {};
				BufferDescription.dwSize = sizeof(BufferDescription);
				// TODO DSBCAPS_GLOBALFOCUS flag
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
				{
					if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
					{
						// Finally set the format!
						OutputDebugStringA("Primary buffer was set!\n");
					}
					else
					{
						// TODO diagnostic
					}
				}
				else
				{
					// TODO diagnostic
				}
			}
			else
			{
				// TODO diagnostic buffer format
			}

			// Create a secondary buffer (what we write to)
			DSBUFFERDESC BufferDescription = {};
			BufferDescription.dwSize = sizeof(BufferDescription);
			BufferDescription.dwFlags = 0;
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat = &WaveFormat;

			if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0)))
			{
				OutputDebugStringA("Secondary buffer created successfully\n");
			}
			else
			{
				// TODO diagnostic
			}
		}
		else
		{
			// TODO diagnostic
		}
	}
	else
	{
		// TODO diagnostic
	}
}

internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;
	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;
	return Result;
}

// NOTE: DIB = device independent bitmap
internal void
win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{

	if (Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}

	Buffer->Width = Width;
	Buffer->Height = Height;
	Buffer->BytesPerPixel = 4;

	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Width;
	Buffer->Info.bmiHeader.biHeight = -Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	int BitmapMemorySize = Buffer->BytesPerPixel * Width * Height;
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	Buffer->Pitch = Width * Buffer->BytesPerPixel;
}

internal void
win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer, HDC DeviceContext, int WindowWidth, int WindowHeight)
{		
	// TODO: Centering / black bars?
	if((WindowWidth >= Buffer->Width*2) && (WindowHeight >= Buffer->Height*2))
	{
		StretchDIBits(DeviceContext,
		0, 0, 2*Buffer->Width, 2*Buffer->Height,
		0, 0, Buffer->Width, Buffer->Height,
		Buffer->Memory,
		&Buffer->Info,
		DIB_RGB_COLORS, SRCCOPY);
	}
	else
	{
		int OffsetX = 10;
		int OffsetY = 10;
		PatBlt(DeviceContext, 0, 0, WindowWidth, OffsetY, BLACKNESS);
		PatBlt(DeviceContext, 0, 0, OffsetX, WindowHeight, BLACKNESS);
		PatBlt(DeviceContext, 0, OffsetY + Buffer->Height, WindowWidth, WindowHeight, BLACKNESS);
		PatBlt(DeviceContext, OffsetX + Buffer->Width, 0, WindowWidth, WindowHeight, BLACKNESS);	

		StretchDIBits(DeviceContext,
					OffsetX, OffsetY, Buffer->Width, Buffer->Height,
					0, 0, Buffer->Width, Buffer->Height,
					Buffer->Memory,
					&Buffer->Info,
					DIB_RGB_COLORS, SRCCOPY);
	}
}

internal void Win32ClearSoundBuffer(win32_sound_output *SoundOutput)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;
	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
											  &Region1, &Region1Size,
											  &Region2, &Region2Size,
											  0)))
	{
		uint8 *DestSample = (uint8 *)Region1;
		for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ByteIndex++)
		{
			*DestSample++ = 0;
		}
		DestSample = (uint8 *)Region2;
		for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ByteIndex++)
		{
			*DestSample++ = 0;
		}
	}
	GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
}

internal int StringLength(char *String)
{
	int Result = 0;
	while(*String++)
	{
		Result++;
	}
	return Result;
}

internal void CatStrings(size_t SourceACount, char *SourceA, 
				size_t SourceBCount, char *SourceB, 
				size_t DestCount, char *Dest)
{
	for(int Index = 0; Index < SourceACount; Index++)
	{
		*Dest++ = *SourceA++;
	}
	for(int Index = 0; Index < SourceBCount; Index++)
	{
		*Dest++ = *SourceB++;
	}
	*Dest++ = 0;
}

internal void Win32BuildEXEPathFilename(win32_state *State, char *Filename, int DestCount, char *Dest)
{
	CatStrings(State->OnePastLastEXEFilenameSlash - State->EXEFilename, State->EXEFilename, 
					StringLength(Filename), Filename,
					DestCount, Dest);
}

internal void Win32GetInputFileLocation(win32_state *State, bool32 InputStream, int SlotIndex, int DestCount, char *Dest)
{	
	char Temp[64];
	if(InputStream)
	{
		wsprintf(Temp, "loop_edit_%d_%s.hmi", SlotIndex, InputStream ? "input" : "state");
	}	
	Win32BuildEXEPathFilename(State, Temp, DestCount, Dest);
}

internal win32_replay_buffer *Win32GetReplayBuffer(win32_state *State, unsigned int Index)
{
	Assert(Index < ArrayCount(State->ReplayBuffers));
	win32_replay_buffer *Result = &State->ReplayBuffers[Index];
	return Result;
}

internal void Win32BeginRecordingInput(win32_state *Win32State, int InputRecordingIndex)
{
	win32_replay_buffer *ReplayBuffer = Win32GetReplayBuffer(Win32State, InputRecordingIndex);
	if(ReplayBuffer->MemoryBlock)
	{
		Win32State->InputRecordingIndex = InputRecordingIndex;			
		
		char Filename[WIN32_STATE_FILE_NAME_COUNT];
		Win32GetInputFileLocation(Win32State, true, InputRecordingIndex, WIN32_STATE_FILE_NAME_COUNT, Filename);
		Win32State->RecordingHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

#if 0
		LARGE_INTEGER FilePosition;
		FilePosition.QuadPart = Win32State->TotalSize;
		SetFilePointerEx(Win32State->RecordingHandle, FilePosition, 0, FILE_BEGIN);
#endif
		
		CopyMemory(ReplayBuffer->MemoryBlock, Win32State->GameMemoryBlock, Win32State->TotalSize);
	}	
}

internal void Win32EndRecordingInput(win32_state *Win32State)
{
	CloseHandle(Win32State->RecordingHandle);	
	Win32State->InputRecordingIndex = 0;
}

internal void Win32RecordInput(win32_state *Win32State, game_input *NewInput)
{
	DWORD BytesWritten;
	WriteFile(Win32State->RecordingHandle, NewInput, sizeof(*NewInput), &BytesWritten, 0);	
}

internal void Win32BeginInputPlayback(win32_state *Win32State, int InputPlaybackIndex)
{
	win32_replay_buffer *ReplayBuffer = Win32GetReplayBuffer(Win32State, InputPlaybackIndex);
	if(ReplayBuffer)
	{
		Win32State->InputPlaybackIndex = InputPlaybackIndex;	
		char Filename[WIN32_STATE_FILE_NAME_COUNT];
		Win32GetInputFileLocation(Win32State, true, InputPlaybackIndex, WIN32_STATE_FILE_NAME_COUNT, Filename);
		Win32State->PlaybackHandle = CreateFileA(Filename, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);

#if 0
		LARGE_INTEGER FilePosition;
		FilePosition.QuadPart = Win32State->TotalSize;
		SetFilePointerEx(Win32State->PlaybackHandle, FilePosition, 0, FILE_BEGIN);		
#endif

		CopyMemory(Win32State->GameMemoryBlock, ReplayBuffer->MemoryBlock, Win32State->TotalSize);
	}	
}

internal void Win32EndInputPlayback(win32_state *Win32State)
{
	CloseHandle(Win32State->PlaybackHandle);
	Win32State->InputPlaybackIndex = 0;
}

internal void Win32PlayBackInput(win32_state *Win32State, game_input *NewInput)
{
	DWORD BytesRead = 0;
	if(ReadFile(Win32State->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0))
	{
	if(BytesRead != sizeof(*NewInput))		
		{
			// NOTE: hit the end of the stream, go back to beginning
			int PlayingIndex = Win32State->InputPlaybackIndex;
			Win32EndInputPlayback(Win32State);
			Win32BeginInputPlayback(Win32State, PlayingIndex);
		}
	}
}

internal void Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock,
								   DWORD BytesToWrite, game_sound_output_buffer *SourceBuffer)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;
	HRESULT LockResult = GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
													 &Region1, &Region1Size,
													 &Region2, &Region2Size,
													 0);
	if (SUCCEEDED(LockResult))
	{
		// TODO assert that Region1Size/Region2Size is even
		DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
		int16 *DestSample = (int16 *)Region1;
		int16 *SourceSample = SourceBuffer->Samples;
		for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; SampleIndex++)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}
		DestSample = (int16 *)Region2;
		DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
		for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; SampleIndex++)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}
	}
	GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
}

internal void Win32ProcessXInputDigitalButton(DWORD XInputButtonState, game_button_state *OldState, DWORD ButtonBit,
											  game_button_state *NewState)
{
	NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
	NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal void Win32ProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown)
{
	// No key repeats
	if(NewState->EndedDown != IsDown){
		NewState->EndedDown = IsDown;
		++NewState->HalfTransitionCount;
	}	
}

LRESULT CALLBACK win32MainWindowCallback(
	HWND Window,
	UINT Message,
	WPARAM wParam,
	LPARAM lParam)
{

	LRESULT Result = 0;

	/* Using case blocks localize variables initialized within a case */
	switch (Message)
	{
	case WM_SIZE:
	{
	}
	break;

	case WM_DESTROY:
	{
		// TODO: handle as error, recreate window
		GlobalRunning = false;
	}
	break;

	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP:
	{
		Assert(!"Keyboard input came through a non dispatch message");
	}
	break;

	case WM_CLOSE:
	{
		// TODO: handle with a message to the user
		GlobalRunning = false;
	}
	break;

	case WM_SETCURSOR:
	{
		if(!DEBUGGlobalShowCursor)
		{		
			SetCursor(0);
		}
		
	} break;

	case WM_ACTIVATEAPP:
	{
		OutputDebugStringA("WM_ACTIVATEAPP\n");
	}
	break;

	case WM_PAINT:
	{
		PAINTSTRUCT Paint;
		HDC DeviceContext = BeginPaint(Window, &Paint);
		win32_window_dimension Dimension = Win32GetWindowDimension(Window);
		win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);
		EndPaint(Window, &Paint);
	}
	break;

	default:
	{
		// OutputDebugStringA("default\n");
		Result = DefWindowProc(Window, Message, wParam, lParam);
	}
	break;
	}

	return Result;
}

internal void Win32ProcessPendingMessages(win32_state *Win32State, game_controller_input *KeyboardController)
{
	MSG Message;
	while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
	{
		switch (Message.message)
		{
			case WM_QUIT:
			{
				GlobalRunning = false;
			}
			break;

			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_KEYDOWN:
			case WM_KEYUP:
			{
				uint32 VKCode = (uint32)Message.wParam;
				bool WasDown = ((Message.lParam & (1 << 30)) != 0);
				bool IsDown = ((Message.lParam >> 31) == 0);

				if (WasDown != IsDown)
				{
					if (VKCode == 'W')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
					}
					else if (VKCode == 'A')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
					}
					else if (VKCode == 'S')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
					}
					else if (VKCode == 'D')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
					}
					else if (VKCode == 'Q')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
					}
					else if (VKCode == 'E')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
					}
					else if (VKCode == VK_UP)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
					}
					else if (VKCode == VK_LEFT)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
					}
					else if (VKCode == VK_DOWN)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
					}
					else if (VKCode == VK_RIGHT)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
					}
					else if (VKCode == VK_ESCAPE)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
					}
					else if (VKCode == VK_SPACE)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
					}
#if HANDMADE_INTERNAL					
					else if (VKCode == 'P')
					{
						if(IsDown){
							GlobalPause = !GlobalPause;
						}
					}
					else if(VKCode == 'L')
					{
						if(IsDown)
						{
							if(Win32State->InputPlaybackIndex == 0)
							{
								if(Win32State->InputRecordingIndex == 0)
								{
									Win32BeginRecordingInput(Win32State, 1);
								}
								else
								{
									Win32EndRecordingInput(Win32State);
									Win32BeginInputPlayback(Win32State, 1);
								}
							}	
							else
							{
								Win32EndInputPlayback(Win32State);
							}						
						}
					}
#endif					
					
				}
				// Handle Alt+F4 case ourselves
				if(IsDown)
				{
					bool32 AltKeyWasDown = Message.lParam & (1 << 29);
					if ((VKCode == VK_F4) && AltKeyWasDown)
					{
						GlobalRunning = false;
					}
					if((VKCode == VK_RETURN) && AltKeyWasDown)
					{
						if(Message.hwnd)
						{
							ToggleFullscreen(Message.hwnd);
						}					
					}
				}
			}
			break;

			default:
			{
				TranslateMessage(&Message);
				DispatchMessage(&Message);
			}
			break;
		}
	}
}

inline LARGE_INTEGER Win32GetWallClock(void)
{
	LARGE_INTEGER Result;
	QueryPerformanceCounter(&Result);
	return Result;
}

inline real32 Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
	real32 Result = ((real32)(End.QuadPart - Start.QuadPart) / (real32)GlobalPerfCountFrequency);
	return Result;
}

internal real32 Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold)
{
	real32 Result = 0;
	if (Value < -DeadZoneThreshold)
	{
		Result = (real32)((Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold));
	}
	else if (Value > DeadZoneThreshold)
	{
		Result = (real32)((Value - DeadZoneThreshold) / (32767.0f - DeadZoneThreshold));
	}
	return Result;
}

internal void Win32DebugDrawVertical(win32_offscreen_buffer *Buffer, int X, int Upper, int Lower, uint32 Color)
{	
	if(Upper >= Buffer->Height){Upper = Buffer->Height-1;}
	if(Lower < 0){Lower = 0;}

	if((X > 0) && (X < Buffer->Width))
	{
		uint8 *Pixel = (uint8 *)Buffer->Memory + (X*Buffer->BytesPerPixel) + (Lower*Buffer->Pitch);
		for(int Y = Lower; Y < Upper; Y++)
		{
			*(uint32 *)Pixel = Color;
			Pixel += Buffer->Pitch;
		}
	}
}

inline void Win32DrawSoundBufferMarker(win32_offscreen_buffer *Buffer, win32_sound_output *SoundOutput,
										real32 C, int PadX, int Upper, int Lower, DWORD Value, uint32 Color)
{		
		int X = (int)(PadX + (C * (real32)Value));
		Win32DebugDrawVertical(Buffer, X, Upper, Lower, Color);				
}

#if 0
internal void Win32DebugSyncDisplay(win32_offscreen_buffer *Buffer, win32_sound_output *SoundOutput, 
									int MarkerCount, win32_debug_time_marker *Markers,
									int CurrentMarkerIndex, real32 TargetSecondsPerFrame)
{
	Assert(CurrentMarkerIndex < MarkerCount);

	int PadX = 16;
	int PadY = 16;
	int LineHeight = 64;	
	if(CurrentMarkerIndex < 0) {CurrentMarkerIndex++;}

	real32 C = (real32)(Buffer->Width - 2*PadX) / (real32)SoundOutput->SecondaryBufferSize;
	for(int MarkerIndex = 0; MarkerIndex < MarkerCount; MarkerIndex++)
	{		
		win32_debug_time_marker *ThisMarker = &Markers[MarkerIndex];
		Assert(ThisMarker->OutputPlayCursor < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->OutputWriteCursor < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->OutputLocation < SoundOutput->SecondaryBufferSize);
		//Assert(ThisMarker->OutputByteCount < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->FlipPlayCursor < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->FlipPlayCursor < SoundOutput->SecondaryBufferSize);

		int Top = PadY;
		int Bottom = PadY + LineHeight;
		DWORD PlayColor = 0xFFFFFFFF;
		DWORD WriteColor = 0xFFFF0000;
		DWORD ExpectedFlipColor = 0xFFFFFF00;
		DWORD PlayWindowColor = 0xFFFF00FF;


		if(MarkerIndex == CurrentMarkerIndex)
		{
			Top += (LineHeight);
			Bottom += (LineHeight);
			int FirstTop = Top;

			Win32DrawSoundBufferMarker(Buffer, SoundOutput, C, PadX, Bottom, Top, ThisMarker->OutputPlayCursor, PlayColor);
			Win32DrawSoundBufferMarker(Buffer, SoundOutput, C, PadX, Bottom, Top, ThisMarker->OutputWriteCursor, WriteColor);			

			Top += (LineHeight);
			Bottom += (LineHeight);
			Win32DrawSoundBufferMarker(Buffer, SoundOutput, C, PadX, Bottom, Top, ThisMarker->OutputLocation, PlayColor);
			Win32DrawSoundBufferMarker(Buffer, SoundOutput, C, PadX, Bottom, Top, ThisMarker->OutputLocation + ThisMarker->OutputByteCount, WriteColor);			

			Top += (LineHeight);
			Bottom += (LineHeight);
			Win32DrawSoundBufferMarker(Buffer, SoundOutput, C, PadX, Bottom, FirstTop, ThisMarker->ExpectedFlipPlayCursor, ExpectedFlipColor);
		}		
		Win32DrawSoundBufferMarker(Buffer, SoundOutput, C, PadX, Bottom, Top, ThisMarker->FlipPlayCursor, PlayColor);
		Win32DrawSoundBufferMarker(Buffer, SoundOutput, C, PadX, Bottom, Top, ThisMarker->FlipPlayCursor - (SoundOutput->BytesPerSample * 480), PlayWindowColor);
		Win32DrawSoundBufferMarker(Buffer, SoundOutput, C, PadX, Bottom, Top, ThisMarker->FlipPlayCursor + (SoundOutput->BytesPerSample * 480), PlayWindowColor);
		Win32DrawSoundBufferMarker(Buffer, SoundOutput, C, PadX, Bottom, Top, ThisMarker->FlipWriteCursor, WriteColor);
	}
}
#endif


internal void Win32GetEXEFilename(win32_state *State)
{	
	DWORD SizeOfFilename = GetModuleFileNameA(0, State->EXEFilename, sizeof(State->EXEFilename));
	State->OnePastLastEXEFilenameSlash = State->EXEFilename;
	for(char *Scan = State->EXEFilename; *Scan; ++Scan)
	{
		if(*Scan == '\\')
		{
			State->OnePastLastEXEFilenameSlash = Scan + 1;
		}
	}	
}

int CALLBACK WinMain(
	HINSTANCE Instance,
	HINSTANCE PrevInstance,
	LPSTR CommandLine,
	int ShowCode)
{	
	win32_state Win32State = {};	

	LARGE_INTEGER PerfCountFrequencyResult;
	QueryPerformanceFrequency(&PerfCountFrequencyResult);
	GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

	// Set the Windows scheduler granularity to 1 ms for more granular sleep
	UINT DesiredSchedulerMS = 1;
	bool32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);
	
	Win32GetEXEFilename(&Win32State);

	char GameCodeDLLPath[WIN32_STATE_FILE_NAME_COUNT];
	Win32BuildEXEPathFilename(&Win32State, "handmade.dll", WIN32_STATE_FILE_NAME_COUNT, GameCodeDLLPath);
	
	char TempGameCodeDLLPath[WIN32_STATE_FILE_NAME_COUNT];					
	Win32BuildEXEPathFilename(&Win32State, "handmade_temp.dll", WIN32_STATE_FILE_NAME_COUNT, TempGameCodeDLLPath);					

	Win32LoadXInput();


#if HANDMADE_INTERNAL
	DEBUGGlobalShowCursor = true;
#else
	DEBUGGlobalShowCursor = false;
#endif

	WNDCLASSA WindowClass = {};

	// NOTE: 1080p display mode is 1920x1080
	// NOTE: Reduced by 4x for software renderer (960x540)
	win32ResizeDIBSection(&GlobalBackbuffer, 960, 540);

	WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	WindowClass.lpfnWndProc = win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	// WindowClass.hCursor = LoadCursor(0, IDC_CROSS);
	// WindowClass.hIcon;
	WindowClass.lpszClassName = "HandmadeHeroWindowClass";	

	if (RegisterClass(&WindowClass))
	{
		HWND Window = CreateWindowEx(
			0, //WS_EX_TOPMOST,
			WindowClass.lpszClassName,
			"Handmade Hero",
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
		    CW_USEDEFAULT,
			0, 0,
			Instance,
			0);

		if (Window)
		{
			HDC DeviceContext = GetDC(Window);							
			int MonitorRefreshHz = 60;
			int Win32RefreshRate = GetDeviceCaps(DeviceContext, VREFRESH);
			if(Win32RefreshRate > 1)
			{
				MonitorRefreshHz = Win32RefreshRate;
			}
			real32 GameUpdateHz = (MonitorRefreshHz/2.0f);
			real32 TargetSecondsPerFrame = 1.0f / (GameUpdateHz);

			win32_sound_output SoundOutput = {};

			SoundOutput.SamplesPerSecond = 48000;			
			SoundOutput.BytesPerSample = sizeof(int16) * 2;			
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;			
			// TODO compute this variance and see what the lowest reasonable value is
			SoundOutput.SafetyBytes = (int)((((real32)SoundOutput.SamplesPerSecond*(real32)SoundOutput.BytesPerSample) / GameUpdateHz) / 3.0f);						

			Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample);
			Win32ClearSoundBuffer(&SoundOutput);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			GlobalRunning = true;			
			Win32State.InputRecordingIndex = 0;
			Win32State.InputPlaybackIndex = 0;			

			// TODO pool with bitmap virtualalloc
			int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

#if HANDMADE_INTERNAL
			LPVOID BaseAddress = (LPVOID)Terabytes((uint64)2);
#else
			LPVOID BaseAddress = (LPVOID)0;
#endif

			game_memory GameMemory = {};
			GameMemory.PermanentStorageSize = Megabytes(64);
			GameMemory.TransientStorageSize = Gigabytes((uint64)1);
			GameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
			GameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
			GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile; 

			Win32State.TotalSize = GameMemory.TransientStorageSize + GameMemory.PermanentStorageSize;
			Win32State.GameMemoryBlock = VirtualAlloc(BaseAddress, (size_t)Win32State.TotalSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
			GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
			GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);

			for(int ReplayIndex = 0; ReplayIndex < ArrayCount(Win32State.ReplayBuffers); ReplayIndex++)
			{
				win32_replay_buffer *ReplayBuffer = &Win32State.ReplayBuffers[ReplayIndex];
								
				Win32GetInputFileLocation(&Win32State, ReplayIndex, false,
											 sizeof(ReplayBuffer->Filename), ReplayBuffer->Filename);
				LARGE_INTEGER MaxSize;
				MaxSize.QuadPart = Win32State.TotalSize;				
				ReplayBuffer->FileHandle = CreateFileA(ReplayBuffer->Filename, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
				ReplayBuffer->MemoryMap = CreateFileMappingA(ReplayBuffer->FileHandle, 0, PAGE_READWRITE,
															MaxSize.HighPart, MaxSize.LowPart, 0);
				ReplayBuffer->MemoryBlock = MapViewOfFile(ReplayBuffer->MemoryMap, FILE_MAP_ALL_ACCESS, 0, 0, Win32State.TotalSize);
				
				if(ReplayBuffer->MemoryBlock)
				{
					
				}
				else
				{
					// TODO diagnostic
				}				
			}

			if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
			{
				game_input Input[2] = {};
				game_input *NewInput = &Input[0];
				game_input *OldInput = &Input[1];				
				
				LARGE_INTEGER LastCounter = Win32GetWallClock();
				LARGE_INTEGER FlipWallClock = Win32GetWallClock();

				int DebugMarkerIndex = 0;
				win32_debug_time_marker DebugMarkers[30] = {};

				DWORD AudioLatencyBytes = 0;
				real32 AudioLatencySeconds = 0.0f;
				bool32 SoundIsValid = false;				

				int64 LastCycleCount = __rdtsc();
				
				char *SourceDLLName = "handmade.dll";
				win32_game_code Game = Win32LoadGameCode(GameCodeDLLPath, TempGameCodeDLLPath);

				while (GlobalRunning)
				{
					NewInput->dtForFrame = TargetSecondsPerFrame;

					FILETIME NewDLLWriteTime = Win32GetLastWriteTime(SourceDLLName);
					if(CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) != 0)
					{
						Win32UnloadGameCode(&Game);
						Game = Win32LoadGameCode(GameCodeDLLPath, TempGameCodeDLLPath);						
						if(!Game.IsValid)
						{
							OutputDebugStringA("Loaded game code is invalid!\n");
						}
					}

					game_controller_input *OldKeyboardController = GetController(OldInput, 0);
					game_controller_input *NewKeyboardController = GetController(NewInput, 0);
					game_controller_input ZeroController = {};
					*NewKeyboardController = ZeroController;	
					NewKeyboardController->IsConnected = true;					

					for (int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ButtonIndex++)
					{
						NewKeyboardController->Buttons[ButtonIndex].EndedDown =
							OldKeyboardController->Buttons[ButtonIndex].EndedDown;
					}
					Win32ProcessPendingMessages(&Win32State, NewKeyboardController);

					// IMPORTANT: This executes a global pause, restarts the while loop
					if(GlobalPause) continue;	


					POINT MouseP;
					GetCursorPos(&MouseP);
					ScreenToClient(Window, &MouseP);
					win32_window_dimension Dimension = Win32GetWindowDimension(Window);
					NewInput->MouseX = MouseP.x;
					NewInput->MouseY = Dimension.Height - MouseP.y;
					NewInput->MouseZ = 0;	 // TODO: Support mousewheel?									
					Win32ProcessKeyboardMessage(&NewInput->MouseButtons[0], GetKeyState(VK_LBUTTON) & (1 << 15));
					Win32ProcessKeyboardMessage(&NewInput->MouseButtons[1], GetKeyState(VK_MBUTTON) & (1 << 15));
					Win32ProcessKeyboardMessage(&NewInput->MouseButtons[2], GetKeyState(VK_RBUTTON) & (1 << 15));
					Win32ProcessKeyboardMessage(&NewInput->MouseButtons[3], GetKeyState(VK_XBUTTON1) & (1 << 15));
					Win32ProcessKeyboardMessage(&NewInput->MouseButtons[4], GetKeyState(VK_XBUTTON2) & (1 << 15));					

					// TODO should we poll this more frequently? Need to not poll disconnected controllers
					DWORD MaxControllerCount = XUSER_MAX_COUNT;
					if (MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1))
					{
						MaxControllerCount = (ArrayCount(NewInput->Controllers) - 1);
					}
					for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount; ControllerIndex++)
					{
						XINPUT_STATE ControllerState;
						DWORD OurControllerIndex = ControllerIndex + 1;
						game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
						game_controller_input *NewController = GetController(NewInput, OurControllerIndex);

						if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
						{	
							NewController->IsConnected = true;	
							NewController->IsAnalog = OldController->IsAnalog;

							XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;	
														
							NewController->StickAverageX = OldController->StickAverageX;
							NewController->StickAverageY = OldController->StickAverageY;

							NewController->StickAverageX = Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
							NewController->StickAverageY = Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);													 
							if((NewController->StickAverageX != 0.0f) || (NewController->StickAverageY != 0.0f))
							{
								NewController->IsAnalog = true;
							}
							
							if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
							{
								NewController->StickAverageY = 1.0f;
								NewController->IsAnalog = false;
							}
							if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
							{
								NewController->StickAverageY = -1.0f;
								NewController->IsAnalog = false;
							}
							if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
							{
								NewController->StickAverageX = -1.0f;
								NewController->IsAnalog = false;
							}
							if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
							{
								NewController->StickAverageX = 1.0f;
								NewController->IsAnalog = false;
							}

							real32 Threshold = 0.5f;							
							Win32ProcessXInputDigitalButton((NewController->StickAverageX < -Threshold) ? 1 : 0, 
															&OldController->MoveLeft, 1, &NewController->MoveLeft);
							Win32ProcessXInputDigitalButton((NewController->StickAverageX > Threshold) ? 1 : 0, 
															&OldController->MoveRight, 1, &NewController->MoveRight);
							Win32ProcessXInputDigitalButton((NewController->StickAverageY < -Threshold) ? 1 : 0, 
															&OldController->MoveDown, 1, &NewController->MoveDown);
							Win32ProcessXInputDigitalButton((NewController->StickAverageY > Threshold) ? 1 : 0, 
															&OldController->MoveUp, 1, &NewController->MoveUp);																							

							Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionDown,
															XINPUT_GAMEPAD_A, &NewController->ActionDown);
							Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionRight,
															XINPUT_GAMEPAD_B, &NewController->ActionRight);
							Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionLeft,
															XINPUT_GAMEPAD_X, &NewController->ActionLeft);
							Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionUp,
															XINPUT_GAMEPAD_Y, &NewController->ActionUp);

							Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->LeftShoulder,
															XINPUT_GAMEPAD_LEFT_SHOULDER, &NewController->LeftShoulder);
							Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->RightShoulder,
															XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder);

							Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Start,
															XINPUT_GAMEPAD_START, &NewController->Start);
							Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Back,
															XINPUT_GAMEPAD_BACK, &NewController->Back);

							// bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
							// bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
						}
						else
						{
							// This controller not available
							NewController->IsConnected = false;	
						}
					}								

					thread_context Thread = {};									

					game_offscreen_buffer Buffer = {};
					Buffer.Memory = GlobalBackbuffer.Memory;
					Buffer.Height = GlobalBackbuffer.Height;
					Buffer.Width = GlobalBackbuffer.Width;
					Buffer.Pitch = GlobalBackbuffer.Pitch;
					Buffer.BytesPerPixel = GlobalBackbuffer.BytesPerPixel;

					if(Win32State.InputRecordingIndex)
					{
						Win32RecordInput(&Win32State, NewInput);
					}

					if(Win32State.InputPlaybackIndex)
					{
						Win32PlayBackInput(&Win32State, NewInput);
					}

					if(Game.UpdateAndRender)
					{
						Game.UpdateAndRender(&Thread, &GameMemory, NewInput, &Buffer);
					}					

					LARGE_INTEGER AudioWallClock = Win32GetWallClock();
					real32 FromBeginToAudioSeconds = Win32GetSecondsElapsed(AudioWallClock, Win32GetWallClock());

					DWORD PlayCursor;
					DWORD WriteCursor;
					if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
					{

						/* NOTE:

						How sound output computation works.

						Define a safety alue that is the number of samples we think the game
						update loop varies by.

						Ofter the game update finishes, check the playcursor position and forecast
						where we think the play cursor will be on the next frame boundary.

						Then look to see if the write cursor is before that by at least the safety value.
						If it is, write up to the next frame boundary from the write cursor and
						one frame further. (Perfect audio sync with low enough latency).

						If the write cursor is after the safety margin then we assume we can never sync the audio perfectly,
						so write one frames worth of audio plus the safety margin.
						*/

						if (!SoundIsValid)
						{
							SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
							SoundIsValid = true;
						}

						DWORD ExpectedSoundBytesPerFrame = (DWORD)(((real32)SoundOutput.SamplesPerSecond * (real32)SoundOutput.BytesPerSample) / GameUpdateHz);
						real32 SecondsLeftUntilFlip = TargetSecondsPerFrame - FromBeginToAudioSeconds;
						DWORD ExpectedBytesUntilFlip = (DWORD)((SecondsLeftUntilFlip / TargetSecondsPerFrame) * (real32)ExpectedSoundBytesPerFrame);

						DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedBytesUntilFlip;
						DWORD SafeWriteCursor = WriteCursor;
						if (SafeWriteCursor < PlayCursor)
						{
							SafeWriteCursor += SoundOutput.SecondaryBufferSize;
						}
						Assert(SafeWriteCursor >= PlayCursor);
						SafeWriteCursor += SoundOutput.SafetyBytes;
						bool32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);

						DWORD TargetCursor = 0;
						DWORD ByteToLock = ((SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize);
						if (AudioCardIsLowLatency)
						{
							TargetCursor = (ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame) % SoundOutput.SecondaryBufferSize;
						}
						else
						{
							TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput.SafetyBytes) % SoundOutput.SecondaryBufferSize;
						}

						DWORD BytesToWrite = 0;
						if (ByteToLock > TargetCursor)
						{
							BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
							BytesToWrite += TargetCursor;
						}
						else
						{
							BytesToWrite = TargetCursor - ByteToLock;
						}

						game_sound_output_buffer SoundBuffer = {};
						SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
						SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
						SoundBuffer.Samples = Samples;

						if(Game.GetSoundSamples)
						{
							Game.GetSoundSamples(&Thread, &GameMemory, &SoundBuffer);
						}						

#if HANDMADE_INTERNAL
						{
							win32_debug_time_marker *Marker = &DebugMarkers[DebugMarkerIndex];
							Marker->OutputPlayCursor = PlayCursor;
							Marker->OutputWriteCursor = WriteCursor;
							Marker->OutputLocation = ByteToLock;
							Marker->OutputByteCount = BytesToWrite;
							Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;


							DWORD UnwrappedWriteCursor = WriteCursor;
							if (UnwrappedWriteCursor < PlayCursor)
							{
								UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
							}
							AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
							AudioLatencySeconds = (((real32)AudioLatencyBytes / (real32)SoundOutput.BytesPerSample) / (real32)SoundOutput.SamplesPerSecond);

							/*
							char TextBuffer[256];
							sprintf_s(TextBuffer, sizeof(TextBuffer), "BTL:%u TC:%u BTW:%u | PC:%u WC:%u DELTA:%u\n",
								ByteToLock, TargetCursor, BytesToWrite,
								PlayCursor, WriteCursor, AudioLatencyBytes);
							OutputDebugStringA(TextBuffer);
							*/
						}
#endif
						Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
					}
					else
					{
						SoundIsValid = false;
					}					

					LARGE_INTEGER WorkCounter = Win32GetWallClock();
					real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);				

					real32 SecondsElapsedForFrame = WorkSecondsElapsed;	

					if(SecondsElapsedForFrame < TargetSecondsPerFrame)
					{
						if(SleepIsGranular)
						{
							DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame - SecondsElapsedForFrame));
							if(SleepMS > 0) {Sleep(SleepMS);}							
						}
						real32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());

						if(TestSecondsElapsedForFrame < TargetSecondsPerFrame)
						{
							// TODO Log missed sleep here
						}

						while(SecondsElapsedForFrame < TargetSecondsPerFrame)
						{										
							// We meltin CPU B)									
							SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());					
						}
					}
					else
					{
						// TODO MISSED FRAME RATE!!!
						// Logging
					}

					LARGE_INTEGER EndCounter = Win32GetWallClock();
					real32 MSPerFrame = 1000.0f*Win32GetSecondsElapsed(LastCounter, EndCounter);					
					LastCounter = EndCounter;										

					Dimension = Win32GetWindowDimension(Window);
					HDC DeviceContext = GetDC(Window);
					win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, 
												Dimension.Width, Dimension.Height);	
					ReleaseDC(Window, DeviceContext);		

					FlipWallClock = Win32GetWallClock();

#if HANDMADE_INTERNAL
					{
						DWORD PlayCursor;
						DWORD WriteCursor;
						if(SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
						{
							DebugMarkers[DebugMarkerIndex].FlipPlayCursor = PlayCursor;
							DebugMarkers[DebugMarkerIndex].FlipWriteCursor = WriteCursor;							
						}
					}
#endif
										

#if HANDMADE_INTERNAL
					DebugMarkerIndex++;
					if(DebugMarkerIndex >= ArrayCount(DebugMarkers)) 
					{
						DebugMarkerIndex = 0;
					}
#endif


					game_input *Temp = NewInput;
					NewInput = OldInput;
					OldInput = Temp;

					int64 EndCycleCount = __rdtsc();
					int64 CyclesElapsed = EndCycleCount - LastCycleCount;
					LastCycleCount = EndCycleCount;
					
					real64 FPS = 0.0f;
					real64 MCPF = ((real64)CyclesElapsed / (1000.0f * 1000.0f));

#if 0
					char FPSBuffer[256];
					sprintf_s(FPSBuffer, "ms/f: %f, FPS: %f, MC/f: %f\n", MSPerFrame, FPS, MCPF);
					OutputDebugStringA(FPSBuffer);	
#endif					
				}
			}
			else
			{
				// No memory allocates, log failure
			}
		}
		else
		{
			// TODO log failure
		}
	}
	else
	{
		// TODO: Log failure
	}

	return 0;
}
