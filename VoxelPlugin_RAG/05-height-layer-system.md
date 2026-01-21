# Height Layer System

## Summary

The height layer system provides 2D heightfield-based terrain generation using `UVoxelHeightLayer`. Height stamps modify terrain by setting height values with blend modes (Max, Min, Override). This is the primary system for traditional terrain surfaces.

---

## Key Classes

| Class | Header | Purpose |
|-------|--------|---------|
| `UVoxelHeightLayer` | VoxelLayer.h | Height layer asset |
| `UVoxelHeightGraph` | VoxelHeightGraph.h | Visual graph for height generation |
| `FVoxelHeightStamp` | VoxelHeightStamp.h | Base height stamp struct |
| `FVoxelHeightGraphStamp` | VoxelHeightGraphStamp.h | Graph-based height stamp |
| `FVoxelHeightmapStamp` | VoxelHeightmapStamp.h | Imported heightmap stamp |
| `FVoxelHeightSculptStamp` | VoxelHeightSculptStamp.h | Runtime sculpting stamp |
| `EVoxelHeightBlendMode` | VoxelHeightBlendMode.h | Blend mode enum |

---

## EVoxelHeightBlendMode

Controls how height stamps combine with existing terrain:

```cpp
UENUM(BlueprintType, meta = (VoxelSegmentedEnum))
enum class EVoxelHeightBlendMode : uint8
{
    Max,      // Take maximum height (additive hills)
    Min,      // Take minimum height (carving valleys)
    Override  // Replace height completely
};
```

### Blend Mode Behavior

| Mode | Formula | Use Case |
|------|---------|----------|
| **Max** | `Result = max(Existing, New)` | Adding hills, mountains, raised platforms |
| **Min** | `Result = min(Existing, New)` | Carving valleys, rivers, trenches |
| **Override** | `Result = New` | Flat roads, complete terrain replacement |

### Visual Representation

```
Max Blend:                  Min Blend:                  Override:
     ┌────┐                      ────                        ┌────────┐
    /      \                    /    \                       │        │
───/        \───           ────┘      └────              ────┘        └────
   (hill on                (valley in               (flat section
    flat ground)            terrain)                 replaces all)
```

---

## UVoxelHeightGraph

Visual programming graph for procedural height generation.

```cpp
UCLASS(BlueprintType, meta = (AssetSubMenu = "Graph"))
class VOXEL_API UVoxelHeightGraph : public UVoxelGraph
{
    GENERATED_BODY()

public:
    virtual UScriptStruct* GetOutputNodeStruct() const override;
};
```

### Creating Height Graph

1. Content Browser > Right-click
2. **Voxel > Height Graph**
3. Double-click to open graph editor

### Standard Height Graph Structure

```
[Make Seed (12345)]
    │
    └──► [Advanced Noise 2D]
              Position: [Get Position 2D]
              Amplitude: 50000
              FeatureScale: 200000
              NumOctaves: 6
              │
              └──► [Output Height]
                        Height: (noise output)
```

### Output Height Node Inputs

| Pin | Type | Description |
|-----|------|-------------|
| `Height` | `float` | Height value at query position |
| `BlendModeOverride` | `EVoxelHeightBlendMode` | Override stamp blend mode |

---

## FVoxelHeightStamp (Base Class)

Abstract base for all height stamps.

```cpp
USTRUCT(meta = (Abstract))
struct VOXEL_API FVoxelHeightStamp : public FVoxelStamp
{
    GENERATED_BODY()

public:
    // Target layer for this stamp
    UPROPERTY(EditAnywhere, Category = "Config")
    TObjectPtr<UVoxelHeightLayer> Layer;

    // How to blend with existing terrain
    UPROPERTY(EditAnywhere, Category = "Config")
    EVoxelHeightBlendMode BlendMode = EVoxelHeightBlendMode::Max;

    // Additional layers this stamp affects
    UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay)
    TArray<TObjectPtr<UVoxelHeightLayer>> AdditionalLayers;
};
```

### Inherited from FVoxelStamp

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `Priority` | `int32` | 0 | Processing order |
| `Smoothness` | `float` | 0 | Edge smoothing (for non-Override modes) |
| `Transform` | `FVoxelStampTransform` | - | Position, rotation, scale |

---

## Height Stamp Types

### FVoxelHeightGraphStamp

Uses a `UVoxelHeightGraph` for procedural generation.

```cpp
USTRUCT(meta = (ShortName = "Graph", Icon = "ClassIcon.Blueprint"))
struct VOXEL_API FVoxelHeightGraphStamp : public FVoxelHeightStamp
{
    GENERATED_BODY()

public:
    // The height graph to execute
    UPROPERTY(EditAnywhere, Category = "Config")
    TObjectPtr<UVoxelHeightGraph> Graph;

    // Parameter overrides for the graph
    UPROPERTY()
    FVoxelParameterOverrides ParameterOverrides;
};
```

**Use cases:**
- Procedural terrain (noise-based)
- Parametric shapes
- Complex height functions

### FVoxelHeightmapStamp

Imports height from a `UVoxelHeightmap` asset.

```cpp
USTRUCT(meta = (ShortName = "Heightmap", Icon = "LandscapeEditor.Target_Heightmap"))
struct VOXEL_API FVoxelHeightmapStamp : public FVoxelHeightStamp
{
    GENERATED_BODY()

public:
    // Heightmap asset to use
    UPROPERTY(EditAnywhere, Category = "Config")
    TObjectPtr<UVoxelHeightmap> Heightmap;

    // Default surface when no weightmap applies
    UPROPERTY(EditAnywhere, Category = "Config")
    TObjectPtr<UVoxelSurfaceTypeInterface> DefaultSurfaceType;

    // Weightmap-to-surface mappings
    UPROPERTY(EditAnywhere, Category = "Config")
    TArray<FVoxelHeightmapStampWeightmapSurfaceType> WeightmapSurfaceTypes;
};
```

**Use cases:**
- Real-world terrain data
- Pre-designed heightmaps
- Landscape conversion

### FVoxelHeightSculptStamp

Runtime-editable height data.

```cpp
USTRUCT(meta = (Internal))
struct VOXEL_API FVoxelHeightSculptStamp : public FVoxelHeightStamp
{
    GENERATED_BODY()

public:
    // Resolution scale (100 = 1m per sample)
    UPROPERTY(EditAnywhere, Category = "Config")
    float ScaleXY = 100;

    // Store heights relative to previous stamps
    UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay)
    bool bRelativeHeight = false;

    // Optional stack for distance queries
    UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay)
    TObjectPtr<UVoxelLayerStack> StackOverride;

public:
    // Apply a height modifier
    FVoxelFuture ApplyModifier(const TSharedRef<const FVoxelHeightModifier>& Modifier);

    // Clear all sculpt data
    FVoxelFuture ClearSculptData();

    // Get/Set internal data
    TSharedRef<FVoxelHeightSculptData> GetData() const;
    void SetData(const TSharedRef<FVoxelHeightSculptData>& NewData);
};
```

**Use cases:**
- Player terrain editing
- Dynamic terrain modification
- Saveable terrain changes

---

## Runtime Stamp Classes

Each stamp type has a corresponding runtime class:

| Stamp | Runtime |
|-------|---------|
| `FVoxelHeightGraphStamp` | `FVoxelHeightGraphStampRuntime` |
| `FVoxelHeightmapStamp` | `FVoxelHeightmapStampRuntime` |
| `FVoxelHeightSculptStamp` | `FVoxelHeightSculptStampRuntime` |

### FVoxelHeightStampRuntime Base

```cpp
USTRUCT()
struct VOXEL_API FVoxelHeightStampRuntime : public FVoxelStampRuntime
{
public:
    EVoxelHeightBlendMode GetBlendMode() const;
    float GetSmoothness() const;

    // Override in derived classes
    virtual void Apply(
        const FVoxelHeightBulkQuery& Query,
        const FVoxelHeightTransform& StampToQuery) const;
};
```

---

## Weightmap System

For heightmap stamps, weightmaps provide surface type blending:

```cpp
USTRUCT(BlueprintType)
struct FVoxelHeightmapStampWeightmapSurfaceType
{
    // Index of the weightmap channel
    UPROPERTY(EditAnywhere, Category = "Voxel", meta = (ClampMin = 0))
    int32 Index = 0;

    // Surface type to apply
    UPROPERTY(EditAnywhere, Category = "Voxel")
    TObjectPtr<UVoxelSurfaceTypeInterface> SurfaceType;
};
```

### Weightmap Configuration

```
WeightmapSurfaceTypes:
├── [0] Index: 0, SurfaceType: Grass
├── [1] Index: 1, SurfaceType: Rock
└── [2] Index: 2, SurfaceType: Sand
```

---

## Height Spline Stamps

For road/path integration, see the spline system. Height spline stamps flatten terrain along paths:

```cpp
// Located in: Spline/VoxelHeightSplineStamp.h
struct FVoxelHeightSplineStamp : FVoxelHeightStamp
{
    // Spline component for path
    UVoxelSplineComponent* Spline;

    // Width of flattened area
    float Width;

    // Falloff distance
    float Falloff;
};
```

---

## Setup Workflow

### Creating Height Layer Setup

1. **Create Height Layer**
   - Content Browser > Voxel > Height Layer
   - Name: `HL_MainTerrain`

2. **Create Height Graph**
   - Content Browser > Voxel > Height Graph
   - Build noise-based generation
   - Connect to Output Height node

3. **Create Layer Stack**
   - Content Browser > Voxel > Layer Stack
   - Add `HL_MainTerrain` to HeightLayers array

4. **Place Stamp Actor**
   - Drag Height Graph Stamp actor into level
   - Assign `HL_MainTerrain` as Layer
   - Assign your Height Graph

5. **Configure VoxelWorld**
   - Assign LayerStack to VoxelWorld
   - Terrain generates on Play

---

## C++ Usage Examples

### Creating Height Graph Stamp Programmatically

```cpp
void AMyActor::CreateHeightStamp()
{
    // Create stamp actor
    FActorSpawnParameters Params;
    AVoxelStampActor* StampActor = GetWorld()->SpawnActor<AVoxelStampActor>(
        AVoxelStampActor::StaticClass(),
        GetActorLocation(),
        FRotator::ZeroRotator,
        Params);

    // Get stamp component and configure
    UVoxelStampComponent* StampComp = StampActor->GetStampComponent();

    // Create height graph stamp
    FVoxelHeightGraphStamp& Stamp = StampComp->CreateStamp<FVoxelHeightGraphStamp>();
    Stamp.Layer = MyHeightLayer;
    Stamp.Graph = MyHeightGraph;
    Stamp.BlendMode = EVoxelHeightBlendMode::Max;
    Stamp.Priority = 100;

    // Register with world
    StampComp->RegisterStamp();
}
```

### Querying Height at Position

```cpp
float AMyActor::GetTerrainHeight(FVector2D Position)
{
    AVoxelWorld* World = GetVoxelWorld();
    if (!World || !World->IsVoxelWorldReady())
    {
        return 0.f;
    }

    // Use height query
    FVoxelHeightQuery Query;
    Query.Position = Position;

    float Height = 0.f;
    if (World->QueryHeight(Query, Height))
    {
        return Height;
    }

    return 0.f;
}
```

---

## Blueprint Usage

### Setting Blend Mode

1. Select Height Stamp actor
2. In Details panel > Config section
3. Set **Blend Mode** dropdown:
   - **Max** - Add terrain on top
   - **Min** - Carve into terrain
   - **Override** - Replace terrain

### Using Smoothness

When using Max or Min blend:
- **Smoothness = 0** - Sharp edges
- **Smoothness > 0** - Gradual transition

Override mode ignores Smoothness (hard boundary).

---

## Best Practices

### Blend Mode Selection

| Scenario | Recommended Mode |
|----------|------------------|
| Mountains, hills | Max |
| Valleys, rivers | Min |
| Roads, flat areas | Override |
| Raised platforms | Override (with smooth edges) |

### Performance Tips

1. **Minimize Override stamps** - They prevent early-out optimizations
2. **Use appropriate graph complexity** - Lower octaves for distant terrain
3. **Batch similar stamps** - Same layer, same blend mode
4. **Consider heightmap stamps** - More efficient than complex graphs

### Layer Organization

```
LayerStack
└── HeightLayers
    ├── [0] Base Terrain (procedural noise)
    ├── [1] Roads Layer (spline-flattened)
    └── [2] Details Layer (small features)
```

---

## Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| Height clipping | MaxDistance too small | Increase LayerStack.MaxDistance |
| Seams between stamps | Different blend modes | Use consistent blend modes |
| Smoothness ignored | Using Override mode | Switch to Max/Min mode |
| Z-fighting | Overlapping stamps same priority | Adjust Priority values |
| Performance issues | Too many Override stamps | Consolidate or use Max/Min |

---

## Smoothness Behavior

Smoothness creates a gradual blend at stamp edges:

```
Smoothness = 0:               Smoothness = 500:
     ┌────┐                      ╭────╮
     │    │                     /      \
─────┘    └─────           ────╯        ╰────
(sharp edge)               (gradual falloff)
```

Only applies to Max and Min modes. Override always has sharp boundaries.

---

## Related Topics

- [03-noise-generation.md](03-noise-generation.md) - Noise for height graphs
- [04-voxelworld-layerstack.md](04-voxelworld-layerstack.md) - Layer configuration
- [06-volume-layer-system.md](06-volume-layer-system.md) - 3D terrain alternative
- [07-stamp-system.md](07-stamp-system.md) - Stamp fundamentals
- [10-spline-system.md](10-spline-system.md) - Road/path generation
- [11-heightmap-import.md](11-heightmap-import.md) - Importing heightmaps
