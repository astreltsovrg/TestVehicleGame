// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Chaos/ImplicitFwd.h"
#include "Chaos/PhysicalMaterials.h"

#include "HeightfieldMeshCollisionComponent.generated.h"

class UTexture2D;
class UPhysicalMaterial;

namespace Chaos
{
	class FHeightField;
}

/**
 * A component that creates heightfield collision from a BGRA8 texture.
 *
 * Texture format:
 * - B + G channels = 16-bit height (B = high byte, G = low byte)
 * - R channel = physical material index (maps to PhysicalMaterials array)
 *
 * This allows using heightfield collision for regular meshes (quads)
 * without Landscape dependency.
 */
UCLASS(ClassGroup="Collision", meta=(BlueprintSpawnableComponent), HideCategories=(Mobility))
class TESTVEHICLEGAME_API UHeightfieldMeshCollisionComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UHeightfieldMeshCollisionComponent(const FObjectInitializer& ObjectInitializer);
	virtual ~UHeightfieldMeshCollisionComponent();

	//~ Begin UActorComponent Interface
	virtual void OnRegister() override;
	//~ End UActorComponent Interface

	//~ Begin UPrimitiveComponent Interface
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual bool ShouldCreatePhysicsState() const override;
	virtual void OnCreatePhysicsState() override;
	virtual void OnDestroyPhysicsState() override;
	virtual UBodySetup* GetBodySetup() override { return nullptr; }
	//~ End UPrimitiveComponent Interface

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Rebuilds the heightfield collision from the current texture */
	UFUNCTION(BlueprintCallable, Category="Collision")
	void RebuildCollision();

	/**
	 * Efficiently updates only a region of the heightfield at runtime.
	 * Much faster than full rebuild for dynamic terrain deformation.
	 *
	 * @param Heights - Array of new height values for the region (row-major order)
	 * @param StartRow - Starting row index (Y)
	 * @param StartCol - Starting column index (X)
	 * @param NumRows - Number of rows to update
	 * @param NumCols - Number of columns to update
	 */
	UFUNCTION(BlueprintCallable, Category="Collision")
	void UpdateHeightfieldRegion(const TArray<float>& Heights, int32 StartRow, int32 StartCol, int32 NumRows, int32 NumCols);

	/**
	 * C++ version that takes uint16 heights directly (more efficient, matches internal format).
	 */
	void UpdateHeightfieldRegionRaw(TArrayView<const uint16> Heights, int32 StartRow, int32 StartCol, int32 NumRows, int32 NumCols);

	/** Returns the heightmap texture */
	UFUNCTION(BlueprintCallable, Category="Heightfield")
	UTexture2D* GetHeightmapTexture() const { return HeightmapTexture; }

	/** Sets the heightmap texture and rebuilds collision */
	UFUNCTION(BlueprintCallable, Category="Heightfield")
	void SetHeightmapTexture(UTexture2D* NewTexture);

protected:
	/**
	 * The heightmap texture (BGRA8 format).
	 * B+G = 16-bit height, R = material index.
	 * Texture must have CompressionNone and SRGB=false for correct data.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heightfield")
	TObjectPtr<UTexture2D> HeightmapTexture;

	/**
	 * Physical materials mapped to indices in the R channel.
	 * Index 0 in texture R channel -> PhysicalMaterials[0], etc.
	 * If index is out of range or material is null, uses default physics material.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heightfield")
	TArray<TObjectPtr<UPhysicalMaterial>> PhysicalMaterials;

	/**
	 * Scale of the heightfield in world units.
	 * X, Y = horizontal cell size (spacing between height samples)
	 * Z = vertical scale factor for height values
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heightfield", meta=(ClampMin="0.01"))
	FVector HeightfieldScale = FVector(100.0f, 100.0f, 100.0f);

	/** Cached local-space bounding box */
	UPROPERTY(Transient)
	FBox CachedLocalBox;

	/** Cached texture dimensions */
	UPROPERTY(Transient)
	int32 CachedNumRows = 0;

	UPROPERTY(Transient)
	int32 CachedNumCols = 0;

private:
	/**
	 * Extract height and material data from texture.
	 * @param OutHeights - Output array of 16-bit heights (row-major order)
	 * @param OutMaterialIndices - Output array of material indices per cell
	 * @param OutNumRows - Output number of rows (texture height)
	 * @param OutNumCols - Output number of columns (texture width)
	 * @return true if extraction succeeded
	 */
	bool ExtractHeightData(
		TArray<uint16>& OutHeights,
		TArray<uint8>& OutMaterialIndices,
		int32& OutNumRows,
		int32& OutNumCols) const;

	/** Create the Chaos physics objects and add to scene */
	void CreateCollisionObject();

	/** Clean up existing physics objects */
	void DestroyCollisionObject();

	/** Calculate local bounds from heightfield data */
	void UpdateCachedBounds();

	/** The Chaos heightfield geometry (runtime only, not serialized) */
	Chaos::FHeightFieldPtr HeightfieldGeometry;

	/** Chaos material handles for physics simulation */
	TArray<Chaos::FMaterialHandle> ChaosMaterialHandles;
};
