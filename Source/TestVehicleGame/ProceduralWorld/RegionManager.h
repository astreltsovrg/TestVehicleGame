// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/DataAsset.h"
#include "RegionManager.generated.h"

class AVoxelWorld;
class UVoxelInstancedStampComponent;
class UVoxelHeightGraph;
class URegionManagerConfig;

/**
 * Configuration DataAsset for RegionManager.
 * Create this in Content Browser: Right-click → Miscellaneous → Data Asset → RegionManagerConfig
 */
UCLASS(BlueprintType)
class TESTVEHICLEGAME_API URegionManagerConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Size of each region in centimeters (default: 1024m = 102400cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
	float RegionSizeCm = 102400.0f;

	/** Number of regions to load around player (Chebyshev distance). 2 = 5x5 grid */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region", meta = (ClampMin = 1, ClampMax = 5))
	int32 LoadRadius = 2;

	/** Regions beyond this distance will be unloaded (should be > LoadRadius) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region", meta = (ClampMin = 2, ClampMax = 6))
	int32 UnloadRadius = 3;

	/** Smoothness for stamp blending at region edges (cm). Higher = wider blend zone */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region", meta = (ClampMin = 100, ClampMax = 10000))
	float StampSmoothness = 500.0f;

	/** Height graph to use for terrain generation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
	TSoftObjectPtr<UVoxelHeightGraph> ProceduralHeightGraph;

	/** Maximum number of regions to load per tick (throttling) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region", meta = (ClampMin = 1, ClampMax = 10))
	int32 MaxLoadsPerTick = 4;

	/** Maximum number of regions to unload per tick (throttling) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region", meta = (ClampMin = 1, ClampMax = 10))
	int32 MaxUnloadsPerTick = 4;
};

/**
 * Data for a single loaded terrain region
 */
USTRUCT()
struct FRegionData
{
	GENERATED_BODY()

	/** Region coordinate in grid space */
	FIntPoint Coordinate = FIntPoint::ZeroValue;

	/** Index in the stamp component's stamp array */
	int32 StampIndex = INDEX_NONE;

	/** Time when region was last accessed (for LRU unloading) */
	double LastAccessTime = 0.0;
};

/**
 * World Subsystem for managing infinite procedural terrain streaming.
 *
 * Uses VoxelPlugin's stamp system to dynamically load/unload terrain regions
 * as the player moves through the world. Each region is a height graph stamp
 * with automatic seamless blending via the Smoothness parameter.
 *
 * Key features:
 * - Automatic region streaming based on player position
 * - Throttled loading/unloading to prevent frame hitches
 * - Seamless terrain transitions via VoxelPlugin's built-in blending
 * - Chebyshev distance for region selection (square load pattern)
 */
UCLASS()
class TESTVEHICLEGAME_API URegionManager : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Configuration asset for region streaming.
	 * Set this before the subsystem initializes, or call SetConfig() at runtime.
	 *
	 * To create: Content Browser → Right-click → Miscellaneous → Data Asset → RegionManagerConfig
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
	TObjectPtr<URegionManagerConfig> Config;

	/** Set configuration at runtime (before VoxelWorld is ready) */
	UFUNCTION(BlueprintCallable, Category = "Region")
	void SetConfig(URegionManagerConfig* NewConfig);

	// Accessors for config values (with defaults if no config set)
	float GetRegionSizeCm() const { return Config ? Config->RegionSizeCm : 102400.0f; }
	int32 GetLoadRadius() const { return Config ? Config->LoadRadius : 2; }
	int32 GetUnloadRadius() const { return Config ? Config->UnloadRadius : 3; }
	float GetStampSmoothness() const { return Config ? Config->StampSmoothness : 500.0f; }
	int32 GetMaxLoadsPerTick() const { return Config ? Config->MaxLoadsPerTick : 4; }
	int32 GetMaxUnloadsPerTick() const { return Config ? Config->MaxUnloadsPerTick : 4; }

public:
	//~ UWorldSubsystem Interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//~ FTickableGameObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bIsInitialized && VoxelWorld.IsValid(); }
	virtual bool IsTickableInEditor() const override { return false; }

public:
	//~ Public API

	/** Convert world position to region grid coordinate */
	UFUNCTION(BlueprintPure, Category = "Region")
	FIntPoint WorldToRegionCoord(const FVector& WorldPos) const;

	/** Get world center position of a region coordinate */
	UFUNCTION(BlueprintPure, Category = "Region")
	FVector RegionCoordToWorldCenter(FIntPoint Coord) const;

	/** Get current number of loaded regions */
	UFUNCTION(BlueprintPure, Category = "Region")
	int32 GetLoadedRegionCount() const { return LoadedRegions.Num(); }

	/** Check if a specific region is loaded */
	UFUNCTION(BlueprintPure, Category = "Region")
	bool IsRegionLoaded(FIntPoint Coord) const { return LoadedRegions.Contains(Coord); }

	/** Force load a specific region (bypasses throttling) */
	UFUNCTION(BlueprintCallable, Category = "Region")
	void ForceLoadRegion(FIntPoint Coord);

	/** Get the current player region coordinate */
	UFUNCTION(BlueprintPure, Category = "Region")
	FIntPoint GetCurrentPlayerRegion() const { return CurrentPlayerRegion; }

protected:
	/** Called when VoxelWorld runtime is ready */
	void OnVoxelWorldReady();

	/** Find the VoxelWorld actor in the level */
	AVoxelWorld* FindVoxelWorld() const;

	/** Update which regions should be loaded/unloaded based on player position */
	void UpdateRegionLoadState();

	/** Load a single region at the given coordinate */
	void LoadRegion(FIntPoint Coord);

	/** Unload a single region at the given coordinate */
	void UnloadRegion(FIntPoint Coord);

	/** Calculate Chebyshev distance between two region coordinates */
	int32 RegionDistance(FIntPoint A, FIntPoint B) const;

	/** Process pending load queue */
	void ProcessPendingLoads();

	/** Process pending unload queue */
	void ProcessPendingUnloads();

private:
	/** Map of loaded regions by coordinate */
	UPROPERTY()
	TMap<FIntPoint, FRegionData> LoadedRegions;

	/** Actor that holds the stamp component */
	UPROPERTY()
	TObjectPtr<AActor> RegionHolderActor;

	/** Component managing all region stamps */
	UPROPERTY()
	TObjectPtr<UVoxelInstancedStampComponent> StampComponent;

	/** Loaded height graph asset */
	UPROPERTY()
	TObjectPtr<UVoxelHeightGraph> LoadedHeightGraph;

	/** Weak reference to the VoxelWorld actor */
	TWeakObjectPtr<AVoxelWorld> VoxelWorld;

	/** Current region coordinate of the player */
	FIntPoint CurrentPlayerRegion = FIntPoint(0, 0);

	/** Queue of regions waiting to be loaded */
	TArray<FIntPoint> PendingLoads;

	/** Queue of regions waiting to be unloaded */
	TArray<FIntPoint> PendingUnloads;

	/** Pool of free stamp indices for reuse */
	TArray<int32> FreeStampIndices;

	/** Whether the subsystem is fully initialized */
	bool bIsInitialized = false;

	/** Handle for VoxelWorld runtime created delegate */
	FDelegateHandle OnRuntimeCreatedHandle;
};
