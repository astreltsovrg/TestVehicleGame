// Copyright Epic Games, Inc. All Rights Reserved.

#include "RegionManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

// VoxelPlugin includes
#include "VoxelWorld.h"
#include "VoxelInstancedStampComponent.h"
#include "VoxelHeightBlendMode.h"
#include "Graphs/VoxelHeightGraph.h"
#include "Graphs/VoxelHeightGraphStamp.h"
#include "VoxelStampRef.h"

// For TActorIterator
#include "EngineUtils.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRegionManager, Log, All);
DEFINE_LOG_CATEGORY(LogRegionManager);

void URegionManager::SetConfig(URegionManagerConfig* NewConfig)
{
	if (!NewConfig)
	{
		UE_LOG(LogRegionManager, Warning, TEXT("SetConfig called with null config"));
		return;
	}

	if (bIsInitialized)
	{
		UE_LOG(LogRegionManager, Warning, TEXT("SetConfig called but RegionManager already initialized"));
		return;
	}

	Config = NewConfig;
	UE_LOG(LogRegionManager, Log, TEXT("Config set: %s"), *GetNameSafe(Config));

	// Load the height graph
	if (!Config->ProceduralHeightGraph.IsNull())
	{
		LoadedHeightGraph = Config->ProceduralHeightGraph.LoadSynchronous();
		if (LoadedHeightGraph)
		{
			UE_LOG(LogRegionManager, Log, TEXT("Loaded height graph: %s"), *GetNameSafe(LoadedHeightGraph));
		}
		else
		{
			UE_LOG(LogRegionManager, Error, TEXT("Failed to load ProceduralHeightGraph from config"));
			return;
		}
	}
	else
	{
		UE_LOG(LogRegionManager, Error, TEXT("ProceduralHeightGraph not set in config"));
		return;
	}

	// Find VoxelWorld in the level (actors exist now since we're in BeginPlay)
	VoxelWorld = FindVoxelWorld();
	if (!VoxelWorld.IsValid())
	{
		UE_LOG(LogRegionManager, Error, TEXT("No AVoxelWorld found in level - place one in your level"));
		return;
	}

	UE_LOG(LogRegionManager, Log, TEXT("Found VoxelWorld: %s"), *GetNameSafe(VoxelWorld.Get()));

	// Check if VoxelWorld runtime is ready
	if (VoxelWorld->IsRuntimeCreated())
	{
		OnVoxelWorldReady();
	}
	else
	{
		// Wait for runtime to be created
		OnRuntimeCreatedHandle = VoxelWorld->OnRuntimeCreated.AddUObject(this, &URegionManager::OnVoxelWorldReady);
		UE_LOG(LogRegionManager, Log, TEXT("Waiting for VoxelWorld runtime..."));
	}
}

bool URegionManager::ShouldCreateSubsystem(UObject* Outer) const
{
	// Only create in game worlds, not in editor preview worlds
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->IsGameWorld();
	}
	return false;
}

void URegionManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogRegionManager, Log, TEXT("RegionManager subsystem created - waiting for SetConfig() from Level Blueprint"));
	// VoxelWorld and actors don't exist yet at this point
	// Everything will be initialized when SetConfig() is called from Level Blueprint's BeginPlay
}

void URegionManager::Deinitialize()
{
	UE_LOG(LogRegionManager, Log, TEXT("Deinitializing RegionManager subsystem"));

	// Remove delegate if still bound
	if (VoxelWorld.IsValid() && OnRuntimeCreatedHandle.IsValid())
	{
		VoxelWorld->OnRuntimeCreated.Remove(OnRuntimeCreatedHandle);
		OnRuntimeCreatedHandle.Reset();
	}

	// Clear all stamps
	if (StampComponent)
	{
		StampComponent->ClearStamps();
	}

	// Destroy holder actor
	if (RegionHolderActor)
	{
		RegionHolderActor->Destroy();
		RegionHolderActor = nullptr;
	}

	LoadedRegions.Empty();
	PendingLoads.Empty();
	PendingUnloads.Empty();
	FreeStampIndices.Empty();
	bIsInitialized = false;

	Super::Deinitialize();
}

void URegionManager::OnVoxelWorldReady()
{
	UE_LOG(LogRegionManager, Log, TEXT("VoxelWorld runtime ready, initializing region streaming..."));

	if (bIsInitialized)
	{
		UE_LOG(LogRegionManager, Warning, TEXT("OnVoxelWorldReady called but already initialized"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogRegionManager, Error, TEXT("Cannot initialize - no world"));
		return;
	}

	// Spawn holder actor for the stamp component
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = FName(TEXT("RegionManager_StampHolder"));
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	RegionHolderActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!RegionHolderActor)
	{
		UE_LOG(LogRegionManager, Error, TEXT("Failed to spawn RegionHolderActor"));
		return;
	}

	// Set a root component so the stamp component can be attached
	USceneComponent* RootComp = NewObject<USceneComponent>(RegionHolderActor, TEXT("RootComponent"));
	RootComp->RegisterComponent();
	RegionHolderActor->SetRootComponent(RootComp);

	// Create the instanced stamp component
	StampComponent = NewObject<UVoxelInstancedStampComponent>(RegionHolderActor, TEXT("StampComponent"));
	if (!StampComponent)
	{
		UE_LOG(LogRegionManager, Error, TEXT("Failed to create UVoxelInstancedStampComponent"));
		return;
	}

	StampComponent->SetupAttachment(RootComp);
	StampComponent->RegisterComponent();

	bIsInitialized = true;

	// Initial region update
	UpdateRegionLoadState();

	UE_LOG(LogRegionManager, Log, TEXT("RegionManager fully initialized"));
}

AVoxelWorld* URegionManager::FindVoxelWorld() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Find the first VoxelWorld actor in the level
	for (TActorIterator<AVoxelWorld> It(World); It; ++It)
	{
		return *It;
	}

	return nullptr;
}

void URegionManager::Tick(float DeltaTime)
{
	if (!bIsInitialized || !StampComponent)
	{
		return;
	}

	// Get player position
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	APawn* PlayerPawn = PC->GetPawn();
	if (!PlayerPawn)
	{
		return;
	}

	// Calculate current player region
	FIntPoint NewRegion = WorldToRegionCoord(PlayerPawn->GetActorLocation());

	// Only update if player moved to a new region
	if (NewRegion != CurrentPlayerRegion)
	{
		UE_LOG(LogRegionManager, Verbose, TEXT("Player moved to region (%d, %d)"), NewRegion.X, NewRegion.Y);
		CurrentPlayerRegion = NewRegion;
		UpdateRegionLoadState();
	}

	// Process throttled loading/unloading
	ProcessPendingLoads();
	ProcessPendingUnloads();
}

TStatId URegionManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URegionManager, STATGROUP_Tickables);
}

FIntPoint URegionManager::WorldToRegionCoord(const FVector& WorldPos) const
{
	// Convert world position to region coordinate
	// Region (0,0) is centered at world origin
	const float RegionSize = GetRegionSizeCm();
	return FIntPoint(
		FMath::FloorToInt((WorldPos.X + RegionSize * 0.5f) / RegionSize),
		FMath::FloorToInt((WorldPos.Y + RegionSize * 0.5f) / RegionSize)
	);
}

FVector URegionManager::RegionCoordToWorldCenter(FIntPoint Coord) const
{
	// Get the center position of a region in world space
	const float RegionSize = GetRegionSizeCm();
	return FVector(
		Coord.X * RegionSize,
		Coord.Y * RegionSize,
		0.0f
	);
}

void URegionManager::UpdateRegionLoadState()
{
	// Clear pending queues - we're recalculating
	PendingLoads.Empty();
	PendingUnloads.Empty();

	const int32 CurrentLoadRadius = GetLoadRadius();
	const int32 CurrentUnloadRadius = GetUnloadRadius();

	// Determine which regions should be loaded (within LoadRadius)
	TSet<FIntPoint> DesiredRegions;
	for (int32 X = -CurrentLoadRadius; X <= CurrentLoadRadius; ++X)
	{
		for (int32 Y = -CurrentLoadRadius; Y <= CurrentLoadRadius; ++Y)
		{
			DesiredRegions.Add(CurrentPlayerRegion + FIntPoint(X, Y));
		}
	}

	// Queue regions that need to be loaded
	for (const FIntPoint& Coord : DesiredRegions)
	{
		if (!LoadedRegions.Contains(Coord))
		{
			PendingLoads.Add(Coord);
		}
		else
		{
			// Update access time for existing region
			LoadedRegions[Coord].LastAccessTime = GetWorld()->GetTimeSeconds();
		}
	}

	// Queue regions that are too far away for unloading
	for (auto& Pair : LoadedRegions)
	{
		const FIntPoint& Coord = Pair.Key;
		int32 Distance = RegionDistance(Coord, CurrentPlayerRegion);

		if (Distance > CurrentUnloadRadius)
		{
			PendingUnloads.Add(Coord);
		}
	}

	// Sort pending loads by distance (closest first)
	PendingLoads.Sort([this](const FIntPoint& A, const FIntPoint& B)
	{
		return RegionDistance(A, CurrentPlayerRegion) < RegionDistance(B, CurrentPlayerRegion);
	});

	// Sort pending unloads by distance (farthest first)
	PendingUnloads.Sort([this](const FIntPoint& A, const FIntPoint& B)
	{
		return RegionDistance(A, CurrentPlayerRegion) > RegionDistance(B, CurrentPlayerRegion);
	});

	UE_LOG(LogRegionManager, Verbose, TEXT("Region state update: %d to load, %d to unload, %d currently loaded"),
		PendingLoads.Num(), PendingUnloads.Num(), LoadedRegions.Num());
}

void URegionManager::ProcessPendingLoads()
{
	int32 LoadsThisTick = FMath::Min(GetMaxLoadsPerTick(), PendingLoads.Num());

	for (int32 i = 0; i < LoadsThisTick; ++i)
	{
		FIntPoint Coord = PendingLoads[0];
		PendingLoads.RemoveAt(0);
		LoadRegion(Coord);
	}
}

void URegionManager::ProcessPendingUnloads()
{
	int32 UnloadsThisTick = FMath::Min(GetMaxUnloadsPerTick(), PendingUnloads.Num());

	for (int32 i = 0; i < UnloadsThisTick; ++i)
	{
		FIntPoint Coord = PendingUnloads[0];
		PendingUnloads.RemoveAt(0);
		UnloadRegion(Coord);
	}
}

void URegionManager::LoadRegion(FIntPoint Coord)
{
	if (LoadedRegions.Contains(Coord))
	{
		UE_LOG(LogRegionManager, Warning, TEXT("Attempted to load already loaded region (%d, %d)"), Coord.X, Coord.Y);
		return;
	}

	if (!StampComponent || !LoadedHeightGraph)
	{
		UE_LOG(LogRegionManager, Error, TEXT("Cannot load region - missing components"));
		return;
	}

	// Create the height graph stamp
	FVoxelHeightGraphStamp Stamp;
	Stamp.Graph = LoadedHeightGraph;
	Stamp.Smoothness = GetStampSmoothness();
	Stamp.BlendMode = EVoxelHeightBlendMode::Max;
	Stamp.Priority = 0; // Same priority for all regions

	// Set transform to position the stamp at the region center
	FVector RegionCenter = RegionCoordToWorldCenter(Coord);
	Stamp.Transform = FTransform(FRotator::ZeroRotator, RegionCenter, FVector::OneVector);

	// Create stamp reference and add to component
	FVoxelStampRef StampRef = FVoxelStampRef::New(Stamp);

	int32 StampIndex;
	if (FreeStampIndices.Num() > 0)
	{
		// Reuse a freed index
		StampIndex = FreeStampIndices.Pop();
		StampComponent->SetStamp(StampIndex, StampRef);
	}
	else
	{
		// Add new stamp
		StampIndex = StampComponent->AddStamp(StampRef);
	}

	// Store region data
	FRegionData RegionData;
	RegionData.Coordinate = Coord;
	RegionData.StampIndex = StampIndex;
	RegionData.LastAccessTime = GetWorld()->GetTimeSeconds();

	LoadedRegions.Add(Coord, RegionData);

	UE_LOG(LogRegionManager, Log, TEXT("Loaded region (%d, %d) at stamp index %d, center: (%.0f, %.0f)"),
		Coord.X, Coord.Y, StampIndex, RegionCenter.X, RegionCenter.Y);
}

void URegionManager::UnloadRegion(FIntPoint Coord)
{
	FRegionData* RegionData = LoadedRegions.Find(Coord);
	if (!RegionData)
	{
		UE_LOG(LogRegionManager, Warning, TEXT("Attempted to unload non-existent region (%d, %d)"), Coord.X, Coord.Y);
		return;
	}

	if (!StampComponent)
	{
		UE_LOG(LogRegionManager, Error, TEXT("Cannot unload region - StampComponent is null"));
		return;
	}

	int32 StampIndex = RegionData->StampIndex;

	// Remove the stamp (this clears it, indices don't change)
	StampComponent->RemoveStamp(StampIndex);

	// Add index to free pool for reuse
	FreeStampIndices.Add(StampIndex);

	// Remove from loaded regions
	LoadedRegions.Remove(Coord);

	UE_LOG(LogRegionManager, Log, TEXT("Unloaded region (%d, %d), freed stamp index %d"),
		Coord.X, Coord.Y, StampIndex);
}

int32 URegionManager::RegionDistance(FIntPoint A, FIntPoint B) const
{
	// Chebyshev distance (chessboard distance) - max of X and Y differences
	return FMath::Max(FMath::Abs(A.X - B.X), FMath::Abs(A.Y - B.Y));
}

void URegionManager::ForceLoadRegion(FIntPoint Coord)
{
	if (LoadedRegions.Contains(Coord))
	{
		UE_LOG(LogRegionManager, Verbose, TEXT("Region (%d, %d) already loaded"), Coord.X, Coord.Y);
		return;
	}

	// Remove from pending queue if present
	PendingLoads.Remove(Coord);

	// Load immediately
	LoadRegion(Coord);
}
