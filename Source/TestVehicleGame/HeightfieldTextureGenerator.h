// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/Texture2D.h"
#include "HeightfieldTextureGenerator.generated.h"

/**
 * Terrain generation pattern types
 */
UENUM(BlueprintType)
enum class EHeightfieldPattern : uint8
{
	/** Flat terrain with no variation */
	Flat,
	/** Sine wave hills */
	SineWaves,
	/** Circular hill in center */
	CentralHill,
	/** Bowl/crater shape */
	Bowl,
	/** Random noise terrain */
	Noise,
	/** Combination of patterns */
	Mixed
};

/**
 * Blueprint function library for generating procedural heightmap textures
 * for use with UHeightfieldMeshCollisionComponent and UHeightfieldMeshComponent
 */
UCLASS()
class TESTVEHICLEGAME_API UHeightfieldTextureGenerator : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Creates a procedural heightmap texture with the specified pattern
	 *
	 * @param Width Texture width in pixels (power of 2 recommended)
	 * @param Height Texture height in pixels (power of 2 recommended)
	 * @param Pattern The terrain pattern to generate
	 * @param Amplitude Height variation amplitude (0-1, maps to full 16-bit range)
	 * @param Frequency Pattern frequency (higher = more detail)
	 * @param Seed Random seed for noise patterns
	 * @return The generated BGRA8 texture
	 */
	UFUNCTION(BlueprintCallable, Category = "Heightfield|Generator", meta = (Keywords = "create generate terrain heightmap"))
	static UTexture2D* GenerateHeightmapTexture(
		int32 Width = 256,
		int32 Height = 256,
		EHeightfieldPattern Pattern = EHeightfieldPattern::SineWaves,
		float Amplitude = 0.3f,
		float Frequency = 4.0f,
		int32 Seed = 12345
	);

	/**
	 * Creates a flat heightmap texture at specified height
	 *
	 * @param Width Texture width
	 * @param Height Texture height
	 * @param HeightValue Normalized height (0-1, 0.5 = middle)
	 * @param MaterialIndex Physical material index for R channel
	 */
	UFUNCTION(BlueprintCallable, Category = "Heightfield|Generator")
	static UTexture2D* GenerateFlatHeightmap(
		int32 Width = 256,
		int32 Height = 256,
		float HeightValue = 0.5f,
		uint8 MaterialIndex = 0
	);

	/**
	 * Encodes a normalized height (0-1) to BGRA8 format
	 * B = low byte, G = high byte, R = material index, A = 255
	 */
	UFUNCTION(BlueprintPure, Category = "Heightfield|Generator")
	static FColor EncodeHeightToColor(float NormalizedHeight, uint8 MaterialIndex = 0);

	/**
	 * Decodes BGRA8 color back to normalized height (0-1)
	 */
	UFUNCTION(BlueprintPure, Category = "Heightfield|Generator")
	static float DecodeHeightFromColor(FColor Color);

private:
	/** Generate height value for a given pattern at normalized coordinates */
	static float GenerateHeightAtPoint(float NormX, float NormY, EHeightfieldPattern Pattern, float Amplitude, float Frequency, FRandomStream& Random);

	/** Simple noise function */
	static float SimpleNoise(float X, float Y, int32 Seed);
};
