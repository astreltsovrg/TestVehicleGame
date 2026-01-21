# VoxelWorld & LayerStack

## Summary

The `AVoxelWorld` actor is the main entry point for VoxelPlugin2 terrain in a level. It references a `UVoxelLayerStack` that configures which layers (height and volume) compose the terrain. This document covers world configuration, layer stack setup, and the layer reference system.

---

## Key Classes

| Class | Header | Purpose |
|-------|--------|---------|
| `AVoxelWorld` | VoxelWorld.h | Main terrain actor |
| `UVoxelLayerStack` | VoxelLayerStack.h | Container for height and volume layers |
| `UVoxelLayer` | VoxelLayer.h | Abstract base for all layers |
| `UVoxelHeightLayer` | VoxelLayer.h | 2D height layer type |
| `UVoxelVolumeLayer` | VoxelLayer.h | 3D volume layer type |
| `FVoxelStackLayer` | VoxelStackLayer.h | Reference to layer within stack |
| `EVoxelLayerType` | VoxelStackLayer.h | Height or Volume enum |

---

## AVoxelWorld Actor

The main terrain actor that renders and manages voxel terrain.

### Core Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `LayerStack` | `UVoxelLayerStack*` | Default() | Layer configuration asset |
| `VoxelSize` | `int32` | 100 | Voxel size in cm (1m default) |
| `MegaMaterial` | `UVoxelMegaMaterial*` | nullptr | Multi-material rendering |
| `LODQuality` | `FVoxelLODQuality` | - | Min/Max LOD quality |
| `QualityExponent` | `double` | 1.0 | LOD selection bias |

### Runtime Control

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `bCreateRuntimeOnBeginPlay` | `bool` | true | Auto-create runtime at BeginPlay |
| `bWaitOnBeginPlay` | `bool` | true | Wait for initial generation |

### Nanite Settings

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `bEnableNanite` | `bool` | true | Enable Nanite rendering |
| `bEnableTessellation` | `bool` | true | Enable Nanite tessellation |
| `NaniteMaxTessellationLOD` | `int32` | 2 | Max LOD for tessellation |
| `NanitePositionPrecision` | `int32` | 6 | Position bit precision |

### Rendering Settings

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `bEnableLumen` | `bool` | false | Enable Lumen GI support |
| `bEnableRaytracing` | `bool` | false | Enable raytracing effects |
| `bGenerateMeshDistanceFields` | `bool` | false | Generate distance fields |
| `RenderChunkSize` | `EVoxelRenderChunkSize` | Size32 | Render chunk size |

### Collision Settings

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `CollisionChunkSize` | `int32` | 32 | Collision chunk size |
| `bDoubleSidedCollision` | `bool` | false | Double-sided collision mesh |
| `VisibilityCollision` | `FCollisionProfileName` | BlockAll | Visibility collision profile |
| `InvokerCollision` | `FCollisionProfileName` | BlockAll | Invoker collision profile |

---

## UVoxelLayerStack

Container asset that defines which layers compose the terrain.

### Class Declaration

```cpp
UCLASS(BlueprintType, meta = (VoxelAssetType, AssetColor=Grey))
class VOXEL_API UVoxelLayerStack : public UVoxelAsset
{
    GENERATED_BODY()

public:
    // Height layers (2D terrain generation)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    TArray<TObjectPtr<UVoxelHeightLayer>> HeightLayers;

    // Volume layers (3D terrain generation)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    TArray<TObjectPtr<UVoxelVolumeLayer>> VolumeLayers;

    // How far up & down to extend the distance field from height layers
    // Lower values generate fewer chunks
    UPROPERTY(EditAnywhere, Category = "Config")
    float MaxDistance = 100000.f;  // 1km default

public:
    static UVoxelLayerStack* Default();
};
```

### Key Methods

| Method | Description |
|--------|-------------|
| `Default()` | Get/create default layer stack |

### Creating a LayerStack

1. Right-click in Content Browser
2. Select **Voxel > Layer Stack**
3. Add height and/or volume layers
4. Assign to `AVoxelWorld.LayerStack`

---

## Layer Types

### EVoxelLayerType

```cpp
UENUM()
enum class EVoxelLayerType : uint8
{
    Height,  // 2D heightfield terrain
    Volume   // 3D volumetric terrain (SDF)
};
```

### UVoxelLayer (Base Class)

```cpp
UCLASS(BlueprintType, Abstract)
class VOXEL_API UVoxelLayer : public UVoxelAsset
{
    GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Editor")
    FVoxelAssetIcon AssetIcon;
#endif

public:
    EVoxelLayerType GetType() const;
};
```

### UVoxelHeightLayer

2D heightfield layer for terrain surface generation.

```cpp
UCLASS(meta = (VoxelAssetType, AssetColor=Grey))
class VOXEL_API UVoxelHeightLayer : public UVoxelLayer
{
    GENERATED_BODY()

public:
    static UVoxelHeightLayer* Default();
};
```

**Use cases:**
- Ground surface
- Mountain ranges
- Heightmap-based terrain
- 2D noise landscapes

### UVoxelVolumeLayer

3D volumetric layer for SDF-based terrain.

```cpp
UCLASS(meta = (VoxelAssetType, AssetColor=Grey))
class VOXEL_API UVoxelVolumeLayer : public UVoxelLayer
{
    GENERATED_BODY()

public:
    static UVoxelVolumeLayer* Default();
};
```

**Use cases:**
- Caves and tunnels
- Overhangs
- 3D sculpted terrain
- Runtime digging/building

---

## Layer Reference System

VoxelPlugin uses typed layer references for type safety.

### FVoxelStackLayer (Generic Reference)

```cpp
USTRUCT(BlueprintType)
struct VOXEL_API FVoxelStackLayer
{
    GENERATED_BODY()

    // The layer stack containing the layer
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    TObjectPtr<UVoxelLayerStack> Stack;

    // Specific layer (null = use last layer in stack)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    TObjectPtr<UVoxelLayer> Layer;

public:
    bool IsValid() const;
    EVoxelLayerType GetType() const;
};
```

### FVoxelStackHeightLayer (Height Reference)

```cpp
USTRUCT(BlueprintType)
struct VOXEL_API FVoxelStackHeightLayer
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    TObjectPtr<UVoxelLayerStack> Stack;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    TObjectPtr<UVoxelHeightLayer> Layer;

public:
    operator FVoxelStackLayer() const;
    bool IsValid() const;
};
```

### FVoxelStackVolumeLayer (Volume Reference)

```cpp
USTRUCT(BlueprintType)
struct VOXEL_API FVoxelStackVolumeLayer
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    TObjectPtr<UVoxelLayerStack> Stack;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    TObjectPtr<UVoxelVolumeLayer> Layer;

public:
    operator FVoxelStackLayer() const;
    bool IsValid() const;
};
```

### Weak Layer References

For non-owning references (won't keep assets loaded):

```cpp
struct FVoxelWeakStackLayer
{
    EVoxelLayerType Type;
    TVoxelObjectPtr<UVoxelLayerStack> Stack;
    TVoxelObjectPtr<UVoxelLayer> Layer;

    FVoxelStackLayer Resolve() const;
};

struct FVoxelWeakStackHeightLayer : FVoxelWeakStackLayer
{
    FVoxelStackHeightLayer Resolve() const;
};

struct FVoxelWeakStackVolumeLayer : FVoxelWeakStackLayer
{
    FVoxelStackVolumeLayer Resolve() const;
};
```

---

## Setup Workflow

### Basic World Setup

1. **Create Layer Stack**
   - Content Browser > Voxel > Layer Stack
   - Add at least one height or volume layer

2. **Create Layers**
   - Content Browser > Voxel > Height Layer (for 2D terrain)
   - Content Browser > Voxel > Volume Layer (for 3D terrain)

3. **Place VoxelWorld**
   - Drag `AVoxelWorld` into level
   - Assign LayerStack in Details panel

4. **Configure World**
   ```
   VoxelSize: 100 (1m resolution)
   bEnableNanite: true
   bCreateRuntimeOnBeginPlay: true
   ```

### Multi-Layer Configuration

```
UVoxelLayerStack
├── HeightLayers
│   ├── [0] Base Terrain (noise-based ground)
│   └── [1] Roads Layer (spline-flattened areas)
└── VolumeLayers
    ├── [0] Caves Layer (underground volumes)
    └── [1] Sculpt Layer (player modifications)
```

---

## C++ Usage Examples

### Spawning VoxelWorld at Runtime

```cpp
void AMyGameMode::SpawnTerrain()
{
    // Load assets
    UVoxelLayerStack* Stack = LoadObject<UVoxelLayerStack>(
        nullptr, TEXT("/Game/Terrain/LS_MainTerrain"));

    // Spawn world
    FActorSpawnParameters Params;
    AVoxelWorld* World = GetWorld()->SpawnActor<AVoxelWorld>(
        AVoxelWorld::StaticClass(),
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        Params);

    // Configure
    World->LayerStack = Stack;
    World->VoxelSize = 100;
    World->bEnableNanite = true;

    // Create runtime (starts generation)
    World->CreateRuntime();
}
```

### Checking World State

```cpp
void AMyActor::CheckTerrain()
{
    AVoxelWorld* World = GetVoxelWorld();
    if (!World) return;

    if (World->IsVoxelWorldReady())
    {
        // Terrain fully generated, safe to query
        UE_LOG(LogTemp, Log, TEXT("Terrain ready!"));
    }
    else if (World->IsRuntimeCreated())
    {
        // Generation in progress
        UE_LOG(LogTemp, Log, TEXT("Terrain generating..."));
    }
    else
    {
        // Runtime not created yet
        UE_LOG(LogTemp, Log, TEXT("Terrain not started"));
    }
}
```

### Working with Layer References

```cpp
void AMyStamp::SetupLayer()
{
    // Create typed layer reference
    FVoxelStackVolumeLayer VolumeRef;
    VolumeRef.Stack = MyLayerStack;
    VolumeRef.Layer = MyVolumeLayer;

    // Check validity
    if (VolumeRef.IsValid())
    {
        // Convert to generic reference if needed
        FVoxelStackLayer GenericRef = VolumeRef;

        // Use in stamp
        MyStamp.Layer = VolumeRef;
    }
}
```

---

## Blueprint Usage

### Setting Up VoxelWorld

1. Place `AVoxelWorld` in level
2. In Details panel:
   - Set **Layer Stack** to your asset
   - Configure **Voxel Size** (100 = 1m)
   - Enable/disable **Nanite**

### Creating Layer Stack in Blueprint

```
Event BeginPlay
    │
    ├── Create Object (UVoxelLayerStack)
    │       └── Outer: Get Transient Package
    │
    ├── Add to HeightLayers array
    │       └── Default Height Layer reference
    │
    └── Set LayerStack on VoxelWorld
```

---

## Default Layer System

VoxelPlugin provides default layers via `UVoxelSettings`:

```cpp
// Project Settings > Plugins > Voxel
UPROPERTY(Config, EditAnywhere, Category = "Layers")
TSoftObjectPtr<UVoxelHeightLayer> DefaultHeightLayer;

UPROPERTY(Config, EditAnywhere, Category = "Layers")
TSoftObjectPtr<UVoxelVolumeLayer> DefaultVolumeLayer;
```

Access defaults:
```cpp
UVoxelHeightLayer* DefaultHeight = UVoxelHeightLayer::Default();
UVoxelVolumeLayer* DefaultVolume = UVoxelVolumeLayer::Default();
UVoxelLayerStack* DefaultStack = UVoxelLayerStack::Default();
```

---

## MaxDistance Parameter

The `MaxDistance` property on `UVoxelLayerStack` controls how far the distance field extends from height layer surfaces:

```
MaxDistance = 100000.f (1km)

        ▲ +MaxDistance
        │
~~~~~~~~│~~~~~~~~ Height Surface
        │
        ▼ -MaxDistance
```

**Guidelines:**
- Lower values = fewer chunks = better performance
- Must cover the deepest volumes you need
- Typical values: 50000 (500m) to 200000 (2km)

---

## Common Patterns

### Height-Only Terrain

```
LayerStack
└── HeightLayers
    └── [0] Procedural Height Layer
```

Simple 2D terrain without caves or overhangs.

### Volume-Only Terrain

```
LayerStack
└── VolumeLayers
    └── [0] Procedural Volume Layer
```

Full 3D terrain with caves, useful for Minecraft-like games.

### Hybrid Terrain

```
LayerStack
├── HeightLayers
│   └── [0] Base Ground
└── VolumeLayers
    └── [0] Caves & Sculpting
```

2D base terrain with 3D modifications on top.

---

## Performance Considerations

1. **Layer Count**: Each layer adds computation; minimize active layers
2. **MaxDistance**: Keep as low as practical
3. **VoxelSize**: Larger = faster but less detail
4. **Nanite**: Enable for high-detail terrain
5. **LODQuality**: Reduce for better performance

---

## Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| No terrain visible | LayerStack is null | Assign LayerStack in Details |
| Terrain doesn't generate | Runtime not created | Check `bCreateRuntimeOnBeginPlay` |
| Holes in terrain | MaxDistance too small | Increase MaxDistance |
| Poor performance | Too many layers | Consolidate layers |
| Collision missing | Wrong collision profile | Check collision settings |

---

## Related Topics

- [01-architecture.md](01-architecture.md) - Module overview
- [05-height-layer-system.md](05-height-layer-system.md) - Height layer details
- [06-volume-layer-system.md](06-volume-layer-system.md) - Volume layer details
- [07-stamp-system.md](07-stamp-system.md) - Adding stamps to layers
