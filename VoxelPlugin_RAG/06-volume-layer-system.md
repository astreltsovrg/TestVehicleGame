# Volume Layer System

## Summary

The volume layer system provides 3D SDF (Signed Distance Field) based terrain generation using `UVoxelVolumeLayer`. Volume stamps create caves, overhangs, and complex 3D structures that aren't possible with height-only terrain. This is the system for Minecraft-like terrain editing with proper 3D sculpting.

---

## Key Classes

| Class | Header | Purpose |
|-------|--------|---------|
| `UVoxelVolumeLayer` | VoxelLayer.h | Volume layer asset |
| `UVoxelVolumeGraph` | VoxelVolumeGraph.h | Visual graph for volume generation |
| `FVoxelVolumeStamp` | VoxelVolumeStamp.h | Base volume stamp struct |
| `FVoxelVolumeGraphStamp` | VoxelVolumeGraphStamp.h | Graph-based volume stamp |
| `FVoxelShapeStamp` | VoxelShapeStamp.h | Primitive shape stamp |
| `FVoxelMeshStamp` | VoxelMeshStamp.h | Static mesh as SDF stamp |
| `FVoxelVolumeSculptStamp` | VoxelVolumeSculptStamp.h | Runtime sculpting stamp |
| `EVoxelVolumeBlendMode` | VoxelVolumeBlendMode.h | Blend mode enum |
| `FVoxelShape` | VoxelShape.h | Base shape struct |

---

## EVoxelVolumeBlendMode

Controls how volume stamps combine with existing terrain:

```cpp
UENUM(BlueprintType, meta = (VoxelSegmentedEnum))
enum class EVoxelVolumeBlendMode : uint8
{
    Additive,    // Add material even without previous stamps
    Subtractive, // Carve out even without previous stamps
    Intersect,   // Keep only intersection
    Override     // Replace completely
};
```

### Blend Mode Behavior (SDF Operations)

| Mode | SDF Formula | Use Case |
|------|-------------|----------|
| **Additive** | `min(Existing, New)` | Adding solid material (hills, rocks, structures) |
| **Subtractive** | `max(Existing, -New)` | Carving caves, tunnels, holes |
| **Intersect** | `max(Existing, New)` | Cutting shapes, boolean intersection |
| **Override** | `New` | Complete terrain replacement |

### Visual Representation

```
Additive (Union):              Subtractive (Difference):
    ┌───┐                          ┌───────────┐
   ┌┼───┼┐                         │     ○     │
   │└───┘│                         │   (hole)  │
   └─────┘                         └───────────┘
   (adds sphere                    (removes sphere
    to cube)                        from cube)

Intersect:                     Override:
    ┌───┐                          ┌───────────┐
    │ ○ │                          │  NEW ONLY │
    └───┘                          └───────────┘
   (only overlap                   (replaces all)
    remains)
```

---

## Signed Distance Fields (SDF)

VoxelPlugin2 uses SDF representation for volume terrain:

```
Distance > 0:  Outside (air)
Distance = 0:  Surface boundary
Distance < 0:  Inside (solid)

Example: Sphere with radius 100cm at origin
- Point (0, 0, 0):   Distance = -100 (center, inside)
- Point (50, 0, 0):  Distance = -50  (inside)
- Point (100, 0, 0): Distance = 0    (on surface)
- Point (150, 0, 0): Distance = 50   (outside)
```

### SDF Operations

```cpp
// Union (Additive) - adds volumes together
SDF_Union = min(SDF_A, SDF_B);

// Difference (Subtractive) - removes B from A
SDF_Difference = max(SDF_A, -SDF_B);

// Intersection - keeps only overlap
SDF_Intersection = max(SDF_A, SDF_B);

// Smooth Union - smooth blending
SDF_SmoothUnion = min(SDF_A, SDF_B) - h*h*0.25/k;
// where k = smoothness, h = max(k - abs(SDF_A - SDF_B), 0)
```

---

## UVoxelVolumeGraph

Visual programming graph for procedural volume generation.

```cpp
UCLASS(BlueprintType, meta = (AssetSubMenu = "Graph"))
class VOXEL_API UVoxelVolumeGraph : public UVoxelGraph
{
    GENERATED_BODY()

public:
    virtual UScriptStruct* GetOutputNodeStruct() const override;
};
```

### Creating Volume Graph

1. Content Browser > Right-click
2. **Voxel > Volume Graph**
3. Double-click to open graph editor

### Standard Volume Graph Structure

```
[Make Seed (12345)]
    │
    └──► [Advanced Noise 3D]
              Position: [Get Position 3D]
              Amplitude: 100000
              FeatureScale: 200000
              NumOctaves: 4
              │
              ├──► [Subtract] ◄── [Get Position 3D.Z]
              │         │
              │         └──► [Output Volume]
              │                   Distance: (result)
              │                   SurfaceType: (optional)
              │                   Bounds: (optional)
```

### Output Volume Node Inputs

| Pin | Type | Default | Description |
|-----|------|---------|-------------|
| `Distance` | `FVoxelFloatBuffer` | Required | SDF distance at query positions |
| `SurfaceType` | `FVoxelSurfaceTypeBlendBuffer` | nullptr | Surface material blending |
| `Alpha` | `FVoxelFloatBuffer` | 1.0 | Override blend weight (0-1) |
| `Bounds` | `FVoxelBox` | nullptr | Explicit bounds override |
| `EnableLayerOverride` | `bool` | false | Override stamp's layer |
| `LayerOverride` | `FVoxelVolumeLayerObject` | nullptr | New layer target |
| `EnableBlendModeOverride` | `bool` | false | Override stamp's blend mode |
| `BlendModeOverride` | `EVoxelVolumeBlendMode` | Override | New blend mode |

---

## FVoxelVolumeStamp (Base Class)

Abstract base for all volume stamps.

```cpp
USTRUCT(meta = (Abstract))
struct VOXEL_API FVoxelVolumeStamp : public FVoxelStamp
{
    GENERATED_BODY()

public:
    // Target layer for this stamp
    UPROPERTY(EditAnywhere, Category = "Config")
    TObjectPtr<UVoxelVolumeLayer> Layer;

    // How to blend with existing terrain
    UPROPERTY(EditAnywhere, Category = "Config")
    EVoxelVolumeBlendMode BlendMode = EVoxelVolumeBlendMode::Additive;

    // Additional layers this stamp affects
    UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay)
    TArray<TObjectPtr<UVoxelVolumeLayer>> AdditionalLayers;
};
```

### Inherited from FVoxelStamp

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `Priority` | `int32` | 0 | Processing order |
| `Smoothness` | `float` | 0 | Edge smoothing in cm |
| `Transform` | `FVoxelStampTransform` | - | Position, rotation, scale |

---

## Volume Stamp Types

### FVoxelVolumeGraphStamp

Uses a `UVoxelVolumeGraph` for procedural generation.

```cpp
USTRUCT(meta = (ShortName = "Graph", Icon = "ClassIcon.Blueprint", SortOrder = 2))
struct VOXEL_API FVoxelVolumeGraphStamp final : public FVoxelVolumeStamp
{
    GENERATED_BODY()

public:
    // The volume graph to execute
    UPROPERTY(EditAnywhere, Category = "Config")
    TObjectPtr<UVoxelVolumeGraph> Graph;

    // Parameter overrides for the graph
    UPROPERTY()
    FVoxelParameterOverrides ParameterOverrides;
};
```

**Use cases:**
- Procedural caves (noise-based)
- Complex 3D structures
- Parametric volumes

### FVoxelShapeStamp

Primitive shape stamp using SDF shapes.

```cpp
USTRUCT(meta = (ShortName = "Shape", Icon = "ClassIcon.Cube", SortOrder = 4))
struct VOXEL_API FVoxelShapeStamp final : public FVoxelVolumeStamp
{
    GENERATED_BODY()

public:
    // Shape to use (Sphere, Cube, Plane)
    UPROPERTY(EditAnywhere, Category = "Config")
    TVoxelInstancedStruct<FVoxelShape> Shape;

    // Surface material
    UPROPERTY(EditAnywhere, Category = "Config")
    TObjectPtr<UVoxelSurfaceTypeInterface> SurfaceType;
};
```

**Use cases:**
- Quick prototyping
- Simple volume additions/subtractions
- Platform/structure generation

### FVoxelMeshStamp

Converts static mesh to SDF volume.

```cpp
USTRUCT(meta = (ShortName = "Mesh", Icon = "ClassIcon.StaticMesh", SortOrder = 0))
struct VOXEL_API FVoxelMeshStamp final : public FVoxelVolumeStamp
{
    GENERATED_BODY()

public:
    // Voxel static mesh asset
    UPROPERTY(EditAnywhere, Category = "Config", DisplayName = "Mesh")
    TObjectPtr<UVoxelStaticMesh> NewMesh;

    // Surface material
    UPROPERTY(EditAnywhere, Category = "Config")
    TObjectPtr<UVoxelSurfaceTypeInterface> SurfaceType;

    // Tricubic interpolation is ~3x slower but better looking
    UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay)
    bool bUseTricubic = true;
};
```

**Use cases:**
- Complex mesh-based volumes
- Pre-authored terrain features
- Imported assets as terrain

### FVoxelVolumeSculptStamp

Runtime-editable volume data (for Minecraft-like editing).

```cpp
USTRUCT(meta = (Internal))
struct VOXEL_API FVoxelVolumeSculptStamp final : public FVoxelVolumeStamp
{
    GENERATED_BODY()

public:
    // Resolution scale (100 = 1m per voxel)
    UPROPERTY(EditAnywhere, Category = "Config")
    float Scale = 100;

    // Compress distances to one byte (faster, less precise)
    UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay)
    bool bUseFastDistances = false;

    // Enable edit diffing (slower but handles moved stamps)
    UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay)
    bool bEnableDiffing = true;

    // Override layer stack for distance queries
    UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay)
    TObjectPtr<UVoxelLayerStack> StackOverride;

public:
    // Apply a volume modifier
    FVoxelFuture ApplyModifier(const TSharedRef<const FVoxelVolumeModifier>& Modifier);

    // Clear all sculpt data
    FVoxelFuture ClearSculptData();

    // Get/Set internal data
    TSharedRef<FVoxelVolumeSculptData> GetData() const;
    void SetData(const TSharedRef<FVoxelVolumeSculptData>& NewData);
};
```

**Use cases:**
- Player terrain editing
- Dynamic cave carving
- Saveable terrain modifications
- Minecraft-style building/digging

---

## Built-in Shapes

### FVoxelSphereShape

```cpp
USTRUCT(meta = (ShortName = "Sphere", SortOrder = 0))
struct VOXEL_API FVoxelSphereShape : public FVoxelShape
{
    GENERATED_BODY()

public:
    // Sphere radius in cm
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    double Radius = 1000.f;  // 10m default
};
```

### FVoxelCubeShape

```cpp
USTRUCT(meta = (ShortName = "Cube", SortOrder = 1))
struct VOXEL_API FVoxelCubeShape : public FVoxelShape
{
    GENERATED_BODY()

public:
    // Box size in cm
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    FVector Size = FVector(1000.f);  // 10m cube default

    // Corner roundness (0 = sharp, 1 = fully rounded)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    float Roundness = 0.f;
};
```

### FVoxelPlaneShape

```cpp
USTRUCT(meta = (ShortName = "Plane", SortOrder = 2))
struct VOXEL_API FVoxelPlaneShape : public FVoxelShape
{
    GENERATED_BODY()

public:
    // Plane size in cm
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    FVector2D Size = FVector2D(1000.f);

    // Vertical extent multiplier
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", AdvancedDisplay)
    double Height = 1.f;
};
```

---

## Runtime Stamp Classes

Each stamp type has a corresponding runtime class:

| Stamp | Runtime |
|-------|---------|
| `FVoxelVolumeGraphStamp` | `FVoxelVolumeGraphStampRuntime` |
| `FVoxelShapeStamp` | `FVoxelShapeStampRuntime` |
| `FVoxelMeshStamp` | `FVoxelMeshStampRuntime` |
| `FVoxelVolumeSculptStamp` | `FVoxelVolumeSculptStampRuntime` |

### FVoxelVolumeStampRuntime Base

```cpp
USTRUCT()
struct VOXEL_API FVoxelVolumeStampRuntime : public FVoxelStampRuntime
{
public:
    EVoxelVolumeBlendMode GetBlendMode() const;
    float GetSmoothness() const;

    // Override in derived classes
    virtual void Apply(
        const FVoxelVolumeBulkQuery& Query,
        const FVoxelVolumeTransform& StampToQuery) const;

    virtual void Apply(
        const FVoxelVolumeSparseQuery& Query,
        const FVoxelVolumeTransform& StampToQuery) const;
};
```

---

## Setup Workflow

### Creating Volume Layer Setup

1. **Create Volume Layer**
   - Content Browser > Voxel > Volume Layer
   - Name: `VL_MainVolume`

2. **Create Volume Graph** (optional)
   - Content Browser > Voxel > Volume Graph
   - Build noise-based or SDF generation
   - Connect to Output Volume node

3. **Create Layer Stack**
   - Content Browser > Voxel > Layer Stack
   - Add `VL_MainVolume` to VolumeLayers array

4. **Place Stamp Actor**
   - Drag Volume Shape Stamp actor into level
   - Assign `VL_MainVolume` as Layer
   - Configure shape (Sphere, Cube, etc.)
   - Set BlendMode (Additive/Subtractive)

5. **Configure VoxelWorld**
   - Assign LayerStack to VoxelWorld
   - Terrain generates on Play

### Hybrid Height + Volume Setup

```
LayerStack
├── HeightLayers
│   └── [0] Base Ground (noise-based)
└── VolumeLayers
    ├── [0] Caves Layer (subtractive stamps)
    └── [1] Sculpt Layer (player modifications)
```

---

## C++ Usage Examples

### Creating Shape Stamp Programmatically

```cpp
void AMyActor::CreateSphereVolume(FVector Location, float Radius, bool bSubtract)
{
    // Create stamp actor
    FActorSpawnParameters Params;
    AVoxelStampActor* StampActor = GetWorld()->SpawnActor<AVoxelStampActor>(
        AVoxelStampActor::StaticClass(),
        Location,
        FRotator::ZeroRotator,
        Params);

    // Get stamp component and configure
    UVoxelStampComponent* StampComp = StampActor->GetStampComponent();

    // Create shape stamp with sphere
    FVoxelShapeStamp& Stamp = StampComp->CreateStamp<FVoxelShapeStamp>();
    Stamp.Layer = MyVolumeLayer;

    // Configure sphere shape
    FVoxelSphereShape SphereShape;
    SphereShape.Radius = Radius;
    Stamp.Shape.InitializeAs<FVoxelSphereShape>(SphereShape);

    // Set blend mode
    Stamp.BlendMode = bSubtract
        ? EVoxelVolumeBlendMode::Subtractive
        : EVoxelVolumeBlendMode::Additive;

    Stamp.SurfaceType = MySurfaceType;

    // Register with world
    StampComp->RegisterStamp();
}
```

### Creating Cave System with Noise

```cpp
// In custom volume graph node
void FVoxelNode_CaveNoise::Compute(FVoxelGraphQuery Query) const
{
    // Get 3D positions
    const FVoxelVectorBuffer& Positions = Query.Get(PositionPin);

    // Create output distances
    FVoxelFloatBuffer Distances;
    Distances.SetNum(Positions.Num());

    // Generate cave-like noise
    for (int32 i = 0; i < Positions.Num(); i++)
    {
        const FVector& Pos = Positions[i];

        // 3D noise for cave structure
        float NoiseValue = FMath::PerlinNoise3D(Pos / FeatureScale);

        // Depth-based cave probability
        float DepthFactor = FMath::Clamp(-Pos.Z / MaxDepth, 0.f, 1.f);

        // Combine for cave SDF
        // Negative = inside cave, Positive = solid rock
        Distances[i] = (NoiseValue - CaveThreshold) * Amplitude * DepthFactor;
    }

    Query.Set(DistanceOutput, MoveTemp(Distances));
}
```

### Using Volume Sculpt Stamp

```cpp
void AMyGameMode::SetupTerrainEditing()
{
    // Create sculpt actor
    AVoxelVolumeSculptActor* SculptActor = GetWorld()->SpawnActor<AVoxelVolumeSculptActor>(
        AVoxelVolumeSculptActor::StaticClass(),
        FVector::ZeroVector,
        FRotator::ZeroRotator);

    // Configure sculpt stamp
    FVoxelVolumeSculptStamp& SculptStamp = SculptActor->GetSculptStamp();
    SculptStamp.Layer = EditableVolumeLayer;
    SculptStamp.Scale = 100.f;  // 1m voxels
    SculptStamp.bUseFastDistances = true;  // Performance optimization
    SculptStamp.bEnableDiffing = false;    // Faster editing

    // Store reference for later use
    PlayerSculptActor = SculptActor;
}
```

---

## Blueprint Usage

### Setting Up Volume Stamp

1. Select Volume Stamp actor
2. In Details panel > Config section
3. Set **Layer** to your Volume Layer
4. Configure **Shape**:
   - Choose Sphere, Cube, or Plane
   - Set size/radius parameters
5. Set **Blend Mode**:
   - **Additive** - Add terrain
   - **Subtractive** - Remove terrain

### Using Smoothness

Smoothness creates gradual transitions between volumes:

```
Smoothness = 0:               Smoothness = 500:
   ┌────────┐                    ╭────────╮
   │        │                   /          \
   │        │                  /            \
   └────────┘                 ╯              ╰
   (sharp edges)              (smooth blend)
```

Works with all blend modes for soft unions/differences.

---

## Performance Tips

### bUseFastDistances

| Setting | Storage | Performance | Quality |
|---------|---------|-------------|---------|
| `false` | 32-bit float | Slower | High precision |
| `true` | 8-bit (1 byte) | 4x faster | Limited range |

Use `bUseFastDistances = true` for:
- Large editable areas
- Mobile platforms
- Performance-critical scenarios

### bEnableDiffing

| Setting | Edit Speed | Moving Stamps | Memory |
|---------|------------|---------------|--------|
| `true` | Slower | Works correctly | Higher |
| `false` | Up to 5x faster | Visual artifacts | Lower |

Use `bEnableDiffing = false` when:
- Stamps won't be moved after sculpting
- Performance is critical
- Player won't notice artifacts

---

## Best Practices

### Blend Mode Selection

| Scenario | Recommended Mode | Smoothness |
|----------|------------------|------------|
| Adding rocks/structures | Additive | 0-200 |
| Carving caves/tunnels | Subtractive | 100-500 |
| Precise cutouts | Intersect | 0 |
| Complete replacement | Override | 0 |

### Layer Organization

```
LayerStack
└── VolumeLayers
    ├── [0] Procedural Caves (noise graph)
    ├── [1] Placed Features (shape stamps)
    └── [2] Player Edits (sculpt stamp)
```

### Volume vs Height

| Feature | Height Layer | Volume Layer |
|---------|--------------|--------------|
| Caves | No | Yes |
| Overhangs | No | Yes |
| Floating islands | No | Yes |
| Performance | Better | More expensive |
| Memory | Lower | Higher |
| Sculpting | Limited | Full 3D |

---

## Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| Holes in volume | MaxDistance too small | Increase LayerStack.MaxDistance |
| No visible terrain | BlendMode is Intersect with nothing to intersect | Use Additive for first stamp |
| Artifacts when moving stamps | bEnableDiffing = false | Enable diffing or don't move stamps |
| Poor performance | Too many overlapping stamps | Reduce stamp count, use sculpt stamps |
| Z-fighting | Overlapping same-priority stamps | Adjust Priority values |
| Blocky appearance | Scale too large | Reduce sculpt Scale value |

---

## SDF Distance Field Tips

### Debugging SDF

```cpp
// Visualize SDF values
float Distance = SampleSDF(Position);
if (Distance < 0)
{
    // Inside solid
    DrawDebugPoint(World, Position, 5.f, FColor::Red);
}
else if (Distance < 100.f)
{
    // Near surface
    DrawDebugPoint(World, Position, 5.f, FColor::Yellow);
}
else
{
    // Outside (air)
    DrawDebugPoint(World, Position, 5.f, FColor::Green);
}
```

### Common SDF Formulas

```cpp
// Sphere: distance to surface
float SphereSDF(FVector P, float Radius)
{
    return P.Size() - Radius;
}

// Box: distance to surface
float BoxSDF(FVector P, FVector Size)
{
    FVector Q = P.GetAbs() - Size;
    return FVector::Max(Q, FVector::ZeroVector).Size()
         + FMath::Min(FMath::Max(Q.X, FMath::Max(Q.Y, Q.Z)), 0.f);
}

// Smooth union
float SmoothUnion(float A, float B, float K)
{
    float H = FMath::Max(K - FMath::Abs(A - B), 0.f);
    return FMath::Min(A, B) - H * H * 0.25f / K;
}
```

---

## Related Topics

- [04-voxelworld-layerstack.md](04-voxelworld-layerstack.md) - Layer configuration
- [05-height-layer-system.md](05-height-layer-system.md) - 2D terrain alternative
- [07-stamp-system.md](07-stamp-system.md) - Stamp fundamentals
- [08-runtime-sculpting.md](08-runtime-sculpting.md) - Blueprint sculpting API
- [09-persistence-saveload.md](09-persistence-saveload.md) - Saving sculpt data
