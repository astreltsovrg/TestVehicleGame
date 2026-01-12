// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeightfieldMeshCollisionComponent.h"

#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Physics/PhysicsFiltering.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Physics/PhysicsInterfaceScene.h"
#include "Physics/Experimental/PhysScene_Chaos.h"

#include "Chaos/HeightField.h"
#include "Chaos/ImplicitObjectTransformed.h"
#include "Chaos/ImplicitObjectUnion.h"
#include "Chaos/ShapeInstance.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

// Same Z scale as Landscape
static constexpr float HEIGHTFIELD_ZSCALE = 1.0f / 128.0f;

UHeightfieldMeshCollisionComponent::UHeightfieldMeshCollisionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set collision defaults
	SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	SetGenerateOverlapEvents(false);

	// No mobility change allowed for collision components
	Mobility = EComponentMobility::Static;

	// Initialize cached bounds
	CachedLocalBox.Init();
}

UHeightfieldMeshCollisionComponent::~UHeightfieldMeshCollisionComponent()
{
	// HeightfieldGeometry is ref-counted and will be cleaned up automatically
}

void UHeightfieldMeshCollisionComponent::OnRegister()
{
	Super::OnRegister();

	// Update bounds on register
	UpdateCachedBounds();
}

FBoxSphereBounds UHeightfieldMeshCollisionComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (CachedLocalBox.IsValid)
	{
		return FBoxSphereBounds(CachedLocalBox.TransformBy(LocalToWorld));
	}

	// Fallback: calculate from texture dimensions
	if (HeightmapTexture && HeightmapTexture->GetPlatformData() && HeightmapTexture->GetPlatformData()->Mips.Num() > 0)
	{
		const FTexturePlatformData* PlatformData = HeightmapTexture->GetPlatformData();
		const int32 Width = PlatformData->Mips[0].SizeX;
		const int32 Height = PlatformData->Mips[0].SizeY;

		// Max height range (16-bit centered at 32768)
		const float MaxHeight = 32767.0f * HeightfieldScale.Z * HEIGHTFIELD_ZSCALE;

		FBox LocalBox(
			FVector(0, 0, -MaxHeight),
			FVector(
				Width * HeightfieldScale.X,
				Height * HeightfieldScale.Y,
				MaxHeight
			)
		);
		return FBoxSphereBounds(LocalBox.TransformBy(LocalToWorld));
	}

	return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.0f);
}

bool UHeightfieldMeshCollisionComponent::ShouldCreatePhysicsState() const
{
	// Only create physics if we have a valid texture
	if (!HeightmapTexture || !HeightmapTexture->GetPlatformData())
	{
		return false;
	}

	return Super::ShouldCreatePhysicsState();
}

void UHeightfieldMeshCollisionComponent::OnCreatePhysicsState()
{
	// Skip UPrimitiveComponent implementation (which uses BodySetup),
	// go directly to USceneComponent
	USceneComponent::OnCreatePhysicsState();

	if (!BodyInstance.IsValidBodyInstance())
	{
		CreateCollisionObject();
	}
}

void UHeightfieldMeshCollisionComponent::OnDestroyPhysicsState()
{
	DestroyCollisionObject();
	Super::OnDestroyPhysicsState();
}

#if WITH_EDITOR
void UHeightfieldMeshCollisionComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property ?
		PropertyChangedEvent.Property->GetFName() : NAME_None;

	// Rebuild collision when relevant properties change
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UHeightfieldMeshCollisionComponent, HeightmapTexture) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UHeightfieldMeshCollisionComponent, PhysicalMaterials) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UHeightfieldMeshCollisionComponent, HeightfieldScale))
	{
		RebuildCollision();
	}
}
#endif

void UHeightfieldMeshCollisionComponent::RebuildCollision()
{
	UpdateCachedBounds();

	if (IsPhysicsStateCreated())
	{
		RecreatePhysicsState();
	}

	// Mark render state dirty for debug visualization
	MarkRenderStateDirty();
}

void UHeightfieldMeshCollisionComponent::SetHeightmapTexture(UTexture2D* NewTexture)
{
	if (HeightmapTexture != NewTexture)
	{
		HeightmapTexture = NewTexture;
		RebuildCollision();
	}
}

bool UHeightfieldMeshCollisionComponent::ExtractHeightData(
	TArray<uint16>& OutHeights,
	TArray<uint8>& OutMaterialIndices,
	int32& OutNumRows,
	int32& OutNumCols) const
{
	if (!HeightmapTexture)
	{
		UE_LOG(LogTemp, Warning, TEXT("HeightfieldMeshCollision: No heightmap texture assigned"));
		return false;
	}

	const FTexturePlatformData* PlatformData = HeightmapTexture->GetPlatformData();
	if (!PlatformData || PlatformData->Mips.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("HeightfieldMeshCollision: Texture has no platform data"));
		return false;
	}

	// Verify format
	if (PlatformData->PixelFormat != PF_B8G8R8A8)
	{
		UE_LOG(LogTemp, Error, TEXT("HeightfieldMeshCollision: Texture must be BGRA8 format (got %d). Set CompressionSettings=VectorDisplacementmap or UserInterface2D, and SRGB=false."),
			static_cast<int32>(PlatformData->PixelFormat));
		return false;
	}

	const FTexture2DMipMap& Mip0 = PlatformData->Mips[0];
	OutNumCols = Mip0.SizeX;  // Width = Columns (X direction)
	OutNumRows = Mip0.SizeY;  // Height = Rows (Y direction)

	const int32 NumVertices = OutNumRows * OutNumCols;
	const int32 NumCells = (OutNumRows - 1) * (OutNumCols - 1);

	OutHeights.SetNumUninitialized(NumVertices);
	OutMaterialIndices.SetNumUninitialized(NumCells);

	// Lock texture data for reading
	const uint8* PixelData = static_cast<const uint8*>(Mip0.BulkData.LockReadOnly());
	if (!PixelData)
	{
		UE_LOG(LogTemp, Error, TEXT("HeightfieldMeshCollision: Failed to lock texture mip data"));
		return false;
	}

	// Extract heights from B+G channels
	// BGRA8 layout: B=0, G=1, R=2, A=3 per pixel
	for (int32 i = 0; i < NumVertices; ++i)
	{
		const int32 PixelOffset = i * 4;
		const uint8 B = PixelData[PixelOffset + 0];  // High byte of height
		const uint8 G = PixelData[PixelOffset + 1];  // Low byte of height

		OutHeights[i] = (static_cast<uint16>(B) << 8) | static_cast<uint16>(G);
	}

	// Extract material indices from R channel (per cell, not per vertex)
	// Cell [row, col] uses the pixel at vertex [row, col] for material
	const int32 NumMaterials = PhysicalMaterials.Num();
	for (int32 Row = 0; Row < OutNumRows - 1; ++Row)
	{
		for (int32 Col = 0; Col < OutNumCols - 1; ++Col)
		{
			const int32 VertexIndex = Row * OutNumCols + Col;
			const int32 CellIndex = Row * (OutNumCols - 1) + Col;
			const int32 PixelOffset = VertexIndex * 4;

			uint8 MaterialIndex = PixelData[PixelOffset + 2];  // R channel

			// Clamp to valid range, or mark as hole (0xFF) if out of range
			if (MaterialIndex >= NumMaterials && NumMaterials > 0)
			{
				MaterialIndex = 0;  // Default to first material if out of range
			}
			else if (NumMaterials == 0)
			{
				MaterialIndex = 0;  // Single default material
			}

			OutMaterialIndices[CellIndex] = MaterialIndex;
		}
	}

	Mip0.BulkData.Unlock();
	return true;
}

void UHeightfieldMeshCollisionComponent::CreateCollisionObject()
{
	// Extract height data from texture
	TArray<uint16> Heights;
	TArray<uint8> MaterialIndices;
	int32 NumRows, NumCols;

	if (!ExtractHeightData(Heights, MaterialIndices, NumRows, NumCols))
	{
		return;
	}

	// Cache dimensions
	CachedNumRows = NumRows;
	CachedNumCols = NumCols;

	// Create the Chaos heightfield
	HeightfieldGeometry = Chaos::FHeightFieldPtr(new Chaos::FHeightField(
		MakeArrayView(Heights),
		MakeArrayView(MaterialIndices),
		NumRows,
		NumCols,
		Chaos::FVec3(1.0)  // Unit scale, we apply transform via SetScale
	));

	// Calculate final scale
	FTransform ComponentTransform = GetComponentToWorld();
	FVector WorldScale = ComponentTransform.GetScale3D();

	FVector FinalScale(
		HeightfieldScale.X * WorldScale.X,
		HeightfieldScale.Y * WorldScale.Y,
		HeightfieldScale.Z * WorldScale.Z * HEIGHTFIELD_ZSCALE
	);

	HeightfieldGeometry->SetScale(FinalScale * WorldScale.GetSignVector());

	// Register physical materials
	ChaosMaterialHandles.Reset();
	if (PhysicalMaterials.Num() > 0)
	{
		for (UPhysicalMaterial* PhysMat : PhysicalMaterials)
		{
			if (PhysMat)
			{
				ChaosMaterialHandles.Add(PhysMat->GetPhysicsMaterial());
			}
			else
			{
				// Use default material for null entries
				ChaosMaterialHandles.Add(GEngine->DefaultPhysMaterial->GetPhysicsMaterial());
			}
		}
	}
	else
	{
		// Single default material if none specified
		ChaosMaterialHandles.Add(GEngine->DefaultPhysMaterial->GetPhysicsMaterial());
	}

	// Create physics actor
	FActorCreationParams Params;
	Params.InitialTM = ComponentTransform;
	Params.InitialTM.SetScale3D(FVector::OneVector);  // Scale is baked into heightfield
	Params.bQueryOnly = false;
	Params.bStatic = true;
	Params.Scene = GetWorld()->GetPhysicsScene();

#if USE_BODYINSTANCE_DEBUG_NAMES
	const FString DebugName = GetOwner() ?
		FString::Printf(TEXT("%s:%s"), *GetOwner()->GetName(), *GetName()) : *GetName();
	BodyInstance.CharDebugName = MakeShareable(new TArray<ANSICHAR>(StringToArray<ANSICHAR>(*DebugName, DebugName.Len() + 1)));
	Params.DebugName = BodyInstance.CharDebugName.IsValid() ? BodyInstance.CharDebugName->GetData() : nullptr;
#endif

	FPhysicsActorHandle PhysHandle;
	FPhysicsInterface::CreateActor(Params, PhysHandle);

	if (!FPhysicsInterface::IsValid(PhysHandle))
	{
		UE_LOG(LogTemp, Error, TEXT("HeightfieldMeshCollision: Failed to create physics actor"));
		return;
	}

	Chaos::FRigidBodyHandle_External& Body_External = PhysHandle->GetGameThreadAPI();

	// Wrap heightfield in transformed implicit object
	Chaos::FImplicitObjectPtr ImplicitHeightField(HeightfieldGeometry);
	Chaos::FImplicitObjectPtr TransformedHeightField = MakeImplicitObjectPtr<Chaos::TImplicitObjectTransformed<Chaos::FReal, 3>>(
		ImplicitHeightField,
		Chaos::FRigidTransform3(FTransform::Identity)
	);

	// Create shape
	Chaos::FShapesArray ShapeArray;
	TUniquePtr<Chaos::FPerShapeData> NewShape = Chaos::FShapeInstanceProxy::Make(ShapeArray.Num(), TransformedHeightField);

	// Setup collision filtering
	FCollisionFilterData QueryFilterData, SimFilterData;
	CreateShapeFilterData(
		static_cast<uint8>(GetCollisionObjectType()),
		FMaskFilter(0),
		GetOwner() ? GetOwner()->GetUniqueID() : 0,
		GetCollisionResponseToChannels(),
		GetUniqueID(),
		0,
		QueryFilterData,
		SimFilterData,
		false,  // bEnableCCD
		BodyInstance.bNotifyRigidBodyCollision,
		true    // bStaticShape
	);

	// Mark as both simple and complex collision
	QueryFilterData.Word3 |= (EPDF_SimpleCollision | EPDF_ComplexCollision);
	SimFilterData.Word3 |= (EPDF_SimpleCollision | EPDF_ComplexCollision);

	NewShape->SetQueryData(QueryFilterData);
	NewShape->SetSimData(SimFilterData);
	NewShape->SetMaterials(ChaosMaterialHandles);

	// Set geometry on body
	Body_External.SetGeometry(TransformedHeightField);

	// Update shape bounds
	Chaos::FRigidTransform3 WorldTransform(Body_External.X(), Body_External.R());
	NewShape->UpdateShapeBounds(WorldTransform);
	ShapeArray.Emplace(MoveTemp(NewShape));
	Body_External.MergeShapesArray(MoveTemp(ShapeArray));

	// Setup body instance
	BodyInstance.PhysicsUserData = FPhysicsUserData(&BodyInstance);
	BodyInstance.OwnerComponent = this;
	BodyInstance.SetPhysicsActor(PhysHandle);
	Body_External.SetUserData(&BodyInstance.PhysicsUserData);

	// Add to physics scene
	FPhysScene* PhysScene = GetWorld()->GetPhysicsScene();
	if (PhysScene)
	{
		FPhysicsCommand::ExecuteWrite(PhysScene, [PhysHandle, PhysScene]()
		{
			TArray<FPhysicsActorHandle> Actors = { PhysHandle };
			const bool bImmediateAccelStructureInsertion = true;
			PhysScene->AddActorsToScene_AssumesLocked(Actors, bImmediateAccelStructureInsertion);
		});

		PhysScene->AddToComponentMaps(this, PhysHandle);

		if (BodyInstance.bNotifyRigidBodyCollision)
		{
			PhysScene->RegisterForCollisionEvents(this);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("HeightfieldMeshCollision: Created heightfield %dx%d with scale (%f, %f, %f)"),
		NumCols, NumRows, FinalScale.X, FinalScale.Y, FinalScale.Z);
}

void UHeightfieldMeshCollisionComponent::DestroyCollisionObject()
{
	if (UWorld* World = GetWorld())
	{
		if (FPhysScene_Chaos* PhysScene = World->GetPhysicsScene())
		{
			FPhysicsActorHandle ActorHandle = BodyInstance.GetPhysicsActor();
			if (FPhysicsInterface::IsValid(ActorHandle))
			{
				PhysScene->RemoveFromComponentMaps(ActorHandle);
			}

			if (BodyInstance.bNotifyRigidBodyCollision)
			{
				PhysScene->UnRegisterForCollisionEvents(this);
			}
		}
	}

	HeightfieldGeometry = nullptr;
	ChaosMaterialHandles.Empty();
}

void UHeightfieldMeshCollisionComponent::UpdateCachedBounds()
{
	if (!HeightmapTexture || !HeightmapTexture->GetPlatformData() || HeightmapTexture->GetPlatformData()->Mips.Num() == 0)
	{
		CachedLocalBox.Init();
		return;
	}

	const FTexturePlatformData* PlatformData = HeightmapTexture->GetPlatformData();
	const int32 Width = PlatformData->Mips[0].SizeX;
	const int32 Height = PlatformData->Mips[0].SizeY;

	// Max height range (16-bit centered at 32768)
	const float MaxHeight = 32767.0f * HeightfieldScale.Z * HEIGHTFIELD_ZSCALE;

	CachedLocalBox = FBox(
		FVector(0, 0, -MaxHeight),
		FVector(
			Width * HeightfieldScale.X,
			Height * HeightfieldScale.Y,
			MaxHeight
		)
	);

	UpdateBounds();
}

void UHeightfieldMeshCollisionComponent::UpdateHeightfieldRegion(
	const TArray<float>& Heights,
	int32 StartRow, int32 StartCol,
	int32 NumRows, int32 NumCols)
{
	if (Heights.Num() != NumRows * NumCols)
	{
		UE_LOG(LogTemp, Warning, TEXT("HeightfieldMeshCollision: Heights array size (%d) doesn't match region size (%d x %d = %d)"),
			Heights.Num(), NumRows, NumCols, NumRows * NumCols);
		return;
	}

	// Convert float heights to uint16 format
	// Float input: -256 to +256 range maps to uint16 0-65535 (centered at 32768)
	TArray<uint16> Heights16;
	Heights16.SetNumUninitialized(Heights.Num());

	for (int32 i = 0; i < Heights.Num(); ++i)
	{
		// Convert from world-scale height to uint16
		// Reverse of: LocalHeight = (uint16Height - 32768) * HeightfieldScale.Z * HEIGHTFIELD_ZSCALE
		const float ScaledHeight = Heights[i] / (HeightfieldScale.Z * HEIGHTFIELD_ZSCALE);
		const int32 IntHeight = FMath::RoundToInt(ScaledHeight + 32768.0f);
		Heights16[i] = static_cast<uint16>(FMath::Clamp(IntHeight, 0, 65535));
	}

	UpdateHeightfieldRegionRaw(Heights16, StartRow, StartCol, NumRows, NumCols);
}

void UHeightfieldMeshCollisionComponent::UpdateHeightfieldRegionRaw(
	TArrayView<const uint16> Heights,
	int32 StartRow, int32 StartCol,
	int32 NumRows, int32 NumCols)
{
	if (!HeightfieldGeometry)
	{
		UE_LOG(LogTemp, Warning, TEXT("HeightfieldMeshCollision: No heightfield geometry to update"));
		return;
	}

	FPhysicsActorHandle PhysActorHandle = BodyInstance.GetPhysicsActor();
	if (!FPhysicsInterface::IsValid(PhysActorHandle))
	{
		UE_LOG(LogTemp, Warning, TEXT("HeightfieldMeshCollision: No physics actor to update"));
		return;
	}

	// Validate region bounds
	if (StartRow < 0 || StartCol < 0 ||
		StartRow + NumRows > CachedNumRows ||
		StartCol + NumCols > CachedNumCols)
	{
		UE_LOG(LogTemp, Warning, TEXT("HeightfieldMeshCollision: Update region (%d,%d) + (%d,%d) out of bounds (%d,%d)"),
			StartRow, StartCol, NumRows, NumCols, CachedNumRows, CachedNumCols);
		return;
	}

	FPhysicsCommand::ExecuteWrite(PhysActorHandle, [&](const FPhysicsActorHandle& Actor)
	{
		// Update the heightfield data
		HeightfieldGeometry->EditHeights(Heights, StartRow, StartCol, NumRows, NumCols);

		// Rebuild geometry wrapper to update local bounds
		Chaos::FRigidBodyHandle_External& Body_External = Actor->GetGameThreadAPI();
		const Chaos::FImplicitObject* CurrentGeom = Body_External.GetGeometry();

		if (CurrentGeom)
		{
			// Get the transformed heightfield and recreate it to update bounds
			if (const auto* TransformedHF = CurrentGeom->template GetObject<Chaos::TImplicitObjectTransformed<Chaos::FReal, 3>>())
			{
				Chaos::FImplicitObjectPtr NewGeom = MakeImplicitObjectPtr<Chaos::TImplicitObjectTransformed<Chaos::FReal, 3>>(
					TransformedHF->GetGeometry(),
					TransformedHF->GetTransform()
				);
				Body_External.SetGeometry(NewGeom);
			}
		}

		// Update acceleration structure
		FPhysScene* PhysScene = GetWorld()->GetPhysicsScene();
		if (PhysScene)
		{
			PhysScene->UpdateActorInAccelerationStructure(Actor);
		}
	});
}
