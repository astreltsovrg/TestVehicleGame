// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeightfieldTextureGenerator.h"
#include "Engine/Texture2D.h"

UTexture2D* UHeightfieldTextureGenerator::GenerateHeightmapTexture(
	int32 Width,
	int32 Height,
	EHeightfieldPattern Pattern,
	float Amplitude,
	float Frequency,
	int32 Seed)
{
	// Validate dimensions
	Width = FMath::Clamp(Width, 4, 4096);
	Height = FMath::Clamp(Height, 4, 4096);
	Amplitude = FMath::Clamp(Amplitude, 0.0f, 1.0f);

	// Create transient texture (runtime only, not saved to disk)
	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!Texture)
	{
		UE_LOG(LogTemp, Error, TEXT("HeightfieldTextureGenerator: Failed to create texture"));
		return nullptr;
	}

	// Configure texture settings
	Texture->CompressionSettings = TC_VectorDisplacementmap;
	Texture->SRGB = false;
	Texture->Filter = TF_Bilinear;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;

	// Lock texture for writing
	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FColor* Pixels = static_cast<FColor*>(Data);

	// Random stream for noise
	FRandomStream Random(Seed);

	// Generate heightmap data
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			// Normalized coordinates (0-1)
			const float NormX = static_cast<float>(X) / static_cast<float>(Width - 1);
			const float NormY = static_cast<float>(Y) / static_cast<float>(Height - 1);

			// Generate height at this point (centered around 0.5)
			float HeightValue = 0.5f + GenerateHeightAtPoint(NormX, NormY, Pattern, Amplitude, Frequency, Random);
			HeightValue = FMath::Clamp(HeightValue, 0.0f, 1.0f);

			// Determine material index based on height (simple gradient)
			uint8 MaterialIndex = 0;
			if (HeightValue > 0.7f)
			{
				MaterialIndex = 2; // High = rock/snow
			}
			else if (HeightValue > 0.4f)
			{
				MaterialIndex = 1; // Mid = grass
			}
			else
			{
				MaterialIndex = 0; // Low = dirt/sand
			}

			// Encode to BGRA8
			const int32 PixelIndex = Y * Width + X;
			Pixels[PixelIndex] = EncodeHeightToColor(HeightValue, MaterialIndex);
		}
	}

	// Unlock and update texture
	Mip.BulkData.Unlock();
	Texture->UpdateResource();

	UE_LOG(LogTemp, Log, TEXT("HeightfieldTextureGenerator: Created %dx%d heightmap with pattern %d"),
		Width, Height, static_cast<int32>(Pattern));

	return Texture;
}

UTexture2D* UHeightfieldTextureGenerator::GenerateFlatHeightmap(
	int32 Width,
	int32 Height,
	float HeightValue,
	uint8 MaterialIndex)
{
	Width = FMath::Clamp(Width, 4, 4096);
	Height = FMath::Clamp(Height, 4, 4096);
	HeightValue = FMath::Clamp(HeightValue, 0.0f, 1.0f);

	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!Texture)
	{
		return nullptr;
	}

	Texture->CompressionSettings = TC_VectorDisplacementmap;
	Texture->SRGB = false;
	Texture->Filter = TF_Bilinear;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;

	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FColor* Pixels = static_cast<FColor*>(Data);

	const FColor EncodedHeight = EncodeHeightToColor(HeightValue, MaterialIndex);
	const int32 NumPixels = Width * Height;

	for (int32 i = 0; i < NumPixels; ++i)
	{
		Pixels[i] = EncodedHeight;
	}

	Mip.BulkData.Unlock();
	Texture->UpdateResource();

	return Texture;
}

FColor UHeightfieldTextureGenerator::EncodeHeightToColor(float NormalizedHeight, uint8 MaterialIndex)
{
	// Convert 0-1 to 0-65535 (16-bit range)
	const uint16 Height16 = static_cast<uint16>(FMath::Clamp(NormalizedHeight, 0.0f, 1.0f) * 65535.0f);

	// B = low byte, G = high byte (little-endian style)
	const uint8 LowByte = Height16 & 0xFF;
	const uint8 HighByte = (Height16 >> 8) & 0xFF;

	// BGRA format: B=low height, G=high height, R=material, A=255
	return FColor(MaterialIndex, HighByte, LowByte, 255);
}

float UHeightfieldTextureGenerator::DecodeHeightFromColor(FColor Color)
{
	// Reconstruct 16-bit height from B (low) and G (high)
	const uint16 Height16 = static_cast<uint16>(Color.B) | (static_cast<uint16>(Color.G) << 8);

	// Convert back to 0-1 range
	return static_cast<float>(Height16) / 65535.0f;
}

float UHeightfieldTextureGenerator::GenerateHeightAtPoint(
	float NormX,
	float NormY,
	EHeightfieldPattern Pattern,
	float Amplitude,
	float Frequency,
	FRandomStream& Random)
{
	float Height = 0.0f;

	switch (Pattern)
	{
	case EHeightfieldPattern::Flat:
		Height = 0.0f;
		break;

	case EHeightfieldPattern::SineWaves:
		// Multiple sine waves for rolling hills
		Height = FMath::Sin(NormX * Frequency * PI * 2.0f) * 0.5f;
		Height += FMath::Sin(NormY * Frequency * PI * 2.0f) * 0.5f;
		Height += FMath::Sin((NormX + NormY) * Frequency * PI) * 0.3f;
		Height *= Amplitude * 0.5f;
		break;

	case EHeightfieldPattern::CentralHill:
		{
			// Distance from center (0.5, 0.5)
			const float DX = NormX - 0.5f;
			const float DY = NormY - 0.5f;
			const float Dist = FMath::Sqrt(DX * DX + DY * DY) * 2.0f; // 0-1 range
			// Smooth hill falloff
			Height = FMath::Max(0.0f, 1.0f - Dist * Dist) * Amplitude;
		}
		break;

	case EHeightfieldPattern::Bowl:
		{
			// Inverted central hill (crater/bowl)
			const float DX = NormX - 0.5f;
			const float DY = NormY - 0.5f;
			const float Dist = FMath::Sqrt(DX * DX + DY * DY) * 2.0f;
			Height = (Dist * Dist - 0.5f) * Amplitude;
		}
		break;

	case EHeightfieldPattern::Noise:
		{
			// Multi-octave noise
			float NoiseValue = 0.0f;
			float NoiseAmplitude = 1.0f;
			float NoiseFrequency = Frequency;

			for (int32 Octave = 0; Octave < 4; ++Octave)
			{
				NoiseValue += SimpleNoise(NormX * NoiseFrequency, NormY * NoiseFrequency, Random.GetCurrentSeed() + Octave) * NoiseAmplitude;
				NoiseAmplitude *= 0.5f;
				NoiseFrequency *= 2.0f;
			}

			Height = NoiseValue * Amplitude * 0.5f;
		}
		break;

	case EHeightfieldPattern::Mixed:
		{
			// Combine sine waves with noise for more natural terrain
			const float SineComponent = FMath::Sin(NormX * Frequency * PI * 2.0f) * FMath::Sin(NormY * Frequency * PI * 2.0f);
			const float NoiseComponent = SimpleNoise(NormX * Frequency * 2.0f, NormY * Frequency * 2.0f, Random.GetCurrentSeed());

			// Add a gentle central hill
			const float DX = NormX - 0.5f;
			const float DY = NormY - 0.5f;
			const float Dist = FMath::Sqrt(DX * DX + DY * DY) * 2.0f;
			const float HillComponent = FMath::Max(0.0f, 1.0f - Dist);

			Height = (SineComponent * 0.4f + NoiseComponent * 0.4f + HillComponent * 0.2f) * Amplitude;
		}
		break;
	}

	return Height;
}

float UHeightfieldTextureGenerator::SimpleNoise(float X, float Y, int32 Seed)
{
	// Simple value noise implementation
	auto Hash = [Seed](int32 IX, int32 IY) -> float
	{
		int32 N = IX + IY * 57 + Seed * 131;
		N = (N << 13) ^ N;
		return (1.0f - ((N * (N * N * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
	};

	const int32 IX = FMath::FloorToInt(X);
	const int32 IY = FMath::FloorToInt(Y);
	const float FX = X - IX;
	const float FY = Y - IY;

	// Smooth interpolation
	const float SX = FX * FX * (3.0f - 2.0f * FX);
	const float SY = FY * FY * (3.0f - 2.0f * FY);

	// Sample corners
	const float N00 = Hash(IX, IY);
	const float N10 = Hash(IX + 1, IY);
	const float N01 = Hash(IX, IY + 1);
	const float N11 = Hash(IX + 1, IY + 1);

	// Bilinear interpolation
	const float NX0 = FMath::Lerp(N00, N10, SX);
	const float NX1 = FMath::Lerp(N01, N11, SX);

	return FMath::Lerp(NX0, NX1, SY);
}
