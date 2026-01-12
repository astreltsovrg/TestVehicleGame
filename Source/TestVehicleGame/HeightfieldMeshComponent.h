// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "HeightfieldMeshComponent.generated.h"

class UTexture2D;
class UMaterialInterface;

/**
 * A mesh component that renders terrain from a heightmap texture.
 * Designed to work alongside UHeightfieldMeshCollisionComponent for
 * synchronized visual and collision representation.
 *
 * Texture format (same as collision component):
 * - B + G channels = 16-bit height (B = high byte, G = low byte)
 * - R channel = material index (for material layers, optional)
 */
UCLASS(ClassGroup="Rendering", meta=(BlueprintSpawnableComponent))
class TESTVEHICLEGAME_API UHeightfieldMeshComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	UHeightfieldMeshComponent(const FObjectInitializer& ObjectInitializer);

	//~ Begin UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual int32 GetNumMaterials() const override;
	virtual UMaterialInterface* GetMaterial(int32 MaterialIndex) const override;
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* Material) override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End UPrimitiveComponent Interface

	/** Rebuilds the mesh from the heightmap texture */
	UFUNCTION(BlueprintCallable, Category="Mesh")
	void RebuildMesh();

	/** Updates a region of the mesh (for runtime deformation) */
	UFUNCTION(BlueprintCallable, Category="Mesh")
	void UpdateMeshRegion(int32 StartRow, int32 StartCol, int32 NumRows, int32 NumCols);

	/** Get mesh vertex/index data for external use */
	void GetMeshData(TArray<FVector>& OutVertices, TArray<uint32>& OutIndices, TArray<FVector>& OutNormals, TArray<FVector2D>& OutUVs) const;

protected:
	/** The heightmap texture (BGRA8 format, same as collision component) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heightfield")
	TObjectPtr<UTexture2D> HeightmapTexture;

	/** Scale of the heightfield (should match collision component) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heightfield")
	FVector HeightfieldScale = FVector(100.0f, 100.0f, 100.0f);

	/** Material to use for rendering */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heightfield")
	TObjectPtr<UMaterialInterface> TerrainMaterial;

	/** LOD reduction factor (1 = full resolution, 2 = half, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Heightfield", meta=(ClampMin="1", ClampMax="16"))
	int32 LODFactor = 1;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** Extract height from texture at given coordinates */
	float GetHeightAt(int32 X, int32 Y) const;

	/** Calculate normal at given coordinates */
	FVector CalculateNormalAt(int32 X, int32 Y) const;

	/** Cached mesh data */
	TArray<FVector> Vertices;
	TArray<uint32> Indices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> VertexColors;

	/** Cached texture data for fast access */
	TArray<uint16> CachedHeights;
	int32 TextureWidth = 0;
	int32 TextureHeight = 0;

	/** Cached bounds */
	FBox CachedLocalBounds;

	friend class FHeightfieldMeshSceneProxy;
};
