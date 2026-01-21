# PCG Integration

## Summary

VoxelPlugin2 integrates with Unreal Engine's Procedural Content Generation (PCG) framework through the VoxelPCG module. This enables bidirectional workflows: sampling points from voxel terrain for PCG graphs, and using PCG to spawn voxel stamps. The integration provides nodes for terrain sampling, projection, querying, and spline creation.

---

## Key Classes

| Class | Header | Purpose |
|-------|--------|---------|
| `UVoxelPCGSettings` | VoxelPCGHelpers.h | Base class for Voxel PCG nodes |
| `UPCGVoxelSamplerSettings` | PCGVoxelSampler.h | Generate points on voxel terrain |
| `UPCGVoxelQuerySettings` | PCGVoxelQuery.h | Query terrain data at points |
| `UPCGVoxelProjectionSettings` | PCGVoxelProjection.h | Project points onto terrain |
| `UPCGCallVoxelGraphSettings` | PCGCallVoxelGraph.h | Execute VoxelGraph on PCG points |
| `UPCGVoxelStampSpawnerSettings` | PCGVoxelStampSpawner.h | Spawn voxel stamps from points |
| `UPCGWaitForVoxelWorldSettings` | PCGWaitForVoxelWorld.h | Synchronization node |
| `UPCGCreateVoxelSplineSettings` | PCGCreateVoxelSpline.h | Create voxel splines from points |
| `UVoxelPCGGraph` | VoxelPCGGraph.h | VoxelGraph for PCG workflows |
| `FVoxelNode_Generate2DPoints` | VoxelNode_GenerateSurfacePoints2D.h | Generate points on heightmap |
| `FVoxelNode_ScatterPoints` | VoxelNode_ScatterPoints.h | Scatter child points |
| `FVoxelOutputNode_OutputPoints` | VoxelOutputNode_OutputPoints.h | Output points from VoxelGraph |

---

## Module Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         PCG Graph                                │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────────┐  │
│  │ PCG Sampler │ ──►│ PCG Filter  │ ──►│ PCG Spawn Static    │  │
│  │ (Landscape) │    │             │    │ Mesh                 │  │
│  └─────────────┘    └─────────────┘    └─────────────────────┘  │
│         │                                                        │
│         ▼                                                        │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │                  VoxelPCG Integration                        │ │
│  │  ┌─────────────┐  ┌────────────┐  ┌───────────────────────┐ │ │
│  │  │ Voxel       │  │ Voxel      │  │ Voxel Stamp           │ │ │
│  │  │ Sampler     │  │ Projection │  │ Spawner               │ │ │
│  │  └─────────────┘  └────────────┘  └───────────────────────┘ │ │
│  └─────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                        AVoxelWorld                               │
│                    (Terrain Generation)                          │
└─────────────────────────────────────────────────────────────────┘
```

---

## UVoxelPCGSettings Base Class

Base class for all Voxel-PCG integration nodes.

```cpp
UCLASS(ClassGroup = (Voxel))
class VOXELPCG_API UVoxelPCGSettings : public UPCGSettings
{
    GENERATED_BODY()

public:
    // Track voxel layer changes for automatic regeneration
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", AdvancedDisplay, meta = (PCG_Overridable))
    bool bTrackLayerChanges = true;

public:
    // Override in derived classes to create async output
    virtual TSharedPtr<FVoxelPCGOutput> CreateOutput(FPCGContext& Context) const;
};
```

---

## Voxel Sampler Node

Generates PCG points distributed across voxel terrain surface.

```cpp
UCLASS(DisplayName = "Voxel Sampler")
class VOXELPCG_API UPCGVoxelSamplerSettings : public UVoxelPCGSettings
{
public:
    // Generate points even without bounding shape
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
    bool bUnbounded = false;

    // Target layer to sample
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
    FVoxelStackLayer Layer;

    // Point density (points per m²)
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
    float PointsPerSquaredMeter = 0.1f;

    // Cell size for spatial distribution
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
    float CellSize = 100.f;

    // Relaxation factor (0 = grid, 1 = jittered)
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (ClampMin = "0", PCG_Overridable))
    float Looseness = 1.0f;

    // Distance tolerance for surface snapping
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (ClampMin = "0", PCG_Overridable))
    float Tolerance = 0.0f;

    // Apply terrain density to point density
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Points", meta = (PCG_Overridable))
    bool bApplyDensityToPoints = true;

    // Metadata to query at each point
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
    TArray<TObjectPtr<UVoxelMetadata>> NewMetadatasToQuery;

    // LOD level for sampling
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", AdvancedDisplay, meta = (PCG_Overridable, ClampMin = 0))
    int32 LOD = 0;

    // Resolve smart surface types
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", AdvancedDisplay, meta = (PCG_Overridable))
    bool bResolveSmartSurfaceTypes = true;
};
```

### Sampler Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `bUnbounded` | `bool` | false | Generate over entire surface |
| `Layer` | `FVoxelStackLayer` | - | Target terrain layer |
| `PointsPerSquaredMeter` | `float` | 0.1 | Point density |
| `CellSize` | `float` | 100 | Cell size for distribution |
| `Looseness` | `float` | 1.0 | Jitter amount (0-1) |
| `LOD` | `int32` | 0 | Terrain LOD to sample |

---

## Voxel Query Node

Queries terrain data at existing point locations.

```cpp
UCLASS(DisplayName = "Voxel Query")
class VOXELPCG_API UPCGVoxelQuerySettings : public UVoxelPCGSettings
{
public:
    // Target layer to query
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
    FVoxelStackLayer Layer;

    // Attribute name for height/distance output
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
    FString HeightOrDistanceAttribute = "QueryValue";

    // Query surface types at each point
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
    bool bQuerySurfaceTypes = false;

    // Suffix for surface attributes
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable, EditCondition = "bQuerySurfaceTypes"))
    FString SurfaceAttributeSuffix = "_Query";

    // Metadata to query
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
    TArray<TObjectPtr<UVoxelMetadata>> NewMetadatasToQuery;

    // LOD level for queries
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", AdvancedDisplay, meta = (PCG_Overridable, ClampMin = 0))
    int32 LOD = 0;
};
```

### Output Attributes

The query node adds attributes to points:

| Attribute | Type | Description |
|-----------|------|-------------|
| `QueryValue` (configurable) | `float` | Height or distance to surface |
| `SurfaceType_Query` | `string` | Surface type name (if enabled) |
| Custom metadata | varies | Per-point metadata values |

---

## Voxel Projection Node

Projects points onto voxel terrain surface using raymarching.

```cpp
UCLASS(DisplayName = "Voxel Projection")
class VOXELPCG_API UPCGVoxelProjectionSettings : public UVoxelPCGSettings
{
public:
    // Target layer to project onto
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
    FVoxelStackLayer Layer;

    // Kill points further than this from surface
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable, ClampMin = 0, Units = cm))
    float KillDistance = 1000;

    // Update point rotation to match surface normal
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
    bool bUpdateRotation = false;

    // Project along point's down direction
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
    bool bForceDirection = true;

    // LOD for projection
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", AdvancedDisplay, meta = (PCG_Overridable, ClampMin = 0))
    int32 LOD = 0;

    // Maximum raymarch steps (volume layers only)
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", AdvancedDisplay, meta = (PCG_Overridable, ClampMin = 1))
    int32 MaxSteps = 10;

    // Surface tolerance
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", AdvancedDisplay, meta = (PCG_Overridable, Units = cm))
    float Tolerance = 10;

    // Raymarch convergence speed
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", AdvancedDisplay, meta = (PCG_Overridable, ClampMin = 0, ClampMax = 1))
    float Speed = 0.8f;

    // Gradient sampling distance
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", AdvancedDisplay, meta = (PCG_Overridable, ClampMin = 0, ClampMax = 1, Units = cm))
    float GradientStep = 100.f;
};
```

### Projection Modes

| Layer Type | Projection Method | Notes |
|------------|-------------------|-------|
| **Height** | Direct Z-projection | Fast, ignores MaxSteps |
| **Volume** | SDF Raymarching | Uses MaxSteps, Tolerance, Speed |

---

## Call Voxel Graph Node

Executes a VoxelPCGGraph on PCG points for custom processing.

```cpp
UCLASS(BlueprintType, ClassGroup = (Voxel))
class VOXELPCG_API UPCGCallVoxelGraphSettings : public UPCGSettings
{
public:
    // The VoxelPCGGraph to execute
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (PCG_Overridable))
    TObjectPtr<UVoxelPCGGraph> Graph;

    // Map object attributes to voxel types
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", DisplayName = "Object Attributes Mapping")
    TArray<FVoxelPCGObjectAttributeType> ObjectAttributeToType;

    // Force synchronous asset loading
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Debug")
    bool bSynchronousLoad = false;

    // Graph parameter overrides
    UPROPERTY()
    FVoxelParameterOverrides ParameterOverrides;
};
```

---

## Voxel Stamp Spawner Node

Spawns voxel stamps from PCG points.

```cpp
UCLASS()
class VOXELPCG_API UPCGVoxelStampSpawnerSettings : public UPCGSettings
{
public:
    // Stamp template to spawn
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Config, DisplayName = Template)
    FVoxelStampRef NewTemplate = FVoxelStampRef::New(FVoxelHeightmapStamp());

    // Target actor for spawned stamps
    UPROPERTY(BlueprintReadWrite, Category = Settings, meta = (PCG_Overridable))
    TSoftObjectPtr<AActor> TargetActor;

    // Property overrides for spawned stamps
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
    TArray<FPCGObjectPropertyOverrideDescription> SpawnedStampPropertyOverrideDescriptions;

    // Graph parameter overrides
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
    TArray<FPCGObjectPropertyOverrideDescription> SpawnedGraphParameterOverrideDescriptions;

    // Post-spawn function calls
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
    TArray<FName> PostProcessFunctionNames;
};
```

---

## Wait For Voxel World Node

Synchronization node that pauses PCG execution until voxel world is ready.

```cpp
UCLASS()
class VOXELPCG_API UPCGWaitForVoxelWorldSettings : public UPCGSettings
{
    // Control flow node - waits for AVoxelWorld::IsVoxelWorldReady()
    // No additional properties - just delays execution
};
```

### Usage

Place this node before any Voxel nodes to ensure terrain is generated:

```
[Get Actor Data] ──► [Wait For Voxel World] ──► [Voxel Sampler] ──► [Spawn Foliage]
```

---

## Create Voxel Spline Node

Creates voxel spline stamps from PCG spline data.

```cpp
UCLASS(BlueprintType, ClassGroup = (Voxel))
class VOXELPCG_API UPCGCreateVoxelSplineSettings : public UPCGSettings
{
public:
    // Create volume spline instead of height spline
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
    bool bVolumeSpline = false;

    // Height spline graph (when bVolumeSpline = false)
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "!bVolumeSpline"))
    TObjectPtr<UVoxelHeightSplineGraph> HeightGraph;

    // Height blend mode
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PCG_Overridable, EditCondition = "!bVolumeSpline"))
    EVoxelHeightBlendMode HeightBlendMode = EVoxelHeightBlendMode::Max;

    // Volume spline graph (when bVolumeSpline = true)
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "bVolumeSpline"))
    TObjectPtr<UVoxelVolumeSplineGraph> VolumeGraph;

    // Volume blend mode
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PCG_Overridable, EditCondition = "bVolumeSpline"))
    EVoxelVolumeBlendMode VolumeBlendMode = EVoxelVolumeBlendMode::Additive;

    // Edge smoothness
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (Units = cm, ClampMin = 0))
    float Smoothness = 100;

    // Stamp priority
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
    int32 Priority = 0;

    // Target layer
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
    TObjectPtr<UVoxelLayer> Layer;

    // How stamp affects other stamps
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
    EVoxelStampBehavior StampBehavior = EVoxelStampBehavior::AffectAll;
};
```

---

## VoxelPCGGraph Asset

Special VoxelGraph type designed for PCG integration workflows.

```cpp
UCLASS(BlueprintType, meta = (DisplayName = "Voxel PCG Graph", AssetSubMenu = "Graph"))
class VOXELPCG_API UVoxelPCGGraph : public UVoxelGraph
{
    GENERATED_BODY()

public:
    virtual UScriptStruct* GetOutputNodeStruct() const override;
    // Returns FVoxelOutputNode_OutputPoints
};
```

### Creating VoxelPCGGraph

1. Content Browser > Right-click
2. Select **Voxel > PCG Graph**
3. Build point processing nodes
4. Connect to **Output Points** node

---

## VoxelGraph Point Nodes

### FVoxelNode_Generate2DPoints

Generates points distributed on a heightmap.

```cpp
USTRUCT(Category = "Point", DisplayName = "Generate 2D Points")
struct VOXELPCG_API FVoxelNode_Generate2DPoints : public FVoxelNode
{
    VOXEL_INPUT_PIN(FVoxelBox, Bounds, nullptr);
    VOXEL_INPUT_PIN(FVoxelFloatBuffer, Height, nullptr);
    VOXEL_INPUT_PIN(float, CellSize, 100.f);      // Average point spacing
    VOXEL_INPUT_PIN(float, Jitter, 0.75f);        // Randomization (0-1)
    VOXEL_INPUT_PIN(FVoxelSeed, Seed, nullptr);
    VOXEL_OUTPUT_PIN(FVoxelPointSet, Out);
};
```

### FVoxelNode_ScatterPoints

Scatters child points around parent points.

```cpp
USTRUCT(Category = "Point")
struct VOXELPCG_API FVoxelNode_ScatterPoints : public FVoxelNode
{
    VOXEL_INPUT_PIN(FVoxelPointSet, In, nullptr);
    VOXEL_INPUT_PIN(FVoxelFloatRange, Radius, FVoxelFloatRange(0, 200));
    VOXEL_INPUT_PIN(float, RadialOffset, 10.f);   // Degrees
    VOXEL_INPUT_PIN(FVoxelInt32Range, NumPoints, FVoxelInt32Range(5, 10));
    VOXEL_INPUT_PIN(FVoxelSeed, Seed, nullptr);
    VOXEL_OUTPUT_PIN(FVoxelPointSet, Out);
};
```

### FVoxelOutputNode_OutputPoints

Terminal node for outputting points from VoxelPCGGraph.

```cpp
USTRUCT()
struct VOXELPCG_API FVoxelOutputNode_OutputPoints : public FVoxelOutputNode
{
    VOXEL_INPUT_PIN(FVoxelPointSet, Points, nullptr);

    // Supports multiple named output pins
    TVoxelArray<TPinRef_Input<FVoxelPointSet>> InputPins;
    TArray<FName> PinNames;
};
```

---

## Setup Workflow

### Basic PCG-Voxel Integration

1. **Create PCG Graph**
   - Content Browser > PCG > PCG Graph

2. **Add Voxel Nodes**
   ```
   [Get Actor Data]
       │
       └──► [Wait For Voxel World]
               │
               └──► [Voxel Sampler]
                       Layer: MainTerrainLayer
                       PointsPerSquaredMeter: 0.05
                       │
                       └──► [Filter by Surface Type]
                               │
                               └──► [Spawn Static Mesh]
   ```

3. **Place PCG Volume**
   - Add PCG Volume actor to level
   - Assign PCG Graph
   - Configure generation triggers

### Spawning Stamps from PCG

```
[Surface Sampler]
    │
    └──► [Filter by Density]
            │
            └──► [Voxel Stamp Spawner]
                    Template: HeightGraphStamp (rock)
                    TargetActor: VoxelWorld
```

### Creating Procedural Roads

```
[Spline Sampler]
    │
    └──► [Create Voxel Spline]
            HeightGraph: HG_Road
            HeightBlendMode: Override
            Smoothness: 500
            Layer: RoadsLayer
```

---

## C++ Usage Examples

### Custom PCG Node Querying Voxel Data

```cpp
#include "VoxelPCGHelpers.h"
#include "PCGVoxelQuery.h"

void AMyActor::QueryVoxelFromPCG(UPCGComponent* PCGComp)
{
    // Get PCG context
    FPCGContext& Context = PCGComp->GetCurrentContext();

    // Create query settings
    UPCGVoxelQuerySettings* QuerySettings = NewObject<UPCGVoxelQuerySettings>();
    QuerySettings->Layer = MyVoxelLayer;
    QuerySettings->HeightOrDistanceAttribute = "TerrainHeight";
    QuerySettings->bQuerySurfaceTypes = true;

    // Execute through PCG system
    TSharedPtr<FVoxelPCGOutput> Output = QuerySettings->CreateOutput(Context);
    Output->Run();
}
```

### Using VoxelPCGGraph for Point Generation

```cpp
void AMyActor::GeneratePointsWithVoxelGraph()
{
    UVoxelPCGGraph* Graph = LoadObject<UVoxelPCGGraph>(
        nullptr, TEXT("/Game/PCG/VPG_TreePlacement"));

    // Create evaluator
    TVoxelNodeEvaluator<FVoxelOutputNode_OutputPoints> Evaluator;
    Evaluator.Initialize(Graph);

    // Set bounds
    FVoxelBox Bounds(FVector(-10000, -10000, 0), FVector(10000, 10000, 5000));

    // Execute and get points
    FVoxelFuture Future = Evaluator.Compute(Bounds);
    Future.Then([](TSharedRef<FVoxelPointSet> Points)
    {
        UE_LOG(LogTemp, Log, TEXT("Generated %d points"), Points->Num());
    });
}
```

---

## Blueprint Usage

### Setting Up Voxel Sampler in PCG

1. **Add Voxel Sampler Node**
   - Right-click in PCG Graph
   - Select **Voxel > Voxel Sampler**

2. **Configure Properties**
   ```
   Layer: HL_MainTerrain
   PointsPerSquaredMeter: 0.1
   CellSize: 100
   Looseness: 1.0
   bApplyDensityToPoints: true
   ```

3. **Connect to Spawn Nodes**
   ```
   [Voxel Sampler] ──► [Filter] ──► [Spawn Static Mesh]
   ```

### Projecting Points onto Terrain

```
[Grid Sampler]
    │
    └──► [Voxel Projection]
            Layer: VL_MainTerrain
            bUpdateRotation: true
            KillDistance: 500
            │
            └──► [Spawn Actor]
```

---

## Attribute Mapping

### Sampler Output Attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `$Position` | `FVector` | World position on terrain |
| `$Rotation` | `FQuat` | Surface normal orientation |
| `$Scale` | `FVector` | Default (1,1,1) |
| `Density` | `float` | Point density (0-1) |
| `SurfaceType` | `FName` | Surface type name |
| Metadata values | varies | Custom per-point data |

### Query Attribute Mapping

```cpp
// Configure query to add terrain height as attribute
UPCGVoxelQuerySettings* Settings = ...;
Settings->HeightOrDistanceAttribute = "TerrainZ";  // Creates "TerrainZ" attribute
Settings->bQuerySurfaceTypes = true;               // Creates surface attributes
Settings->SurfaceAttributeSuffix = "_Terrain";     // e.g., "SurfaceType_Terrain"
```

---

## Performance Considerations

### Sampling Optimization

| Factor | Impact | Recommendation |
|--------|--------|----------------|
| PointsPerSquaredMeter | High | Start low (0.01-0.1) |
| CellSize | Medium | Match VoxelSize (100) |
| LOD | High | Use higher LOD for distant areas |
| bUnbounded | Critical | Always use bounds! |

### Tracking and Regeneration

```cpp
// Disable auto-regeneration for performance
UPROPERTY()
bool bTrackLayerChanges = false;  // Manual regeneration only
```

### Async Execution

All Voxel PCG nodes execute asynchronously:

```cpp
class FVoxelPCGOutput : public TSharedFromThis<FVoxelPCGOutput>
{
public:
    virtual FVoxelFuture Run() const = 0;  // Async execution
};
```

---

## Common Patterns

### Surface-Filtered Foliage

```
[Voxel Sampler]
    │
    └──► [Voxel Query] (SurfaceType)
            │
            └──► [Filter by Attribute]
                    Attribute: SurfaceType
                    Value: "Grass"
                    │
                    └──► [Spawn Foliage]
```

### Terrain-Aware Placement

```
[Get Spline Data]
    │
    └──► [Create Voxel Spline] (Road)
            │
    [Voxel Sampler] (alongside road)
            │
            └──► [Voxel Projection]
                    │
                    └──► [Spawn Props]
```

### LOD-Based Density

```
[Voxel Sampler LOD 0] ──► [Filter High Density] ──► [Spawn Near Meshes]
         │
         └──► [Voxel Sampler LOD 2] ──► [Spawn Far Imposters]
```

---

## Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| No points generated | Voxel world not ready | Add Wait For Voxel World node |
| Points floating | Wrong layer type | Use Height layer for 2D, Volume for 3D |
| Slow generation | Too many points | Reduce PointsPerSquaredMeter |
| Missing attributes | Query not configured | Enable bQuerySurfaceTypes |
| Stamps not visible | Wrong target actor | Set TargetActor to VoxelWorld actor |
| Regeneration loops | bTrackLayerChanges | Disable or use proper dependencies |

---

## Related Topics

- [04-voxelworld-layerstack.md](04-voxelworld-layerstack.md) - Layer configuration
- [07-stamp-system.md](07-stamp-system.md) - Stamp architecture
- [10-spline-system.md](10-spline-system.md) - Spline stamps
- [14-query-system.md](14-query-system.md) - Terrain queries
- [15-scatter-foliage.md](15-scatter-foliage.md) - Foliage placement
