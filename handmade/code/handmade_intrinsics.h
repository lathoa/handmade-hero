#ifndef HANDMADE_INTRINSICS_H
#define HANDMADE_INTRINSICS_H

#include <math.h>

//TODO convert all of these to platform-efficient versions and remove math.h

inline int32 SignOf(int32 Value)
{
	//int32 Result = (Value >> 31);
	int32 Result = (Value >= 0) ? 1 : -1;
	return Result;
}

inline real32 SquareRoot(real32 Real32)
{
	real32 Result = sqrtf(Real32);
	return Result;
}

inline uint32 RotateLeft(uint32 Value, int32 Amount)
{
	uint32 Result = _rotl(Value, Amount);
	return Result;
}

inline uint32 RotateRight(uint32 Value, int32 Amount)
{
	uint32 Result = _rotr(Value, Amount);
	return Result;
}

inline real32
AbsoluteValue(real32 Real32)
{
	real32 Result = (real32)fabs(Real32);
	return Result;
}

inline int32 RoundReal32ToInt32(real32 v)
{
	int32 Result = (int32)roundf(v);
	return Result;
}

inline uint32 RoundReal32ToUInt32(real32 v)
{
	uint32 Result = (uint32)roundf(v);
	return Result;
}

inline int32 FloorReal32ToInt32(real32 v)
{
	int32 Result = (int32)floorf(v);
	return Result;
}

inline int32 TruncateReal32ToInt32(real32 v)
{
	int32 Result = (int32)(v);
	return Result;
}

internal real32 fClamp(real32 Val, real32 Min, real32 Max)
{
	real32 Result = Val;
	if(Val < Min)
	{
		Result = Min;
	}
	else if(Val > Max)
	{
		Result = Max;
	}

	return Result;
}

internal int32 Clamp(int32 Val, int32 Min, int32 Max)
{
	int32 Result = Val;
	if(Val < Min)
	{
		Result = Min;
	}
	else if(Val > Max)
	{
		Result = Max;
	}

	return Result;
}

internal uint32 RGBReal32ToUInt32(real32 R, real32 G, real32 B)
{		
	uint8 uR = (uint8)(fClamp(R, 0.0f, 1.0f) * 255.0f);
	uint8 uG = (uint8)(fClamp(G, 0.0f, 1.0f) * 255.0f);
	uint8 uB = (uint8)(fClamp(B, 0.0f, 1.0f) * 255.0f);
	// Bit Pattern: 0x AA RR GG BB
	uint32 Result = ((uint32)uR << 16) + ((uint32)uG << 8) + ((uint32)uB);
	return Result;
}

inline real32 Sin(real32 Angle)
{
    real32 Result = sinf(Angle);
    return Result;
}

inline real32 Cos(real32 Angle)
{
    real32 Result = cosf(Angle);
    return Result;
}
inline real32 ATan2(real32 Y, real32 X)
{
    real32 Result = atan2f(Y, X);
    return Result;
}

inline real32 Square(real32 A)
{
	real32 Result = A * A;
	return A;
}

struct bit_scan_result 
{
	bool32 Found;
	uint32 Index;
};

inline bit_scan_result FindLeastSignificantSetBit(uint32 Value)
{
	bit_scan_result Result = {};

#if COMPILER_MSVC
	Result.Found = _BitScanForward((unsigned long *)&Result.Index, Value);
#else
	for(uint32 Test = 0;Test < 32; Test++)
	{
		if(Value & (1 << Test))
		{
			Result.Index = Test;
			Result.Found = true;
			break;
		}
	}		
#endif
	return Result;

}

#endif