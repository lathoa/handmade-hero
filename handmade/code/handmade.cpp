#include "handmade.h"

#include "handmade_tile.cpp"
#include <stdio.h>

internal void GameOutputSound(game_sound_output_buffer *SoundBuffer, game_state *GameState)
{	
	int16 ToneVolume = 3000; 	
	//int WavePeriod = SoundBuffer->SamplesPerSecond / GameState->ToneHz;

	int16 *SampleOut = SoundBuffer->Samples;

	for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; SampleIndex++)
	{
		// Region 1			
#if 0
		real32 SineValue = sinf(GameState->tSine);
		int16 SampleValue = (int16)(SineValue * ToneVolume);
#else 
		int16 SampleValue = 0;
#endif
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;

#if 0
		GameState->tSine += 2.0f*PI/(real32)WavePeriod;	
		if(GameState->tSine > (2.0f*PI))
		{
			GameState->tSine -= (2.0f*PI);
		}	
#endif
	}
}

internal void DrawRectangle(game_offscreen_buffer *Buffer, v2 vMin, v2 vMax, RGBReal RGB)
{
	
	int32 MinX = RoundReal32ToInt32(fClamp(vMin.X, 0.0f, (real32)Buffer->Width));
	int32 MaxX = RoundReal32ToInt32(fClamp(vMax.X, 0.0f, (real32)Buffer->Width));

	int32 MinY = RoundReal32ToInt32(fClamp(vMin.Y, 0.0f, (real32)Buffer->Height));	
	int32 MaxY = RoundReal32ToInt32(fClamp(vMax.Y, 0.0f, (real32)Buffer->Height));

	uint32 Color = RGBReal32ToUInt32(RGB.d[0], RGB.d[1], RGB.d[2]);
	
	for(int Y = MinY; Y < MaxY; Y++)
	{		
		uint8 *Pixel = ((uint8 *)Buffer->Memory + MinX*Buffer->BytesPerPixel + Y*Buffer->Pitch);
		for(int X = MinX; X < MaxX; X++)
		{
			*(uint32 *)Pixel = Color;
			Pixel += Buffer->BytesPerPixel;
		}
	}
}

#pragma pack(push, 1)
struct bitmap_header
{
	uint16 FileType;
	uint32 FileSize;
	uint16 Reserved1;
	uint16 Reserved2;
	uint32 BitmapOffset;
	uint32 Size;
	int32 Width;
	int32 Height;
	uint16 Planes;
	uint16 BitsPerPixel;
	uint32 Compression;
	uint32 SizeOfBitmap;
	int32 HorzResolution;
	int32 VertResolution;
	uint32 ColorsUsed;
	uint32 ColorsImportant;

	uint32 RedMask;
	uint32 GreenMask;
	uint32 BlueMask;
};
#pragma pack(pop)

internal loaded_bitmap DEBUGLoadBMP(thread_context *Thread, debug_platform_read_entire_file *ReadEntireFile, char *Filename)
{
	loaded_bitmap Result = {};

	debug_read_file_result ReadResult = ReadEntireFile(Thread, Filename);	
	if(ReadResult.ContentsSize != 0)
	{
		bitmap_header *Header = (bitmap_header *)ReadResult.Contents;
		uint32 *Pixels = (uint32*)((uint8 *)ReadResult.Contents + Header->BitmapOffset);
		Result.Pixels = Pixels;
		Result.Width = Header->Width;
		Result.Height = Header->Height;

		// NOTE: If you are using this generically, remember that BMP files can go in either direction
		// and the height will be negative for top down
		// Also, there can be compression, etc. Not complete BMP loading code.				

		// Early break if compression value is not 3
		if(Header->Compression != 3) return Result;


		// NOTE: Byte order in memory is determined by the Header itself when compression = 3,
		// we have to read out the masks and convert the pixels ourselves
		uint32 RedMask = Header->RedMask;
		uint32 GreenMask = Header->GreenMask;
		uint32 BlueMask = Header->BlueMask;
		uint32 AlphaMask = ~(RedMask | GreenMask | BlueMask);

		bit_scan_result RedShift = FindLeastSignificantSetBit(RedMask);
		bit_scan_result GreenShift = FindLeastSignificantSetBit(GreenMask);
		bit_scan_result BlueShift = FindLeastSignificantSetBit(BlueMask);
		bit_scan_result AlphaShift = FindLeastSignificantSetBit(AlphaMask);

		Assert(RedShift.Found);
		Assert(GreenShift.Found);
		Assert(BlueShift.Found);
		Assert(AlphaShift.Found);

		uint32 *SourceDest = Pixels;
		for(int32 Y = 0; Y < Header->Height; Y++)
		{
			for(int32 X = 0; X < Header->Width; X++)
			{
				uint32 C = *SourceDest;
				*SourceDest++ = ((((C >> AlphaShift.Index) & 0xFF) << 24) |
								(((C >> RedShift.Index) & 0xFF) << 16) | 
								(((C >> GreenShift.Index) & 0xFF) << 8) | 
								(((C >> BlueShift.Index) & 0xFF) << 0));	
			}
		}
	}	


	return Result;
}

internal void DrawBitmap(game_offscreen_buffer *Buffer, loaded_bitmap *Bitmap, real32 RealX, real32 RealY, int32 AlignX = 0, int32 AlignY = 0)
{	

	RealX -= (real32)AlignX;
	RealY -= (real32)AlignY;	

	int32 MinX = TruncateReal32ToInt32(RealX);
	int32 MinY = TruncateReal32ToInt32(RealY);
	int32 MaxX = TruncateReal32ToInt32(RealX + (real32)Bitmap->Width);
	int32 MaxY = TruncateReal32ToInt32(RealY + (real32)Bitmap->Height);

	int32 SourceOffsetX = 0;
	int32 SourceOffsetY = 0;
	if(MinX < 0)
	{
		SourceOffsetX = -MinX;
		MinX = 0;
	}
	if(MinY < 0)
	{
		SourceOffsetY = -MinY;
		MinY = 0;
	}
	if(MaxX > Buffer->Width)
	{	
		MaxX = Buffer->Width;
	}
	if(MaxY > Buffer->Height)
	{
		MaxY = Buffer->Height;
	}
	
	// TODO WE CRASHING HERE :( but I don't want to fix right now
	uint32 *SourceRow = Bitmap->Pixels + (Bitmap->Width*(Bitmap->Height-1));
	SourceRow += -SourceOffsetY*Bitmap->Width + SourceOffsetX;
	uint8 *DestRow = (uint8 *)Buffer->Memory + MinX*Buffer->BytesPerPixel + MinY*Buffer->Pitch;
	for(int32 Y = MinY; Y < MaxY; Y++)
	{
		uint32 *Dest = (uint32 *)DestRow;
		uint32 *Source = (uint32 *)SourceRow;
		for(int32 X = MinX; X < MaxX; X++)
		{
			real32 A = ((real32)((*Source >> 24) & 0xFF) / 255.0f);
			real32 SR = (real32)((*Source >> 16) & 0xFF);
			real32 SG = (real32)((*Source >> 8) & 0xFF);
			real32 SB = (real32)((*Source >> 0) & 0xFF);

			real32 DR = (real32)((*Dest >> 16) & 0xFF);
			real32 DG = (real32)((*Dest >> 8) & 0xFF);
			real32 DB = (real32)((*Dest >> 0 )& 0xFF);

			real32 R = (1.0f - A)*DR + A*SR;
			real32 G = (1.0f - A)*DG + A*SG;
			real32 B = (1.0f - A)*DB + A*SB;

			*Dest =	((*Source & 0xFF000000) | 
					((uint32)(R + 0.5f) << 16) |
					((uint32)(G + 0.5f) << 8) |
					(uint32)(B + 0.5f)); 
						

			Dest++;
			Source++;
		}
		DestRow += Buffer->Pitch;
		SourceRow -= Bitmap->Width;
	}
}

internal entity* GetEntity(game_state *GameState, uint32 Index)
{
	entity *Entity = 0;
	if((Index > 0) & (Index < ArrayCount(GameState->Entities)))
	{
		Entity = &GameState->Entities[Index];
	}

	return Entity;
}

internal void InitializePlayer(entity *Entity)
{	
	Entity = {};

	Entity->Exists = true;
	Entity->P.AbsTileX = 1;
	Entity->P.AbsTileY = 3;
	Entity->P.Offset.X = 5.0f;
	Entity->P.Offset.Y = 5.0f;		
}

#if 0
internal void movement()
{

	tile_map_position OldPlayerP = GameState->PlayerP;

	ddPlayer *= GameState->PlayerAcceleration;			
	if(Controller->ActionUp.EndedDown)
	{
		ddPlayer *= 8;				
	}	

	// TODO ODE here! (Friction)
	ddPlayer += -1.0f * GameState->dPlayerP;

	tile_map_position NewPlayerP = GameState->PlayerP;
	// Update position and velocity
	v2 PlayerDelta = (0.5f * ddPlayer*Square(Input->dtForFrame) + 
						(GameState->dPlayerP * Input->dtForFrame));
	NewPlayerP.Offset += PlayerDelta; 								
	GameState->dPlayerP += ddPlayer*Input->dtForFrame;						
	NewPlayerP = RecanonicalizePosition(TileMap, NewPlayerP);

#if 1			
	tile_map_position PlayerLeft = NewPlayerP;
	PlayerLeft.Offset.X -= 0.5f*GameState->PlayerWidth;
	PlayerLeft = RecanonicalizePosition(TileMap, PlayerLeft);

	tile_map_position PlayerRight = NewPlayerP;
	PlayerRight.Offset.X += 0.5f*GameState->PlayerWidth;
	PlayerRight = RecanonicalizePosition(TileMap, PlayerRight);
	

	bool32 Collided = false;
	tile_map_position ColP = {};
	if(!IsTileMapPointEmpty(TileMap, NewPlayerP))
	{
		Collided = true;
		ColP = NewPlayerP;
	}
	if(!IsTileMapPointEmpty(TileMap, PlayerLeft))
	{
		Collided = true;
		ColP = PlayerLeft;
	}
	if(!IsTileMapPointEmpty(TileMap, PlayerRight))
	{
		Collided = true;
		ColP = PlayerRight;
	}
	if(Collided)
	{
		v2 R = {};
		if(ColP.AbsTileX < GameState->PlayerP.AbsTileX)
		{
			// Left wall;
			R = V2(1, 0);
		}
		if(ColP.AbsTileX > GameState->PlayerP.AbsTileX)
		{
			// Right wall;
			R = V2(-1, 0);
		}
		if(ColP.AbsTileY < GameState->PlayerP.AbsTileY)
		{
			R = V2(0, 1);
		}
		if(ColP.AbsTileY > GameState->PlayerP.AbsTileY)
		{
			R = V2(0, -1);
		}
		GameState->dPlayerP = GameState->dPlayerP - 1 * Inner(GameState->dPlayerP, R) * R;
	}
	else
	{
		GameState->PlayerP = NewPlayerP;
	}
#else
	uint32 MinTileX = 0;
	uint32 MinTileY = 0;
	uint32 OnePastMaxTileX = 0;
	uint32 OnePastMaxTileY = 0;
	uint32 AbsTileZ = GameState->PlayerP.AbsTileZ;
	tile_map_position BestPoint  = GameState->PlayerP;
	real32 BestDistanceSq = LengthSq(PlayerDelta);
	for(uint32 AbsTileY = MinTileY; AbsTileY != OnePastMaxTileY; AbsTileY++)
	{
		for(uint32 AbsTileX = MinTileX; AbsTileX != OnePastMaxTileX; AbsTileX++)
		{
			tile_map_position TestTileP = CenteredTilePoint(AbsTileX, AbsTileY, AbsTileZ);
			uint32 TileValue = GetTileValue(TileMap, TestTileP);
			if(IsTileValueEmpty(TileValue))
			{
				v2 MinCorner = -0.5f*v2{TileMap->TileSideInMeters, TileMap->TileSideInMeters};
				v2 MaxCorner = 0.5f*v2{TileMap->TileSideInMeters, TileMap->TileSideInMeters};

				tile_map_difference RelNewPlayerP = Subtract(TileMap, &TestTileP, &NewPlayerP);

				v2 TestP = ClosestPointInPectangle(MinCorner, MaxCorner, RelNewPlayerP);
				if(LengthSq(TestP) < BestDistanceSq)
				{

				}
			}
		}
	}		
#endif

	// NOTE: update camera/player Z based on last movement
	if(!AreOnSameTile(&OldPlayerP, &GameState->PlayerP))
	{
		uint32 NewTileValue = GetTileValue(TileMap, GameState->PlayerP);

		if(NewTileValue == 3)
		{
			++GameState->PlayerP.AbsTileZ;
		}
		else if(NewTileValue == 4)
		{
			--GameState->PlayerP.AbsTileZ;
		}
	}
}

#endif

// extern "C": Prevents name mangling of compiled function
extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{    

	// Assert that the Buttons[] and button struct in the game_controller_input are identical sizes
	// Take the last know button, subtract the base address, and the value should be equal the the number of entries
	Assert((&Input->Controllers[0].Start - &Input->Controllers[0].Buttons[0]) == 
			(ArrayCount(Input->Controllers[0].Buttons) - 1));

	// Assert that our game state will fit in the allocated permanent memory
	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);	

	game_state *GameState = (game_state*)Memory->PermanentStorage;
	if(!Memory->IsInitialized)
	{		
		GameState->Backdrop = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_background.bmp");

		hero_bitmaps *Bitmap;

		Bitmap = &GameState->HeroBitmaps[0];
		Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_head.bmp");
		Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_cape.bmp");
		Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_torso.bmp");
		Bitmap->AlignX = 76;
		Bitmap->AlignY = 182;		

		Bitmap++;
		Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_head.bmp");
		Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_cape.bmp");
		Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_torso.bmp");
		Bitmap->AlignX = 71;
		Bitmap->AlignY = 181;				

		Bitmap++;
		Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_head.bmp");
		Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_cape.bmp");
		Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_torso.bmp");
		Bitmap->AlignX = 66;
		Bitmap->AlignY = 181;					

		Bitmap++;
		Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_head.bmp");
		Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_cape.bmp");
		Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_torso.bmp");
		Bitmap->AlignX = 71;
		Bitmap->AlignY = 181;					

		GameState->CameraP.AbsTileX = 17/2;
		GameState->CameraP.AbsTileY = 9/2;				

		InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state), 
						(uint8 *)Memory->PermanentStorage + sizeof(game_state));

		GameState->World = PushStruct(&GameState->WorldArena, world);
		world *World = GameState->World;
		World->TileMap = PushStruct(&GameState->WorldArena, tile_map);

		tile_map *TileMap = World->TileMap;

		// Set to using 256x256 Tile Chunks
		TileMap->ChunkShift = 4;
		TileMap->ChunkMask = (0x1 << TileMap->ChunkShift) - 1; // bit magic B)
		TileMap->ChunkDim = 0x1 << TileMap->ChunkShift;

		TileMap->TileChunkCountX = 128;
		TileMap->TileChunkCountY = 128;
		TileMap->TileChunkCountZ = 2;

		TileMap->TileChunks = PushArray(&GameState->WorldArena, 
										TileMap->TileChunkCountX*TileMap->TileChunkCountY*TileMap->TileChunkCountZ, 
										tile_chunk);

		TileMap->TileSideInMeters = 1.4f;					

		uint32 TilesPerWidth = 17;
		uint32 TilesPerHeight = 9;

		uint32 ScreenX = 0;
		uint32 ScreenY = 0;
		uint32 RandomNumberIndex = 0;
		bool32 DoorLeft = false;
		bool32 DoorRight = false;
		bool32 DoorTop = false;
		bool32 DoorBottom = false;
		bool32 DoorUp = false;
		bool32 DoorDown = false;
		uint32 AbsTileZ = 0;
		for(uint32 ScreenIndex = 0; ScreenIndex < 100; ScreenIndex++)
		{
			// TODO Random Number Generator
			Assert(RandomNumberIndex < ArrayCount(RandomNumberTable));
			uint32 RandomChoice;
			if(DoorUp || DoorDown)
			{
				RandomChoice = RandomNumberTable[RandomNumberIndex++] % 2;
			}
			else{
				RandomChoice = RandomNumberTable[RandomNumberIndex++] % 3;
			}

			bool32 CreatedZDoor = false;
			if(RandomChoice == 2)
			{
				CreatedZDoor = true;
				if(AbsTileZ == 0)
				{
					DoorUp = true;
				}
				else{
					DoorDown = true;
				}
			}
			else if(RandomChoice == 1)
			{
				DoorRight = true;
			}
			else
			{
				DoorTop = true;
			}
			
			for(uint32 TileY = 0; TileY < TilesPerHeight; TileY++)
			{
				for(uint32 TileX = 0; TileX < TilesPerWidth; TileX++)
				{
					uint32 AbsTileX = ScreenX*TilesPerWidth + TileX;
					uint32 AbsTileY = ScreenY*TilesPerHeight + TileY;					

					uint32 TileValue = 1;
					if((TileX == 0) && (!DoorLeft || (TileY != (TilesPerHeight/2))))
					{
						TileValue = 2;					
					}
					if((TileX == (TilesPerWidth - 1)) && (!DoorRight || (TileY != (TilesPerHeight/2))))
					{
						TileValue = 2;					
					}								
					if((TileY == 0) && (!DoorBottom ||TileX != (TilesPerWidth/2)))
					{						
						TileValue = 2;							
					}					
					if((TileY == (TilesPerHeight - 1)) && (!DoorTop ||TileX != (TilesPerWidth/2)))
					{						
						TileValue = 2;							
					}

					if((TileX == 10) && (TileY == 6))
					{
						if(DoorUp)
						{
							TileValue = 3;
						}
						
						if(DoorDown)
						{
							TileValue = 4;
						}
						
					}

					SetTileValue(&GameState->WorldArena, World->TileMap, 
								AbsTileX, AbsTileY, AbsTileZ,
								TileValue);
				}
			}
			
			DoorLeft = DoorRight;
			DoorBottom = DoorTop;

			if(CreatedZDoor)
			{			
				DoorDown = !DoorDown;
				DoorUp = !DoorUp;
			}
			else
			{
				DoorUp = false;
				DoorDown = false;
			}

			DoorRight = false;
			DoorTop = false;
		
			if(RandomChoice == 2)
			{
				if(AbsTileZ == 0)
				{
					AbsTileZ = 1;
				}
				else
				{
					AbsTileZ = 0;
				}
			}
			else if(RandomChoice == 1)
			{
				ScreenX += 1;
			}
			else{
				ScreenY += 1;
			}
		}


		
		Memory->IsInitialized = true;
	}						

	world *World = GameState->World;
	tile_map *TileMap = World->TileMap;

	int32 TileSideInPixels = 60;
	real32 MetersToPixels = (real32)(TileSideInPixels / TileMap->TileSideInMeters);
	real32 PlayerHeight = 1.4f;
	real32 PlayerWidth = PlayerHeight*0.75f;
	real32 PlayerAcceleration = 10.0f;	

	for(int ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ++ControllerIndex)
	{	
		game_controller_input *Controller = GetController(Input, ControllerIndex);

		if(Controller->IsAnalog)
		{	
			// NOTE: analog movement tuning
		}
		else
		{				
			v2 ddPlayer = {};

			if(Controller->MoveUp.EndedDown)
			{
				ddPlayer.Y = 1.0f;
				GameState->HeroFacingDirection = 1;
			}
			if(Controller->MoveDown.EndedDown)
			{
				ddPlayer.Y = -1.0f;
				GameState->HeroFacingDirection = 3;
			}
			if(Controller->MoveLeft.EndedDown)
			{
				ddPlayer.X = -1.0f;
				GameState->HeroFacingDirection = 2;
			}
			if(Controller->MoveRight.EndedDown)
			{
				ddPlayer.X = 1.0f;
				GameState->HeroFacingDirection = 0;
			}			

			// TODO Normalize
			if((ddPlayer.X != 0.0f) && (ddPlayer.Y != 0.0f))
			{
				ddPlayer *= 0.707;				
			}

		}
	}	

	entity *CameraFollowingEntity = GetEntity(GameState, GameState->CameraFollowingEntityIndex);
	if(CameraFollowingEntity)
	{		
		GameState->CameraP.AbsTileZ = GameState->PlayerP.AbsTileZ;	
	}	

	tile_map_difference Diff = Subtract(TileMap, &GameState->PlayerP, &GameState->CameraP);
	if(Diff.dXY.X > (9.0f*TileMap->TileSideInMeters))
	{
		GameState->CameraP.AbsTileX += 17;
	}
	if(Diff.dXY.X < -(9.0f*TileMap->TileSideInMeters))
	{
		GameState->CameraP.AbsTileX -= 17;
	}
	if(Diff.dXY.Y > (5.0f*TileMap->TileSideInMeters))
	{
		GameState->CameraP.AbsTileY += 9;
	}
	if(Diff.dXY.Y < -(5.0f*TileMap->TileSideInMeters))
	{
		GameState->CameraP.AbsTileY -= 9;
	}		
	Diff = Subtract(TileMap, &GameState->PlayerP, &GameState->CameraP);

	// NOTE: Render
	DrawBitmap(Buffer, &GameState->Backdrop, 0.0f, 0.0f);

	real32 ScreenCenterX = 0.5f*(real32)Buffer->Width;
	real32 ScreenCenterY = 0.5f*(real32)Buffer->Height;

	for(int32 RelRow = -10; RelRow < 10; RelRow++)
	{
		for(int32 RelCol = -20; RelCol < 20; RelCol++)
		{

			uint32 Col = RelCol + GameState->CameraP.AbsTileX;
			uint32 Row = RelRow  + GameState->CameraP.AbsTileY;
			uint32 TileID = GetTileValue(TileMap, Col, Row, GameState->CameraP.AbsTileZ);
			if(TileID > 1){		
				real32 Gray = 0.5f;				
				if(TileID == 2)
				{
					Gray = 1.0f;
				}	
				if(TileID > 2)
				{
					Gray = 0.2f;
				}	

				if((Row == GameState->CameraP.AbsTileY) && (Col == GameState->CameraP.AbsTileX))
				{
					Gray = 0.0f;
				}

				

				v2 HalfTileSide = {0.5f*TileSideInPixels, 0.5f*TileSideInPixels};
				v2 Cen = {ScreenCenterX - MetersToPixels*GameState->CameraP.Offset.X + (real32)RelCol*TileSideInPixels,
							ScreenCenterY + MetersToPixels*GameState->CameraP.Offset.Y - (real32)RelRow*TileSideInPixels};
				v2 Min = Cen - HalfTileSide;				
				v2 Max = Cen + HalfTileSide;				
				RGBReal TileColor = {Gray, Gray, Gray};
				DrawRectangle(Buffer, Min, Max, TileColor);
			}
		}
	}		

	RGBReal PlayerColor = {0.8f, 0.8f, 0.0f};
	real32 PlayerGroundX = ScreenCenterX + MetersToPixels*Diff.dXY.X;
	real32 PlayerGroundY = ScreenCenterY - MetersToPixels*Diff.dXY.Y;
	v2 PlayerLeftTop =	{PlayerGroundX - MetersToPixels*0.5f*PlayerWidth,
						 PlayerGroundY - MetersToPixels*PlayerHeight};
	v2 PlayerWidthHeight = {PlayerWidth, PlayerHeight};
	DrawRectangle(Buffer, 
				  PlayerLeftTop,
				  PlayerLeftTop + MetersToPixels*PlayerWidthHeight,
				  PlayerColor);
			
	hero_bitmaps *HeroBitmaps = &GameState->HeroBitmaps[GameState->HeroFacingDirection];			
	
	if(GameState->HeroFacingDirection == 3)
	{
		DrawBitmap(Buffer, &HeroBitmaps->Cape, PlayerGroundX, PlayerGroundY, 
					HeroBitmaps->AlignX, HeroBitmaps->AlignY);
		DrawBitmap(Buffer, &HeroBitmaps->Torso, PlayerGroundX, PlayerGroundY, 
					HeroBitmaps->AlignX, HeroBitmaps->AlignY);				
		DrawBitmap(Buffer, &HeroBitmaps->Head, PlayerGroundX, PlayerGroundY, 
					HeroBitmaps->AlignX, HeroBitmaps->AlignY);
	}
	else
	{
		DrawBitmap(Buffer, &HeroBitmaps->Torso, PlayerGroundX, PlayerGroundY, 
					HeroBitmaps->AlignX, HeroBitmaps->AlignY);
		DrawBitmap(Buffer, &HeroBitmaps->Cape, PlayerGroundX, PlayerGroundY, 
					HeroBitmaps->AlignX, HeroBitmaps->AlignY);	
		DrawBitmap(Buffer, &HeroBitmaps->Head, PlayerGroundX, PlayerGroundY, 
					HeroBitmaps->AlignX, HeroBitmaps->AlignY);			  
	}
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
	// TODO : may not want continuous samples, could require earlier or later samples	
	game_state *GameState = (game_state*)Memory->PermanentStorage;
	GameOutputSound(SoundBuffer, GameState);
}

/*
internal void RenderWeirdGradient(game_offscreen_buffer *Buffer, int XOffset, int YOffset)
{
	uint8 *Row = (uint8 *)Buffer->Memory;
	for (int Y = 0; Y < Buffer->Height; Y++)
	{
		uint32 *Pixel = (uint32 *)Row;
		for (int X = 0; X < Buffer->Width; X++)
		{
			// Pixel in memory: BB GG RR XX
			// 0x0RGB
			uint8 Blue = (uint8)(X + XOffset);
			uint8 Green = (uint8)(Y + YOffset);

			*Pixel++ = (Green << 16) | Blue;
		}
		Row += Buffer->Pitch;
	}


}
*/