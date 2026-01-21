# Scatter & Foliage System

## Summary

The scatter system provides procedural instance placement using `AVoxelScatterActor` and `UVoxelScatterGraph`. Points are generated and processed through VoxelGraph nodes, then rendered using instanced static mesh components. The system uses chunked streaming for efficient rendering of distant foliage.

---

## Key Classes

| Class | Header | Purpose |
|-------|--------|---------|
| `AVoxelScatterActor` | VoxelScatterActor.h | Main actor for scatter placement |
| `UVoxelScatterGraph` | VoxelScatterGraph.h | Visual graph asset for scatter logic |
| `FVoxelNode_ScatterBase` | VoxelNode_ScatterBase.h | Base node for scatter output |
| `FVoxelNode_ScatterMesh` | VoxelScatterMeshNode.h | Mesh scatter output node |
| `FVoxelPointSet` | VoxelPointSet.h | Point data container |
| `FVoxelScatterManager` | VoxelScatterManager.h | World subsystem for scatter nodes |
| `FVoxelScatterSubsystem` | VoxelScatterSubsystem.h | Chunk-level scatter processing |
| `UVoxelScatterFunctionLibrary` | VoxelScatterFunctionLibrary.h | Scatter utility functions |
| `FVoxelNode_Generate2DPoints` | VoxelNode_GenerateSurfacePoints2D.h | Point generation on heightmap |
| `FVoxelNode_ScatterPoints` | VoxelNode_ScatterPoints.h | Scatter points around parents |

---

## Architecture Overview

```
[AVoxelScatterActor]
    │
    ├── UVoxelScatterGraph (defines scatter logic)
    │       │
    │       └── Scatter Nodes: Generate → Process → ScatterMesh
    │
    └── FVoxelScatterActorRuntime
            │
            └── Registers with FVoxelScatterManager
                    │
                    └── Processed by FVoxelScatterSubsystem
                            │
                            └── Chunks → ISM Components
```

---

## AVoxelScatterActor

Main actor for procedural instance placement.

```cpp
UCLASS()
class VOXEL_API AVoxelScatterActor
    : public AActor
    , public IVoxelParameterOverridesObjectOwner
{
    GENERATED_BODY()

public:
    // The scatter graph to execute
    UPROPERTY(EditAnywhere, Category = "Config")
    TObjectPtr<UVoxelScatterGraph> Graph;

    // Parameter overrides for the graph
    UPROPERTY()
    FVoxelParameterOverrides ParameterOverrides;

public:
    void CreateRuntime();
    void DestroyRuntime();
    void UpdateRuntime();
};
```

### Setup in Editor

1. Place `AVoxelScatterActor` in level
2. Assign `UVoxelScatterGraph` to Graph property
3. Configure parameter overrides if needed
4. Runtime creates scatter instances automatically

---

## UVoxelScatterGraph

Visual graph asset type for scatter generation logic.

```cpp
UCLASS(BlueprintType, meta = (AssetSubMenu = "Graph"))
class VOXEL_API UVoxelScatterGraph : public UVoxelGraph
{
    GENERATED_BODY()
};
```

### Creating Scatter Graph

1. Content Browser > Right-click
2. **Voxel > Scatter Graph**
3. Double-click to open graph editor
4. Build point generation pipeline

---

## FVoxelPointSet

Container for point data with named attributes.

```cpp
struct VOXEL_API FVoxelPointSet
    : public FVoxelVirtualStruct
    , public TSharedFromThis<FVoxelPointSet>
{
    // Number of points
    int32 Num() const;

    // Access named attributes
    const TVoxelMap<FName, TSharedPtr<const FVoxelBuffer>>& GetAttributes() const;

    // Find attribute by name
    const FVoxelBuffer* Find(const FName Name) const;
    TSharedPtr<const FVoxelBuffer> FindShared(const FName Name) const;

    template<typename T>
    const T* Find(const FName Name) const;

    // Modification
    void SetNum(int32 NewNum);
    void Add(FName Name, const TSharedRef<const FVoxelBuffer>& Buffer);

    // Utility
    TSharedRef<FVoxelPointSet> Gather(TConstVoxelArrayView<int32> Indices) const;
    static TSharedRef<const FVoxelPointSet> Merge(TVoxelArray<TSharedRef<const FVoxelPointSet>> PointSets);
};
```

### Standard Point Attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `Id` | `FVoxelPointIdBuffer` | Unique point identifier |
| `Mesh` | `FVoxelObjectBuffer` | Static mesh reference |
| `Position` | `FVoxelVectorBuffer` | World position |
| `Rotation` | `FVoxelQuaternionBuffer` | Orientation |
| `Scale` | `FVoxelVectorBuffer` | Instance scale |
| `Density` | `FVoxelFloatBuffer` | Placement density |
| `BoundsMin` | `FVoxelVectorBuffer` | AABB minimum |
| `BoundsMax` | `FVoxelVectorBuffer` | AABB maximum |
| `Color` | `FVoxelLinearColorBuffer` | Per-instance color |
| `Steepness` | `FVoxelFloatBuffer` | Surface slope |
| `SurfaceTypes` | - | Surface type data |

```cpp
struct VOXEL_API FVoxelPointAttributes
{
    static const FName Id;
    static const FName Mesh;
    static const FName Position;
    static const FName Rotation;
    static const FName Scale;
    static const FName Density;
    static const FName BoundsMin;
    static const FName BoundsMax;
    static const FName Color;
    static const FName Steepness;
    static const FName SurfaceTypes;

    // For hierarchical parent data
    static FName MakeParent(const FName Name);
};
```

---

## Scatter Nodes

### FVoxelNode_ScatterBase

Abstract base for all scatter output nodes.

```cpp
USTRUCT(Category = "Scatter", meta = (Abstract, AllowList = "Scatter", NodeColor = "Red"))
struct VOXEL_API FVoxelNode_ScatterBase : public FVoxelNode
{
    GENERATED_BODY()
    GENERATED_VOXEL_NODE_BODY()

    // Input points to scatter
    VOXEL_INPUT_PIN(FVoxelPointSet, In, nullptr);

    // Debug name for this node
    VOXEL_INPUT_PIN(FName, Name, nullptr);

    // Chunk size, in meters
    VOXEL_INPUT_PIN(int32, ChunkSize, 64);

    // Passthrough output
    VOXEL_OUTPUT_PIN(FVoxelPointSet, Out);

    virtual TSharedRef<FVoxelScatterNodeRuntime> MakeRuntime() const;
};
```

### FVoxelNode_ScatterMesh

Outputs point sets as instanced static meshes.

```cpp
USTRUCT()
struct VOXEL_API FVoxelNode_ScatterMesh : public FVoxelNode_ScatterBase
{
    GENERATED_BODY()
    GENERATED_VOXEL_NODE_BODY()

    // Render distance in meters
    VOXEL_INPUT_PIN(float, RenderDistance, 64);

    virtual TSharedRef<FVoxelScatterNodeRuntime> MakeRuntime() const override;
};
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `In` | `FVoxelPointSet` | - | Input points with Mesh attribute |
| `Name` | `FName` | - | Debug identifier |
| `ChunkSize` | `int32` | 64 | Chunk size in meters |
| `RenderDistance` | `float` | 64 | Instance visibility distance |

---

## Point Generation Nodes

### FVoxelNode_Generate2DPoints

Generates points on a heightmap surface.

```cpp
USTRUCT(Category = "Point", DisplayName = "Generate 2D Points")
struct VOXELPCG_API FVoxelNode_Generate2DPoints : public FVoxelNode
{
    GENERATED_BODY()
    GENERATED_VOXEL_NODE_BODY()

    // Area to generate points in
    VOXEL_INPUT_PIN(FVoxelBox, Bounds, nullptr);

    // Height values for positioning
    VOXEL_INPUT_PIN(FVoxelFloatBuffer, Height, nullptr);

    // Average distance between points
    VOXEL_INPUT_PIN(float, CellSize, 100.f);

    // Random offset within cell (0-1)
    VOXEL_INPUT_PIN(float, Jitter, 0.75f);

    // Random seed
    VOXEL_INPUT_PIN(FVoxelSeed, Seed, nullptr, AdvancedDisplay);

    // Output point set
    VOXEL_OUTPUT_PIN(FVoxelPointSet, Out);
};
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `Bounds` | `FVoxelBox` | - | Generation area |
| `Height` | `FVoxelFloatBuffer` | - | Terrain height data |
| `CellSize` | `float` | 100 | Grid cell size (meters) |
| `Jitter` | `float` | 0.75 | Randomization amount |
| `Seed` | `FVoxelSeed` | - | Deterministic seed |

### FVoxelNode_ScatterPoints

Scatters child points around parent points.

```cpp
USTRUCT(Category = "Point")
struct VOXELPCG_API FVoxelNode_ScatterPoints : public FVoxelNode
{
    GENERATED_BODY()
    GENERATED_VOXEL_NODE_BODY()

    // Parent points
    VOXEL_INPUT_PIN(FVoxelPointSet, In, nullptr);

    // Distance range from parent
    VOXEL_INPUT_PIN(FVoxelFloatRange, Radius, FVoxelFloatRange(0, 200));

    // Angular offset in degrees
    VOXEL_INPUT_PIN(float, RadialOffset, 10.f);

    // Number of child points per parent
    VOXEL_INPUT_PIN(FVoxelInt32Range, NumPoints, FVoxelInt32Range(5, 10));

    // Random seed
    VOXEL_INPUT_PIN(FVoxelSeed, Seed, nullptr, AdvancedDisplay);

    // Output child points only
    VOXEL_OUTPUT_PIN(FVoxelPointSet, Out);
};
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `In` | `FVoxelPointSet` | - | Parent point set |
| `Radius` | `FVoxelFloatRange` | 0-200 | Scatter distance range |
| `RadialOffset` | `float` | 10 | Angular variation (degrees) |
| `NumPoints` | `FVoxelInt32Range` | 5-10 | Children per parent |
| `Seed` | `FVoxelSeed` | - | Deterministic seed |

---

## Scatter Function Library

Utility functions available in scatter graphs.

```cpp
UCLASS(Category = "Scatter")
class VOXEL_API UVoxelScatterFunctionLibrary : public UVoxelFunctionLibrary
{
    GENERATED_BODY()

public:
    // Get current scatter chunk bounds
    UFUNCTION(meta = (AllowList = "Scatter", ShowInShortList))
    FVoxelBox GetScatterBounds() const;

    /**
     * Generate points on a volume layer
     * @param Layer                 The layer to generate on
     * @param DistanceBetweenPoints Average spacing
     * @param Looseness             Placement variation
     * @param Seed                  Random seed
     * @param bQuerySurfaceTypes    Include surface type data
     * @param bResolveSurfaceTypes  Resolve smart surface types
     * @param MetadatasToQuery      Additional metadata
     * @param InBounds              Override scatter bounds
     */
    UFUNCTION(meta = (AllowList = "Scatter", ShowInShortList))
    FVoxelPointSet GeneratePoints3D(
        const FVoxelWeakStackVolumeLayer& Layer = {},
        float DistanceBetweenPoints = 100.f,
        float Looseness = 1.f,
        const FVoxelSeed& Seed = {},
        bool bQuerySurfaceTypes = true,
        bool bResolveSurfaceTypes = false,
        const FVoxelMetadataRefBuffer& MetadatasToQuery = {},
        const FVoxelBox& InBounds = {});
};
```

### GeneratePoints3D Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `Layer` | `FVoxelWeakStackVolumeLayer` | - | Volume layer for generation |
| `DistanceBetweenPoints` | `float` | 100 | Average point spacing |
| `Looseness` | `float` | 1.0 | Placement variation (0=strict grid) |
| `Seed` | `FVoxelSeed` | - | Random seed |
| `bQuerySurfaceTypes` | `bool` | true | Include surface data |
| `bResolveSurfaceTypes` | `bool` | false | Resolve smart types |
| `MetadatasToQuery` | `FVoxelMetadataRefBuffer` | - | Extra metadata |
| `InBounds` | `FVoxelBox` | - | Custom bounds (optional) |

---

## Runtime System

### FVoxelScatterManager

World-level subsystem managing all scatter nodes.

```cpp
class VOXEL_API FVoxelScatterManager : public IVoxelWorldSubsystem
{
public:
    GENERATED_VOXEL_WORLD_SUBSYSTEM_BODY(FVoxelScatterManager);

    // Register scatter node
    void AddNode(
        const FVoxelScatterNodeWeakRef& NodeRef,
        const TVoxelNodeEvaluator<FVoxelNode_ScatterBase>& Evaluator);

    // Unregister scatter node
    void RemoveNode(const FVoxelScatterNodeWeakRef& NodeRef);

    // Get all registered nodes
    const TVoxelMap<FVoxelScatterNodeWeakRef, TVoxelNodeEvaluator<FVoxelNode_ScatterBase>>&
        GetNodeRefToEvaluator(FVoxelDependencyCollector& DependencyCollector) const;
};
```

### FVoxelScatterSubsystem

Chunk-level subsystem handling scatter computation and rendering.

```cpp
USTRUCT()
struct VOXEL_API FVoxelScatterSubsystem : public FVoxelSubsystem
{
    GENERATED_BODY()
    GENERATED_VOXEL_SUBSYSTEM_BODY()

    // Lifecycle
    virtual void LoadFromPrevious(FVoxelSubsystem& InPreviousSubsystem) override;
    virtual void Initialize() override;
    virtual void Compute() override;
    virtual void Render(FVoxelRuntime& Runtime) override;
};
```

### FVoxelScatterNodeRuntime

Runtime state for individual scatter nodes.

```cpp
class VOXEL_API FVoxelScatterNodeRuntime : public TSharedFromThis<FVoxelScatterNodeRuntime>
{
public:
    // Node info
    FName GetName() const;
    int32 GetChunkSize() const;
    bool IsInvalidated() const;
    const TVoxelNodeEvaluator<FVoxelNode_ScatterBase>& GetEvaluator() const;

    // Lifecycle
    void Initialize(
        const FVoxelSubsystem& Subsystem,
        const FVoxelScatterNodeWeakRef& NodeRef,
        const TVoxelNodeEvaluator<FVoxelNode_ScatterBase>& Evaluator);

    // Override in derived
    virtual void Compute(const FVoxelSubsystem& Subsystem) {}
    virtual void Render(FVoxelRuntime& Runtime) {}
    virtual void Destroy(FVoxelRuntime& Runtime) {}
};
```

---

## Scatter Graph Workflow

### Basic Foliage Setup

```
[Get Scatter Bounds]
    │
    └──► [Generate 2D Points]
              Bounds: (from scatter bounds)
              Height: [Query Height Layer]
              CellSize: 200
              Jitter: 0.8
              │
              └──► [Set Attribute: Mesh]
                        Mesh: SM_Tree
                        │
                        └──► [Set Attribute: Scale]
                                  Scale: Random(0.8, 1.2)
                                  │
                                  └──► [Scatter Mesh]
                                            RenderDistance: 500
                                            ChunkSize: 64
```

### Multi-Mesh Scatter

```
[Generate 2D Points]
    │
    ├──► [Filter By Steepness]
    │         Min: 0, Max: 0.3
    │         │
    │         └──► [Set Mesh: Trees]
    │                   │
    │                   └──► [Scatter Mesh: Trees]
    │
    └──► [Filter By Steepness]
              Min: 0.3, Max: 1.0
              │
              └──► [Set Mesh: Rocks]
                        │
                        └──► [Scatter Mesh: Rocks]
```

---

## C++ Usage Examples

### Creating Scatter Actor Programmatically

```cpp
void AMyActor::SpawnScatterActor()
{
    // Load scatter graph
    UVoxelScatterGraph* Graph = LoadObject<UVoxelScatterGraph>(
        nullptr,
        TEXT("/Game/Voxel/Scatter/SG_Foliage"));

    if (!Graph)
    {
        return;
    }

    // Spawn scatter actor
    FActorSpawnParameters Params;
    AVoxelScatterActor* ScatterActor = GetWorld()->SpawnActor<AVoxelScatterActor>(
        AVoxelScatterActor::StaticClass(),
        GetActorLocation(),
        FRotator::ZeroRotator,
        Params);

    // Assign graph
    ScatterActor->Graph = Graph;

    // Create runtime (starts scatter generation)
    ScatterActor->CreateRuntime();
}
```

### Creating Custom Scatter Node

```cpp
// In your module's header
USTRUCT(Category = "Scatter")
struct MYGAME_API FVoxelNode_CustomScatter : public FVoxelNode_ScatterBase
{
    GENERATED_BODY()
    GENERATED_VOXEL_NODE_BODY()

    // Custom parameters
    VOXEL_INPUT_PIN(float, CustomDistance, 100.f);
    VOXEL_INPUT_PIN(bool, bUseRandomRotation, true);

    virtual TSharedRef<FVoxelScatterNodeRuntime> MakeRuntime() const override;
};

// Runtime class
class FVoxelCustomScatterRuntime : public TVoxelScatterNodeRuntime<FVoxelNode_CustomScatter>
{
public:
    virtual void Initialize(FVoxelGraphQueryImpl& Query) override;
    virtual void Compute(const FVoxelSubsystem& Subsystem) override;
    virtual void Render(FVoxelRuntime& Runtime) override;
    virtual void Destroy(FVoxelRuntime& Runtime) override;
};
```

### Accessing Point Data

```cpp
void ProcessPointSet(const FVoxelPointSet& Points)
{
    const int32 NumPoints = Points.Num();

    // Access position attribute
    const FVoxelVectorBuffer* Positions = Points.Find<FVoxelVectorBuffer>(FVoxelPointAttributes::Position);
    if (!Positions)
    {
        return;
    }

    // Access mesh attribute
    const FVoxelObjectBuffer* Meshes = Points.Find<FVoxelObjectBuffer>(FVoxelPointAttributes::Mesh);

    // Process each point
    for (int32 i = 0; i < NumPoints; i++)
    {
        FVector Position = (*Positions)[i];
        UStaticMesh* Mesh = Meshes ? Cast<UStaticMesh>((*Meshes)[i].Get()) : nullptr;

        // Use position and mesh...
    }
}
```

---

## Blueprint Usage

### Setting Up Scatter in Blueprint

1. **Create Scatter Graph**
   - Content Browser > Voxel > Scatter Graph
   - Build point generation logic

2. **Place Scatter Actor**
   - Add `AVoxelScatterActor` to level
   - Set Graph property
   - Actor auto-initializes on BeginPlay

3. **Configure Runtime**
   - Adjust ChunkSize for streaming granularity
   - Set RenderDistance per scatter node
   - Use parameter overrides for variation

### Common Node Setup (in Scatter Graph)

1. **Generate 2D Points** - Creates initial point grid
2. **Filter Points** - Remove based on slope, surface type
3. **Set Attribute** - Assign mesh, scale, rotation
4. **Scatter Mesh** - Output as instanced meshes

---

## Performance Optimization

### Chunk Size Guidelines

| Scenario | ChunkSize | Notes |
|----------|-----------|-------|
| Dense foliage | 32-64 | More granular streaming |
| Sparse trees | 128-256 | Fewer chunks to manage |
| Small props | 16-32 | Fine-grained culling |

### Render Distance Tips

- Use lower distances for small, dense objects
- Use higher distances for large, important objects
- Consider LOD systems for distant meshes

### Memory Management

```cpp
// The scatter system automatically manages:
// - Chunk loading/unloading based on camera
// - ISM component pooling
// - Point data streaming
```

---

## Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| No instances visible | Graph not assigned | Check AVoxelScatterActor.Graph |
| Missing mesh | No Mesh attribute | Add Set Mesh node before Scatter Mesh |
| Low density | CellSize too large | Reduce CellSize in Generate 2D Points |
| Performance issues | ChunkSize too small | Increase ChunkSize |
| Popping | RenderDistance too low | Increase RenderDistance |
| Z-fighting | Overlapping instances | Add random offset or culling |

---

## Scatter vs PCG

| Feature | Voxel Scatter | UE5 PCG |
|---------|---------------|---------|
| Integration | Native to VoxelPlugin | Requires PCGVoxelSampler |
| Streaming | Built-in chunked | Manual setup |
| Performance | Optimized for voxel | General purpose |
| Use case | Foliage on voxel terrain | Complex procedural logic |

### When to Use Each

**Use Voxel Scatter:**
- Simple foliage placement
- Dense vegetation
- Terrain-integrated props

**Use PCG with VoxelPlugin:**
- Complex generation rules
- Blueprint scripting
- Multi-step processing

---

## Related Topics

- [04-voxelworld-layerstack.md](04-voxelworld-layerstack.md) - Layer configuration
- [12-pcg-integration.md](12-pcg-integration.md) - PCG framework integration
- [14-query-system.md](14-query-system.md) - Terrain sampling for placement
- [02-voxelgraph-system.md](02-voxelgraph-system.md) - Graph node system
