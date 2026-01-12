// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeightfieldMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "PrimitiveSceneProxy.h"
#include "DynamicMeshBuilder.h"
#include "SceneManagement.h"
#include "Engine/Engine.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"

// Z scale factor for heightfield (matches landscape and collision component)
static constexpr float MESH_HEIGHTFIELD_ZSCALE = 1.0f / 128.0f;

/** Scene proxy for rendering the heightfield mesh */
class FHeightfieldMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
	FHeightfieldMeshSceneProxy(UHeightfieldMeshComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetShaderPlatform()))
		, VertexFactory(GetScene().GetFeatureLevel(), "FHeightfieldMeshVertexFactory")
	{
		// Copy mesh data from component
		Component->GetMeshData(Vertices, Indices, Normals, UVs);

		// Get material
		Material = Component->GetMaterial(0);
		if (!Material)
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		// Build vertex buffer
		VertexBuffers.InitWithDummyData(&VertexFactory, Vertices.Num(), 2);

		// Update vertex buffer with actual data
		if (Vertices.Num() > 0)
		{
			for (int32 i = 0; i < Vertices.Num(); ++i)
			{
				VertexBuffers.PositionVertexBuffer.VertexPosition(i) = FVector3f(Vertices[i]);
				VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(
					i,
					FVector3f(1, 0, 0),
					FVector3f(0, 1, 0),
					FVector3f(Normals.IsValidIndex(i) ? Normals[i] : FVector::UpVector)
				);
				VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(i, 0, FVector2f(UVs.IsValidIndex(i) ? UVs[i] : FVector2D::ZeroVector));
				VertexBuffers.ColorVertexBuffer.VertexColor(i) = FColor::White;
			}
		}

		// Initialize index buffer
		IndexBuffer.Indices = Indices;

		// Enqueue initialization on render thread
		BeginInitResource(&VertexBuffers.PositionVertexBuffer);
		BeginInitResource(&VertexBuffers.StaticMeshVertexBuffer);
		BeginInitResource(&VertexBuffers.ColorVertexBuffer);
		BeginInitResource(&IndexBuffer);
		BeginInitResource(&VertexFactory);
	}

	virtual ~FHeightfieldMeshSceneProxy()
	{
		VertexBuffers.PositionVertexBuffer.ReleaseResource();
		VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
		VertexBuffers.ColorVertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
	}

	virtual SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		if (Vertices.Num() == 0 || Indices.Num() == 0)
		{
			return;
		}

		const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

		FMaterialRenderProxy* MaterialProxy = Material->GetRenderProxy();

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				FMeshBatch& Mesh = Collector.AllocateMesh();
				FMeshBatchElement& BatchElement = Mesh.Elements[0];

				BatchElement.IndexBuffer = &IndexBuffer;
				Mesh.bWireframe = bWireframe;
				Mesh.VertexFactory = &VertexFactory;
				Mesh.MaterialRenderProxy = MaterialProxy;

				FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
				DynamicPrimitiveUniformBuffer.Set(Collector.GetRHICommandList(), GetLocalToWorld(), GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, false, AlwaysHasVelocity());
				BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

				BatchElement.FirstIndex = 0;
				BatchElement.NumPrimitives = Indices.Num() / 3;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = Vertices.Num() - 1;
				Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
				Mesh.Type = PT_TriangleList;
				Mesh.DepthPriorityGroup = SDPG_World;
				Mesh.bCanApplyViewModeOverrides = true;

				Collector.AddMesh(ViewIndex, Mesh);
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
		Result.bRenderInMainPass = ShouldRenderInMainPass();
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		Result.bRenderCustomDepth = ShouldRenderCustomDepth();
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}

	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}

private:
	TArray<FVector> Vertices;
	TArray<uint32> Indices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;

	UMaterialInterface* Material;
	FMaterialRelevance MaterialRelevance;

	FStaticMeshVertexBuffers VertexBuffers;
	FDynamicMeshIndexBuffer32 IndexBuffer;
	FLocalVertexFactory VertexFactory;
};

UHeightfieldMeshComponent::UHeightfieldMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	CachedLocalBounds.Init();
}

FPrimitiveSceneProxy* UHeightfieldMeshComponent::CreateSceneProxy()
{
	if (Vertices.Num() == 0)
	{
		RebuildMesh();
	}

	if (Vertices.Num() > 0 && Indices.Num() > 0)
	{
		return new FHeightfieldMeshSceneProxy(this);
	}

	return nullptr;
}

int32 UHeightfieldMeshComponent::GetNumMaterials() const
{
	return 1;
}

UMaterialInterface* UHeightfieldMeshComponent::GetMaterial(int32 MaterialIndex) const
{
	if (MaterialIndex == 0 && TerrainMaterial)
	{
		return TerrainMaterial;
	}
	return UMaterial::GetDefaultMaterial(MD_Surface);
}

void UHeightfieldMeshComponent::SetMaterial(int32 ElementIndex, UMaterialInterface* Material)
{
	if (ElementIndex == 0)
	{
		TerrainMaterial = Material;
		MarkRenderStateDirty();
	}
}

FBoxSphereBounds UHeightfieldMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (CachedLocalBounds.IsValid)
	{
		return FBoxSphereBounds(CachedLocalBounds.TransformBy(LocalToWorld));
	}
	return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.0f);
}

#if WITH_EDITOR
void UHeightfieldMeshComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property ?
		PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UHeightfieldMeshComponent, HeightmapTexture) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UHeightfieldMeshComponent, HeightfieldScale) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UHeightfieldMeshComponent, LODFactor))
	{
		RebuildMesh();
	}
}
#endif

void UHeightfieldMeshComponent::RebuildMesh()
{
	Vertices.Empty();
	Indices.Empty();
	Normals.Empty();
	UVs.Empty();
	CachedHeights.Empty();

	if (!HeightmapTexture || !HeightmapTexture->GetPlatformData())
	{
		MarkRenderStateDirty();
		return;
	}

	const FTexturePlatformData* PlatformData = HeightmapTexture->GetPlatformData();
	if (PlatformData->Mips.Num() == 0 || PlatformData->PixelFormat != PF_B8G8R8A8)
	{
		UE_LOG(LogTemp, Warning, TEXT("HeightfieldMesh: Invalid texture format"));
		MarkRenderStateDirty();
		return;
	}

	const FTexture2DMipMap& Mip0 = PlatformData->Mips[0];
	TextureWidth = Mip0.SizeX;
	TextureHeight = Mip0.SizeY;

	// Lock and read texture data
	const uint8* PixelData = static_cast<const uint8*>(Mip0.BulkData.LockReadOnly());
	if (!PixelData)
	{
		MarkRenderStateDirty();
		return;
	}

	// Cache all heights
	CachedHeights.SetNumUninitialized(TextureWidth * TextureHeight);
	for (int32 i = 0; i < CachedHeights.Num(); ++i)
	{
		const int32 PixelOffset = i * 4;
		const uint8 B = PixelData[PixelOffset + 0];
		const uint8 G = PixelData[PixelOffset + 1];
		CachedHeights[i] = (static_cast<uint16>(B) << 8) | static_cast<uint16>(G);
	}

	Mip0.BulkData.Unlock();

	// Generate mesh with LOD
	const int32 StepSize = FMath::Max(1, LODFactor);
	const int32 VertsX = (TextureWidth + StepSize - 1) / StepSize;
	const int32 VertsY = (TextureHeight + StepSize - 1) / StepSize;

	// Reserve memory
	Vertices.Reserve(VertsX * VertsY);
	Normals.Reserve(VertsX * VertsY);
	UVs.Reserve(VertsX * VertsY);
	Indices.Reserve((VertsX - 1) * (VertsY - 1) * 6);

	// Generate vertices
	float MinZ = MAX_FLT, MaxZ = -MAX_FLT;
	for (int32 Y = 0; Y < VertsY; ++Y)
	{
		for (int32 X = 0; X < VertsX; ++X)
		{
			const int32 TexX = FMath::Min(X * StepSize, TextureWidth - 1);
			const int32 TexY = FMath::Min(Y * StepSize, TextureHeight - 1);

			const float Height = GetHeightAt(TexX, TexY);
			const FVector Position(
				TexX * HeightfieldScale.X,
				TexY * HeightfieldScale.Y,
				Height
			);

			Vertices.Add(Position);
			Normals.Add(CalculateNormalAt(TexX, TexY));
			UVs.Add(FVector2D(
				static_cast<float>(TexX) / (TextureWidth - 1),
				static_cast<float>(TexY) / (TextureHeight - 1)
			));

			MinZ = FMath::Min(MinZ, Height);
			MaxZ = FMath::Max(MaxZ, Height);
		}
	}

	// Generate indices (two triangles per quad)
	for (int32 Y = 0; Y < VertsY - 1; ++Y)
	{
		for (int32 X = 0; X < VertsX - 1; ++X)
		{
			const int32 TopLeft = Y * VertsX + X;
			const int32 TopRight = TopLeft + 1;
			const int32 BottomLeft = TopLeft + VertsX;
			const int32 BottomRight = BottomLeft + 1;

			// First triangle
			Indices.Add(TopLeft);
			Indices.Add(BottomLeft);
			Indices.Add(TopRight);

			// Second triangle
			Indices.Add(TopRight);
			Indices.Add(BottomLeft);
			Indices.Add(BottomRight);
		}
	}

	// Update cached bounds
	CachedLocalBounds = FBox(
		FVector(0, 0, MinZ),
		FVector(TextureWidth * HeightfieldScale.X, TextureHeight * HeightfieldScale.Y, MaxZ)
	);

	UE_LOG(LogTemp, Log, TEXT("HeightfieldMesh: Built mesh with %d vertices, %d triangles"),
		Vertices.Num(), Indices.Num() / 3);

	UpdateBounds();
	MarkRenderStateDirty();
}

void UHeightfieldMeshComponent::UpdateMeshRegion(int32 StartRow, int32 StartCol, int32 NumRows, int32 NumCols)
{
	// For now, just rebuild the whole mesh
	// TODO: Implement partial update for better performance
	RebuildMesh();
}

void UHeightfieldMeshComponent::GetMeshData(
	TArray<FVector>& OutVertices,
	TArray<uint32>& OutIndices,
	TArray<FVector>& OutNormals,
	TArray<FVector2D>& OutUVs) const
{
	OutVertices = Vertices;
	OutIndices = Indices;
	OutNormals = Normals;
	OutUVs = UVs;
}

float UHeightfieldMeshComponent::GetHeightAt(int32 X, int32 Y) const
{
	if (X < 0 || X >= TextureWidth || Y < 0 || Y >= TextureHeight)
	{
		return 0.0f;
	}

	const int32 Index = Y * TextureWidth + X;
	const uint16 HeightValue = CachedHeights[Index];

	// Convert from uint16 to world height
	// Same formula as collision component
	return (static_cast<float>(HeightValue) - 32768.0f) * HeightfieldScale.Z * MESH_HEIGHTFIELD_ZSCALE;
}

FVector UHeightfieldMeshComponent::CalculateNormalAt(int32 X, int32 Y) const
{
	// Sample neighboring heights for normal calculation
	const float Left = GetHeightAt(X - 1, Y);
	const float Right = GetHeightAt(X + 1, Y);
	const float Up = GetHeightAt(X, Y - 1);
	const float Down = GetHeightAt(X, Y + 1);

	// Calculate normal from height differences
	const FVector Normal(
		(Left - Right) / (2.0f * HeightfieldScale.X),
		(Up - Down) / (2.0f * HeightfieldScale.Y),
		1.0f
	);

	return Normal.GetSafeNormal();
}
