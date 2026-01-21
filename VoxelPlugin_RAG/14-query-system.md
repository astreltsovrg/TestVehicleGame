# Query System

## Summary

VoxelPlugin2's query system provides methods to sample terrain data at runtime. Queries can retrieve height values, distances to surface, surface types, normals, and custom metadata. The system supports both single-point queries and bulk/sparse queries for performance-critical scenarios.

---

## Key Classes

| Class | Header | Purpose |
|-------|--------|---------|
| `FVoxelQuery` | VoxelQuery.h | Base query context |
| `FVoxelHeightBulkQuery` | VoxelStampQuery.h | Grid-based height queries |
| `FVoxelVolumeBulkQuery` | VoxelStampQuery.h | Grid-based volume queries |
| `FVoxelHeightSparseQuery` | VoxelStampQuery.h | Point-based height queries |
| `FVoxelVolumeSparseQuery` | VoxelStampQuery.h | Point-based volume queries |
| `FVoxelQueryResult` | VoxelQueryBlueprintLibrary.h | Single query result struct |
| `UVoxelQueryBlueprintLibrary` | VoxelQueryBlueprintLibrary.h | Blueprint query functions |
| `FVoxelQueryDebugDrawer` | VoxelQueryDebugDrawer.h | Debug visualization |
| `EVoxelFloatQueryType` | VoxelQueryBlueprintLibrary.h | Query type enum |

---

## FVoxelQuery Base Class

Core query context used internally by all query operations.

```cpp
class VOXEL_API FVoxelQuery
{
public:
    // LOD level for sampling
    const int32 LOD;

    // Access to layer data
    const FVoxelLayers& Layers;

    // Surface type resolution
    const FVoxelSurfaceTypeTable& SurfaceTypeTable;

    // Dependency tracking
    FVoxelDependencyCollector& DependencyCollector;

    // Debug breadcrumbs
    FVoxelBreadcrumbs* const Breadcrumbs;

public:
    // Sample height layer at grid positions
    FVoxelFloatBuffer SampleHeightLayer(
        const FVoxelWeakStackLayer& WeakLayer,
        const FVector2D& Start,
        const FIntPoint& Size,
        float Step) const;

    // Sample height layer at sparse positions
    FVoxelFloatBuffer SampleHeightLayer(
        const FVoxelWeakStackLayer& WeakLayer,
        const FVoxelDoubleVector2DBuffer& Positions) const;

    // Sample volume layer at grid positions
    FVoxelFloatBuffer SampleVolumeLayer(
        const FVoxelWeakStackLayer& WeakLayer,
        const FVector& Start,
        const FIntVector& Size,
        float Step) const;

    // Sample volume layer at sparse positions
    FVoxelFloatBuffer SampleVolumeLayer(
        const FVoxelWeakStackLayer& WeakLayer,
        const FVoxelDoubleVectorBuffer& Positions) const;
};
```

---

## FVoxelQueryResult

Structure returned by Blueprint query functions.

```cpp
USTRUCT(BlueprintType)
struct VOXEL_API FVoxelQueryResult
{
    GENERATED_BODY()

    // Height or distance to surface (based on layer type)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    float Value = 0.f;

    // Surface normal at query location
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    FVector Normal = FVector(ForceInit);

    // Surface type before smart resolution
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    TObjectPtr<UVoxelSurfaceTypeInterface> UnresolvedSurfaceType;

    // Final surface type after smart resolution
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    TObjectPtr<UVoxelSurfaceTypeInterface> SurfaceType;

    // Queried metadata values
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    TMap<TObjectPtr<UVoxelMetadata>, FVoxelPinValue> MetadataToValue;
};
```

### Query Result Fields

| Field | Type | Description |
|-------|------|-------------|
| `Value` | `float` | Height (2D) or distance to surface (3D) |
| `Normal` | `FVector` | Surface normal (from gradient) |
| `UnresolvedSurfaceType` | `UVoxelSurfaceTypeInterface*` | Raw surface type |
| `SurfaceType` | `UVoxelSurfaceTypeInterface*` | Smart surface type resolved |
| `MetadataToValue` | `TMap` | Custom metadata values |

---

## UVoxelQueryBlueprintLibrary

Blueprint function library for terrain queries.

### QueryVoxelLayer (Single Point)

```cpp
UFUNCTION(BlueprintCallable, Category = "Voxel")
static FVoxelFuture K2_QueryVoxelLayer(
    bool& bSuccess,
    FVoxelQueryResult& Result,
    UWorld* World,
    FVoxelStackLayer Layer,
    FVector Position,
    TArray<UVoxelMetadata*> MetadatasToQuery,
    float GradientStep = 100.f);
```

### MultiQueryVoxelLayer (Multiple Points)

```cpp
UFUNCTION(BlueprintCallable, Category = "Voxel")
static FVoxelFuture K2_MultiQueryVoxelLayer(
    bool& bSuccess,
    TArray<FVoxelQueryResult>& Results,
    UWorld* World,
    FVoxelStackLayer Layer,
    TArray<FVector> Positions,
    TArray<UVoxelMetadata*> MetadatasToQuery,
    float GradientStep = 100.f);
```

### ExportVoxelDataToRenderTarget

```cpp
UFUNCTION(BlueprintCallable, Category = "Voxel|Query")
static FVoxelFuture K2_ExportVoxelDataToRenderTarget(
    bool& bSuccess,
    UWorld* World,
    UTextureRenderTarget2D* RenderTarget,
    FVoxelStackHeightLayer Layer,
    FVoxelBox2D ZoneToQuery,
    FVoxelColorQuery Query);
```

---

## Query Types

### EVoxelFloatQueryType

```cpp
UENUM(BlueprintType)
enum class EVoxelFloatQueryType : uint8
{
    // Write a constant value
    Constant,

    // Write the terrain height
    Height,

    // Write a metadata value
    Metadata
};
```

### FVoxelFloatQuery

Configuration for a single float channel query.

```cpp
USTRUCT(BlueprintType)
struct VOXEL_API FVoxelFloatQuery
{
    GENERATED_BODY()

    // Query type
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    EVoxelFloatQueryType Type = EVoxelFloatQueryType::Constant;

    // Constant value (when Type == Constant)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    float Constant = 0.f;

    // Metadata to query (when Type == Metadata)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    TObjectPtr<UVoxelMetadata> Metadata;

    // Component to extract from vector/color metadata
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    EVoxelTextureChannel ComponentToExtract = EVoxelTextureChannel::R;
};
```

### FVoxelColorQuery

Four-channel RGBA query configuration.

```cpp
USTRUCT(BlueprintType)
struct VOXEL_API FVoxelColorQuery
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    FVoxelFloatQuery R;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    FVoxelFloatQuery G;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    FVoxelFloatQuery B;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    FVoxelFloatQuery A;
};
```

---

## Bulk Query System

Grid-based queries for efficient sampling of rectangular regions.

### FVoxelHeightBulkQuery

```cpp
struct VOXEL_API FVoxelHeightBulkQuery : FVoxelStampQuery
{
public:
    // Index bounds in the grid
    const FVoxelIntBox2D Indices;

    // World-space start position
    const FVector2d Start;

    // Step size between samples
    const float Step;

    // Output heights array
    const TVoxelArrayView<float> Heights;

    // X stride for indexing
    const int32 StrideX;

public:
    // Create query from parameters
    static FVoxelHeightBulkQuery Create(
        const FVoxelQuery& Query,
        TVoxelArrayView<float> Heights,
        const FVector2D& Start,
        const FIntPoint& Size,
        float Step);

    // Get number of query points
    int32 Num() const;

    // Get world-space bounds
    FVoxelBox2D GetBounds() const;

    // Get position for grid coordinate
    FVector2d GetPosition(const int32 X, const int32 Y) const;
};
```

### FVoxelVolumeBulkQuery

```cpp
struct VOXEL_API FVoxelVolumeBulkQuery : FVoxelStampQuery
{
public:
    // Index bounds in the 3D grid
    const FVoxelIntBox Indices;

    // World-space start position
    const FVector3d Start;

    // Step size between samples
    const float Step;

    // Output distances array
    const TVoxelArrayView<float> Distances;

    // Stride values for indexing
    const int32 StrideX;
    const int32 StrideXY;

public:
    // Create query from parameters
    static FVoxelVolumeBulkQuery Create(
        const FVoxelQuery& Query,
        TVoxelArrayView<float> Distances,
        const FVector& Start,
        const FIntVector& Size,
        float Step);

    // Get number of query points
    int32 Num() const;

    // Get world-space bounds
    FVoxelBox GetBounds() const;

    // Get position for grid coordinate
    FVector3d GetPosition(const int32 X, const int32 Y, const int32 Z) const;
};
```

---

## Sparse Query System

Point-based queries for arbitrary positions.

### FVoxelHeightSparseQuery

```cpp
struct VOXEL_API FVoxelHeightSparseQuery : FVoxelStampSparseQuery
{
public:
    // Output heights for each position
    const TVoxelArrayView<float> IndirectHeights;

    // Query positions (2D)
    const FVoxelDoubleVector2DBuffer Positions;

    // Bounding box of all positions
    const FVoxelBox2D PositionBounds;

public:
    // Create query from parameters
    static FVoxelHeightSparseQuery Create(
        const FVoxelQuery& Query,
        TVoxelArrayView<float> Heights,
        TVoxelArrayView<FVoxelSurfaceTypeBlend> SurfaceTypes,
        const FVoxelMetadataMap& Metadata,
        const FVoxelDoubleVector2DBuffer& Positions,
        bool bQuerySurfaceTypes,
        const TVoxelArray<FVoxelMetadataRef>& MetadatasToQuery);

    // Get height reference at index
    float& GetHeightRef(const int32 Index) const;
};
```

### FVoxelVolumeSparseQuery

```cpp
struct VOXEL_API FVoxelVolumeSparseQuery : FVoxelStampSparseQuery
{
public:
    // Output distances for each position
    const TVoxelArrayView<float> IndirectDistances;

    // Query positions (3D)
    const FVoxelDoubleVectorBuffer Positions;

    // Bounding box of all positions
    const FVoxelBox PositionBounds;

public:
    // Create query from parameters
    static FVoxelVolumeSparseQuery Create(
        const FVoxelQuery& Query,
        TVoxelArrayView<float> Distances,
        TVoxelArrayView<FVoxelSurfaceTypeBlend> SurfaceTypes,
        const FVoxelMetadataMap& Metadata,
        const FVoxelDoubleVectorBuffer& Positions,
        bool bQuerySurfaceTypes,
        const TVoxelArray<FVoxelMetadataRef>& MetadatasToQuery);

    // Get distance reference at index
    float& GetDistanceRef(const int32 Index) const;
};
```

---

## Blueprint Usage

### Single Point Query

```
Event BeginPlay
    │
    └──► Query Voxel Layer
              World: Get World
              Layer: (Height or Volume layer)
              Position: Get Actor Location
              MetadatasToQuery: []
              GradientStep: 100
              │
              ├──► bSuccess: Branch
              │         │
              │         └──► True: Print Result.Value
              │
              └──► Result
                    ├── Value (height/distance)
                    ├── Normal (surface direction)
                    └── SurfaceType
```

### Multiple Point Query

```
Event Tick
    │
    ├──► Build Positions Array
    │         [Actor1 Location, Actor2 Location, ...]
    │
    └──► Multi Query Voxel Layer
              Positions: (array)
              │
              └──► For Each Result
                        └── Process height/surface data
```

### Export to Render Target

```
[Create Render Target 2D]
    │
    └──► Export Voxel Data To Render Target
              RenderTarget: RT_TerrainHeight
              Layer: HL_MainTerrain
              ZoneToQuery: Make Box 2D (Min, Max)
              Query:
                  R: Query Height
                  G: Make Constant (0)
                  B: Make Constant (0)
                  A: Make Constant (1)
```

---

## C++ Usage Examples

### Simple Height Query

```cpp
void AMyActor::QueryTerrainHeight()
{
    FVector Position = GetActorLocation();

    // Async query
    auto Future = UVoxelQueryBlueprintLibrary::QueryVoxelLayer(
        GetWorld(),
        MyHeightLayer,
        Position,
        TArray<UVoxelMetadata*>(),  // No metadata
        100.f);  // Gradient step

    Future.Then([this](TOptional<FVoxelQueryResult> Result)
    {
        if (Result.IsSet())
        {
            float TerrainHeight = Result->Value;
            FVector Normal = Result->Normal;
            UE_LOG(LogTemp, Log, TEXT("Height: %.2f, Normal: %s"),
                TerrainHeight, *Normal.ToString());
        }
    });
}
```

### Batch Query for Multiple Actors

```cpp
void AMyGameMode::QueryMultiplePositions()
{
    TArray<FVector> Positions;
    for (AActor* Actor : AllActors)
    {
        Positions.Add(Actor->GetActorLocation());
    }

    auto Future = UVoxelQueryBlueprintLibrary::MultiQueryVoxelLayer(
        GetWorld(),
        MyVolumeLayer,
        Positions,
        TArray<UVoxelMetadata*>(),
        100.f);

    Future.Then([this, Positions](TOptional<TArray<FVoxelQueryResult>> Results)
    {
        if (Results.IsSet())
        {
            for (int32 i = 0; i < Results->Num(); i++)
            {
                float Distance = (*Results)[i].Value;
                // Negative = inside terrain, Positive = outside
                UE_LOG(LogTemp, Log, TEXT("Actor %d: Distance to surface = %.2f"),
                    i, Distance);
            }
        }
    });
}
```

### Low-Level Bulk Query

```cpp
void AMyActor::BulkSampleHeightGrid()
{
    // Setup grid parameters
    FVector2D Start(0, 0);
    FIntPoint Size(100, 100);
    float Step = 100.f;  // 1m between samples

    // Allocate output buffer
    TArray<float> Heights;
    Heights.SetNumUninitialized(Size.X * Size.Y);

    // Create query context (requires internal access)
    FVoxelDependencyCollector DependencyCollector;
    FVoxelQuery Query(
        0,  // LOD
        *VoxelWorld->GetLayers(),
        *VoxelWorld->GetSurfaceTypeTable(),
        DependencyCollector);

    // Sample height layer
    FVoxelFloatBuffer HeightBuffer = Query.SampleHeightLayer(
        MyWeakHeightLayer,
        Start,
        Size,
        Step);

    // Process results
    for (int32 Y = 0; Y < Size.Y; Y++)
    {
        for (int32 X = 0; X < Size.X; X++)
        {
            float Height = HeightBuffer[X + Y * Size.X];
            // Use height data...
        }
    }
}
```

### Exporting Terrain to Texture

```cpp
void AMyActor::ExportTerrainToTexture()
{
    // Create render target
    UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>();
    RenderTarget->InitAutoFormat(1024, 1024);
    RenderTarget->RenderTargetFormat = RTF_RGBA16f;

    // Define zone to export
    FVoxelBox2D Zone(
        FVector2D(-50000, -50000),  // Min (500m)
        FVector2D(50000, 50000));   // Max (500m)

    // Setup query - height in R channel
    FVoxelColorQuery Query;
    Query.R = UVoxelQueryBlueprintLibrary::QueryHeight();
    Query.G.Type = EVoxelFloatQueryType::Constant;
    Query.G.Constant = 0.f;
    Query.B.Type = EVoxelFloatQueryType::Constant;
    Query.B.Constant = 0.f;
    Query.A.Type = EVoxelFloatQueryType::Constant;
    Query.A.Constant = 1.f;

    // Execute export
    auto Future = UVoxelQueryBlueprintLibrary::ExportVoxelDataToRenderTarget(
        GetWorld(),
        RenderTarget,
        MyHeightLayer,
        Zone,
        Query);

    Future.Then([RenderTarget](bool bSuccess)
    {
        if (bSuccess)
        {
            // RenderTarget now contains terrain height data
            UE_LOG(LogTemp, Log, TEXT("Export complete!"));
        }
    });
}
```

---

## Query Performance

### Query Type Comparison

| Query Type | Speed | Use Case |
|------------|-------|----------|
| Single Point | Fast | Real-time per-actor |
| Multi Point | Fast | Batched actor queries |
| Bulk (Grid) | Fastest | Regular grids, textures |
| Sparse | Medium | Arbitrary point clouds |

### Performance Tips

1. **Batch queries** - Use MultiQuery instead of multiple single queries
2. **Use appropriate LOD** - Higher LOD = faster but less accurate
3. **Limit metadata** - Only query needed metadata
4. **Cache results** - Avoid redundant queries each frame
5. **Use bulk queries** - For regular grids, bulk is much faster

### Async Considerations

All queries return `FVoxelFuture` and execute asynchronously:

```cpp
// DON'T block the game thread
auto Future = UVoxelQueryBlueprintLibrary::QueryVoxelLayer(...);
// Future executes in background

// DO handle results in callback
Future.Then([](TOptional<FVoxelQueryResult> Result)
{
    // Safe to use Result here
});
```

---

## Debug Visualization

### FVoxelQueryDebugDrawer

```cpp
class VOXEL_API FVoxelQueryDebugDrawer
{
public:
    // Visualize height layer queries
    static void OnHeightLayerQuery(const FVoxelHeightBulkQuery& Query, double Time);
    static void OnHeightLayerQuery(const FVoxelHeightSparseQuery& Query, double Time);

    // Visualize volume layer queries
    static void OnVolumeLayerQuery(const FVoxelVolumeBulkQuery& Query, double Time);
    static void OnVolumeLayerQuery(const FVoxelVolumeSparseQuery& Query, double Time);
};

// Enable debug visualization
extern VOXEL_API bool GVoxelShowAllQueries;
```

### Enabling Debug Display

```cpp
// In console or code
GVoxelShowAllQueries = true;

// Or in Blueprint
Execute Console Command: "voxel.ShowAllQueries 1"
```

---

## Query with Metadata

### Querying Custom Metadata

```cpp
void AMyActor::QueryWithMetadata()
{
    // Get metadata assets
    TArray<UVoxelMetadata*> MetadatasToQuery;
    MetadatasToQuery.Add(MyMoistureMetadata);
    MetadatasToQuery.Add(MyTemperatureMetadata);

    auto Future = UVoxelQueryBlueprintLibrary::QueryVoxelLayer(
        GetWorld(),
        MyHeightLayer,
        GetActorLocation(),
        MetadatasToQuery,
        100.f);

    Future.Then([](TOptional<FVoxelQueryResult> Result)
    {
        if (Result.IsSet())
        {
            // Access metadata values
            for (auto& Pair : Result->MetadataToValue)
            {
                UVoxelMetadata* Metadata = Pair.Key;
                FVoxelPinValue Value = Pair.Value;

                UE_LOG(LogTemp, Log, TEXT("Metadata %s = %s"),
                    *Metadata->GetName(),
                    *Value.ToString());
            }
        }
    });
}
```

### Blueprint Metadata Query

```
Query Voxel Layer
    │
    └── MetadatasToQuery:
            [0] VM_Moisture
            [1] VM_Temperature
    │
    └── Result.MetadataToValue
            │
            └── Find (VM_Moisture) ──► Moisture Value
            └── Find (VM_Temperature) ──► Temperature Value
```

---

## Common Patterns

### Ground Snap Actor

```cpp
void AMyActor::SnapToGround()
{
    FVector Location = GetActorLocation();

    auto Future = UVoxelQueryBlueprintLibrary::QueryVoxelLayer(
        GetWorld(),
        MyHeightLayer,
        Location,
        TArray<UVoxelMetadata*>(),
        100.f);

    Future.Then([this](TOptional<FVoxelQueryResult> Result)
    {
        if (Result.IsSet())
        {
            FVector NewLocation = GetActorLocation();
            NewLocation.Z = Result->Value;  // Set Z to terrain height
            SetActorLocation(NewLocation);
        }
    });
}
```

### Surface Type Detection

```cpp
void AMyActor::DetectSurfaceType()
{
    auto Future = UVoxelQueryBlueprintLibrary::QueryVoxelLayer(
        GetWorld(),
        MyHeightLayer,
        GetActorLocation(),
        TArray<UVoxelMetadata*>(),
        100.f);

    Future.Then([this](TOptional<FVoxelQueryResult> Result)
    {
        if (Result.IsSet() && Result->SurfaceType)
        {
            FName SurfaceTypeName = Result->SurfaceType->GetFName();

            if (SurfaceTypeName == "Water")
            {
                // Play splash effect
            }
            else if (SurfaceTypeName == "Grass")
            {
                // Play grass footstep
            }
        }
    });
}
```

---

## Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| Query returns nothing | Voxel world not ready | Wait for IsVoxelWorldReady() |
| Wrong height values | Querying volume layer | Use height layer for 2D terrain |
| Metadata missing | Not in MetadatasToQuery | Add metadata to query array |
| Performance issues | Too many single queries | Use MultiQuery or bulk queries |
| Stale results | Cached old data | Re-query when terrain changes |
| Normal is zero | GradientStep too small | Increase GradientStep (default 100) |

---

## Related Topics

- [04-voxelworld-layerstack.md](04-voxelworld-layerstack.md) - Layer configuration
- [05-height-layer-system.md](05-height-layer-system.md) - Height layers
- [06-volume-layer-system.md](06-volume-layer-system.md) - Volume layers
- [12-pcg-integration.md](12-pcg-integration.md) - PCG queries
- [13-collision-rendering.md](13-collision-rendering.md) - Physics queries
