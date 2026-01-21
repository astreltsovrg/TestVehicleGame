# Collision & Rendering System

## Summary

VoxelPlugin2 provides integrated collision and rendering systems optimized for procedural terrain. The collision system uses invoker-based chunk loading with Chaos physics support. Rendering leverages Nanite for high-detail terrain with automatic LOD streaming. Both systems are managed by subsystems that coordinate with the AVoxelWorld actor.

---

## Key Classes

### Collision Classes

| Class | Header | Purpose |
|-------|--------|---------|
| `UVoxelCollisionComponent` | VoxelCollisionComponent.h | Physics collision mesh component |
| `UVoxelStaticMeshCollisionComponent` | VoxelStaticMeshCollisionComponent.h | Static mesh collision variant |
| `UVoxelCollisionInvokerComponent` | VoxelCollisionInvoker.h | Triggers collision chunk loading |
| `FVoxelCollisionInvokerManager` | VoxelCollisionInvoker.h | Manages all collision invokers |
| `AVoxelCollisionBaker` | VoxelCollisionBaker.h | Bakes collision for static areas |
| `FVoxelCollider` | (internal) | Collision mesh data structure |

### Rendering Classes

| Class | Header | Purpose |
|-------|--------|---------|
| `UVoxelNaniteComponent` | VoxelNaniteComponent.h | Nanite mesh rendering component |
| `FVoxelRenderSubsystem` | VoxelRenderSubsystem.h | Manages render chunks and LOD |
| `FVoxelRenderChunk` | VoxelRenderChunk.h | Individual render chunk data |
| `FVoxelNaniteMesh` | (internal) | Nanite mesh data |
| `FVoxelMeshRenderProxy` | (internal) | Mesh rendering proxy |

---

## Collision System Architecture

```
┌────────────────────────────────────────────────────────────────────┐
│                        AVoxelWorld                                  │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                 FVoxelCollisionInvokerManager                │   │
│  │     ┌─────────────────────────────────────────────────┐     │   │
│  │     │           UVoxelCollisionInvokerComponent       │     │   │
│  │     │                 (on Player Pawn)                │     │   │
│  │     │              Radius: 1000 cm                    │     │   │
│  │     └─────────────────────────────────────────────────┘     │   │
│  │                            │                                 │   │
│  │                            ▼                                 │   │
│  │     ┌─────────────────────────────────────────────────┐     │   │
│  │     │           FVoxelCollisionInvokerView            │     │   │
│  │     │         GetChunks() → Set<FIntVector>           │     │   │
│  │     └─────────────────────────────────────────────────┘     │   │
│  │                            │                                 │   │
│  │                            ▼                                 │   │
│  │     ┌─────────────────────────────────────────────────┐     │   │
│  │     │         UVoxelCollisionComponent (per chunk)    │     │   │
│  │     │          BodySetup + FVoxelCollider             │     │   │
│  │     └─────────────────────────────────────────────────┘     │   │
│  └─────────────────────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────────────────────┘
```

---

## UVoxelCollisionInvokerComponent

Component that triggers collision chunk generation around its location.

```cpp
UCLASS(ClassGroup = Voxel, meta = (BlueprintSpawnableComponent))
class VOXEL_API UVoxelCollisionInvokerComponent : public UPrimitiveComponent
{
    GENERATED_BODY()

public:
    // Enable/disable collision loading
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invoker")
    bool bEnabled = true;

    // Radius for collision chunk loading (world space, ignores scale)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invoker")
    float Radius = 1000.f;

    // Block until collision is ready (prevents falling through terrain)
    // May cause hitches but ensures valid collision
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Invoker")
    bool bWaitForVoxelWorld = true;
};
```

### Invoker Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `bEnabled` | `bool` | true | Enable collision invoker |
| `Radius` | `float` | 1000 | Collision loading radius (cm) |
| `bWaitForVoxelWorld` | `bool` | true | Block on missing collision |

### Typical Setup

Add `UVoxelCollisionInvokerComponent` to:
- Player pawns
- Vehicles
- Important AI characters
- Any actor that needs collision

```cpp
// In your pawn's constructor
CollisionInvoker = CreateDefaultSubobject<UVoxelCollisionInvokerComponent>(TEXT("VoxelCollisionInvoker"));
CollisionInvoker->Radius = 2000.f;  // 20m radius
CollisionInvoker->bWaitForVoxelWorld = true;
```

---

## UVoxelCollisionComponent

Physics collision component generated for each terrain chunk.

```cpp
UCLASS()
class VOXEL_API UVoxelCollisionComponent : public UPrimitiveComponent
{
    GENERATED_BODY()

public:
    // Get the collision mesh data
    TSharedPtr<const FVoxelCollider> GetCollider() const;

    // Set new collision data
    void SetCollider(
        const TSharedRef<const FVoxelCollider>& NewCollider,
        const FBodyInstance& NewBodyInstance,
        bool bDoubleSidedGeometry,
        const FTransform& RelativeTransform);

    // Remove collision
    void ClearCollider();

    // UPrimitiveComponent interface
    virtual UBodySetup* GetBodySetup() override;
    virtual bool ShouldCreatePhysicsState() const override;
};
```

---

## AVoxelCollisionBaker

Actor for baking collision in static areas (editor-time or PIE).

```cpp
UCLASS()
class VOXEL_API AVoxelCollisionBaker : public AActor
{
    GENERATED_BODY()

public:
    // Bake radius
    UPROPERTY(EditAnywhere, Category = "Config")
    float Radius = 1000.f;

    // Voxel resolution
    UPROPERTY(EditAnywhere, Category = "Config")
    int32 VoxelSize = 100;

    // Collision chunk size
    UPROPERTY(EditAnywhere, Category = "Config")
    int32 ChunkSize = 32;

    // Target layer
    UPROPERTY(EditAnywhere, Category = "Config")
    FVoxelStackLayer Layer;

    // Physics properties
    UPROPERTY(EditAnywhere, Category = "Config")
    FBodyInstance Collision;

    // Double-sided collision
    UPROPERTY(EditAnywhere, Category = "Config")
    bool bDoubleSidedCollision = false;

    // Trigger bake in editor
    UPROPERTY(EditAnywhere, Category = "Config", Transient)
    bool bGenerate = false;
};
```

### Baking Usage

1. Place `AVoxelCollisionBaker` in level
2. Configure radius and layer
3. Set `bGenerate = true` in Details panel
4. Collision is baked and serialized with level

---

## FVoxelCollisionInvokerManager

World subsystem managing all collision invokers.

```cpp
class VOXEL_API FVoxelCollisionInvokerManager : public IVoxelWorldSubsystem
{
public:
    // Debug logging
    void LogInvokers();

    // Create new invoker view
    TSharedRef<FVoxelCollisionInvokerView> MakeView(
        int32 ChunkSize,
        const FTransform& LocalToWorld);

    // IVoxelWorldSubsystem interface
    virtual void Tick() override;
    virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
};
```

---

## Collision Configuration in VoxelWorld

AVoxelWorld exposes collision settings:

```cpp
// From VoxelWorld.h (see 04-voxelworld-layerstack.md)

// Collision chunk size in voxels
UPROPERTY(EditAnywhere, Category = "Collision")
int32 CollisionChunkSize = 32;

// Enable double-sided collision meshes
UPROPERTY(EditAnywhere, Category = "Collision")
bool bDoubleSidedCollision = false;

// Collision profile for visibility queries
UPROPERTY(EditAnywhere, Category = "Collision")
FCollisionProfileName VisibilityCollision = FName("BlockAll");

// Collision profile for invoker queries
UPROPERTY(EditAnywhere, Category = "Collision")
FCollisionProfileName InvokerCollision = FName("BlockAll");
```

### Collision Settings Reference

| Setting | Default | Description |
|---------|---------|-------------|
| `CollisionChunkSize` | 32 | Collision mesh resolution |
| `bDoubleSidedCollision` | false | Two-sided collision |
| `VisibilityCollision` | BlockAll | Profile for line traces |
| `InvokerCollision` | BlockAll | Profile for invoker overlap |

---

## Rendering System Architecture

```
┌────────────────────────────────────────────────────────────────────┐
│                        AVoxelWorld                                  │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                   FVoxelRenderSubsystem                      │   │
│  │                                                              │   │
│  │     ┌────────────────────────────────────────────────┐      │   │
│  │     │              FVoxelRenderTree                  │      │   │
│  │     │         (Octree of render chunks)              │      │   │
│  │     │                    │                           │      │   │
│  │     │      ┌─────────────┴─────────────┐            │      │   │
│  │     │      │                           │            │      │   │
│  │     │      ▼                           ▼            │      │   │
│  │     │ FVoxelRenderChunk      FVoxelRenderChunk      │      │   │
│  │     │   (LOD 0)                  (LOD 1)            │      │   │
│  │     └────────────────────────────────────────────────┘      │   │
│  │                            │                                 │   │
│  │                            ▼                                 │   │
│  │     ┌────────────────────────────────────────────────┐      │   │
│  │     │            FVoxelRenderChunkData               │      │   │
│  │     │  ┌─────────────┐  ┌─────────────────────────┐ │      │   │
│  │     │  │ Nanite Mesh │  │ Collision Component     │ │      │   │
│  │     │  │ (rendering) │  │ (physics)               │ │      │   │
│  │     │  └─────────────┘  └─────────────────────────┘ │      │   │
│  │     └────────────────────────────────────────────────┘      │   │
│  └─────────────────────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────────────────────┘
```

---

## UVoxelNaniteComponent

Component rendering terrain via Nanite.

```cpp
UCLASS(NotBlueprintable)
class VOXEL_API UVoxelNaniteComponent : public UStaticMeshComponent
{
    GENERATED_BODY()

public:
    // Get current Nanite mesh
    const TSharedPtr<const FVoxelNaniteMesh>& GetMesh() const;

    // Set new Nanite mesh data
    void SetMesh(
        const TSharedRef<const FVoxelNaniteMesh>& NewMesh,
        const FVoxelConfig& Config,
        const FVoxelNaniteMaterialRenderer& MaterialRenderer);

    // Clear mesh
    void ClearMesh();

    // Set material for Nanite rendering
    void SetNaniteMaterial(UMaterialInterface* Material);

    // UStaticMeshComponent interface
    virtual FPrimitiveSceneProxy* CreateStaticMeshSceneProxy(
        Nanite::FMaterialAudit& NaniteMaterials,
        bool bCreateNanite) override;
};
```

---

## FVoxelRenderSubsystem

Central subsystem coordinating all rendering.

```cpp
USTRUCT()
struct VOXEL_API FVoxelRenderSubsystem : public FVoxelSubsystem
{
    GENERATED_BODY()

public:
    // Check if chunks need subdivision
    bool HasChunksToSubdivide() const;

    // Get current generation bounds
    const FVoxelOptionalBox& GetBoundsToGenerate() const;

    // Access texture manager
    const FVoxelTextureManager& GetTextureManager() const;

    // Access Nanite material renderer
    FVoxelNaniteMaterialRenderer& GetNaniteMaterialRenderer() const;

    // Get material instance for target
    TSharedRef<FVoxelMaterialInstanceRef> GetMaterialInstanceRef(
        EVoxelMegaMaterialTarget Target) const;

    // FVoxelSubsystem interface
    virtual void Initialize() override;
    virtual void Compute() override;
    virtual void Render(FVoxelRuntime& Runtime) override;
};
```

---

## FVoxelRenderChunk

Individual chunk in the render tree.

```cpp
struct FVoxelRenderChunkData
{
    const FVoxelChunkKey ChunkKey;
    const FVoxelChunkNeighborInfo NeighborInfo;

    // Data pointers
    TSharedPtr<FVoxelCollider> Collider;
    TSharedPtr<FVoxelNaniteMesh> NaniteMesh;
    TSharedPtr<FVoxelMeshRenderProxy> RenderProxy;

    // Component references
    TVoxelObjectPtr<UVoxelMeshComponent> MeshComponent;
    TVoxelObjectPtr<UVoxelNaniteComponent> NaniteComponent;
    TVoxelObjectPtr<UVoxelCollisionComponent> CollisionComponent;
    TVoxelObjectPtr<UVoxelStaticMeshCollisionComponent> StaticMeshCollisionComponent;
};

class VOXEL_API FVoxelRenderChunk
{
public:
    const FVoxelChunkKey ChunkKey;

    // Child chunks for LOD subdivision
    TVoxelArray<TSharedPtr<FVoxelRenderChunk>> Children;
    TVoxelArray<TSharedPtr<FVoxelRenderChunk>> ChildrenToDestroy;

    // Mesh data
    TVoxelOptional<TSharedPtr<FVoxelMesh>> Mesh;
    TSharedPtr<FVoxelDependencyTracker> MeshDependencyTracker;
    bool bMeshInvalidated = false;

    // Render data
    TSharedPtr<FVoxelRenderChunkData> RenderData;
};
```

---

## Nanite Configuration in VoxelWorld

```cpp
// From VoxelWorld.h

// Enable Nanite rendering
UPROPERTY(EditAnywhere, Category = "Nanite")
bool bEnableNanite = true;

// Enable Nanite tessellation
UPROPERTY(EditAnywhere, Category = "Nanite")
bool bEnableTessellation = true;

// Maximum LOD level for tessellation
UPROPERTY(EditAnywhere, Category = "Nanite", meta = (ClampMin = 0))
int32 NaniteMaxTessellationLOD = 2;

// Position encoding precision (bits)
UPROPERTY(EditAnywhere, Category = "Nanite", meta = (ClampMin = 0))
int32 NanitePositionPrecision = 6;
```

### Nanite Settings Reference

| Setting | Default | Description |
|---------|---------|-------------|
| `bEnableNanite` | true | Use Nanite for terrain |
| `bEnableTessellation` | true | Enable Nanite tessellation |
| `NaniteMaxTessellationLOD` | 2 | Max LOD for tessellation |
| `NanitePositionPrecision` | 6 | Position bit precision |

---

## Rendering Settings in VoxelWorld

```cpp
// Lumen Global Illumination support
UPROPERTY(EditAnywhere, Category = "Rendering")
bool bEnableLumen = false;

// Raytracing support
UPROPERTY(EditAnywhere, Category = "Rendering")
bool bEnableRaytracing = false;

// Generate mesh distance fields
UPROPERTY(EditAnywhere, Category = "Rendering")
bool bGenerateMeshDistanceFields = false;

// Render chunk size
UPROPERTY(EditAnywhere, Category = "Rendering")
EVoxelRenderChunkSize RenderChunkSize = EVoxelRenderChunkSize::Size32;
```

### EVoxelRenderChunkSize

```cpp
UENUM()
enum class EVoxelRenderChunkSize : uint8
{
    Size16,   // 16 voxels per chunk
    Size32,   // 32 voxels per chunk (default)
    Size64,   // 64 voxels per chunk
    Size128   // 128 voxels per chunk
};
```

---

## LOD System

VoxelPlugin uses an octree-based LOD system:

```
Distance from Camera    LOD Level    Detail
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Near (0-100m)           LOD 0        Full detail
Mid (100-500m)          LOD 1        Half detail
Far (500m+)             LOD 2+       Reduced detail
```

### LOD Configuration

```cpp
// LOD quality settings
UPROPERTY(EditAnywhere, Category = "LOD")
FVoxelLODQuality LODQuality;

// Quality exponent (affects LOD transitions)
UPROPERTY(EditAnywhere, Category = "LOD")
double QualityExponent = 1.0;
```

---

## C++ Usage Examples

### Adding Collision Invoker to Pawn

```cpp
// MyVehiclePawn.h
UCLASS()
class AMyVehiclePawn : public AWheeledVehiclePawn
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel")
    UVoxelCollisionInvokerComponent* VoxelCollisionInvoker;

    AMyVehiclePawn();
};

// MyVehiclePawn.cpp
AMyVehiclePawn::AMyVehiclePawn()
{
    VoxelCollisionInvoker = CreateDefaultSubobject<UVoxelCollisionInvokerComponent>(
        TEXT("VoxelCollisionInvoker"));
    VoxelCollisionInvoker->SetupAttachment(RootComponent);
    VoxelCollisionInvoker->Radius = 3000.f;  // 30m collision radius
    VoxelCollisionInvoker->bWaitForVoxelWorld = true;
}
```

### Configuring VoxelWorld for Performance

```cpp
void AMyGameMode::ConfigureVoxelWorld(AVoxelWorld* World)
{
    // Nanite settings
    World->bEnableNanite = true;
    World->bEnableTessellation = true;
    World->NaniteMaxTessellationLOD = 1;  // Reduce for better performance

    // Collision settings
    World->CollisionChunkSize = 32;
    World->bDoubleSidedCollision = false;

    // Rendering settings
    World->bEnableLumen = false;  // Disable Lumen for performance
    World->bEnableRaytracing = false;
    World->RenderChunkSize = EVoxelRenderChunkSize::Size32;

    // LOD settings
    World->QualityExponent = 0.8;  // More aggressive LOD
}
```

### Baking Collision at Runtime

```cpp
void AMyActor::BakeCollisionInArea(FVector Location, float Radius)
{
    // Spawn collision baker
    FActorSpawnParameters Params;
    AVoxelCollisionBaker* Baker = GetWorld()->SpawnActor<AVoxelCollisionBaker>(
        AVoxelCollisionBaker::StaticClass(),
        Location,
        FRotator::ZeroRotator,
        Params);

    // Configure
    Baker->Radius = Radius;
    Baker->VoxelSize = 100;
    Baker->ChunkSize = 32;
    Baker->Layer = MyVoxelLayer;
    Baker->bDoubleSidedCollision = false;

    // Trigger bake
    Baker->bGenerate = true;
}
```

---

## Blueprint Usage

### Setting Up Collision Invoker

1. **Add Component to Pawn**
   - Open your pawn Blueprint
   - Add Component > Voxel Collision Invoker
   - Configure in Details panel:
     ```
     bEnabled: true
     Radius: 2000 (20m)
     bWaitForVoxelWorld: true
     ```

2. **Attach to Root**
   - Drag to attach under root component
   - Component follows pawn automatically

### Configuring VoxelWorld Rendering

1. **Select VoxelWorld Actor**
2. **Configure Nanite (Details panel)**
   ```
   bEnableNanite: true
   bEnableTessellation: true
   NaniteMaxTessellationLOD: 2
   NanitePositionPrecision: 6
   ```

3. **Configure Collision**
   ```
   CollisionChunkSize: 32
   bDoubleSidedCollision: false
   VisibilityCollision: BlockAll
   InvokerCollision: BlockAll
   ```

4. **Configure LOD**
   ```
   QualityExponent: 1.0 (lower = more aggressive LOD)
   RenderChunkSize: Size32
   ```

---

## Performance Guidelines

### Collision Optimization

| Factor | Impact | Recommendation |
|--------|--------|----------------|
| Invoker Radius | High | Use minimum needed (1000-3000) |
| CollisionChunkSize | Medium | 32 is balanced |
| bDoubleSidedCollision | Low | Disable unless needed |
| bWaitForVoxelWorld | Hitch vs safety | Enable for important pawns |

### Rendering Optimization

| Factor | Impact | Recommendation |
|--------|--------|----------------|
| bEnableNanite | Critical | Enable for quality terrain |
| bEnableTessellation | Medium | Disable for max performance |
| bEnableLumen | High | Disable unless needed |
| bEnableRaytracing | High | Disable unless needed |
| QualityExponent | Medium | Lower values = faster LOD |
| RenderChunkSize | Low | Larger = fewer chunks |

### Memory Considerations

| Component | Typical Memory | Notes |
|-----------|----------------|-------|
| Render chunk (LOD 0) | 1-5 MB | Depends on complexity |
| Collision chunk | 0.5-2 MB | Per active chunk |
| Nanite mesh | 2-10 MB | Compressed GPU memory |

---

## Collision Channels

VoxelPlugin uses a dedicated collision channel:

```cpp
// VoxelCollisionChannels.h
#define ECC_VoxelEditor ECC_EngineTraceChannel6
```

### Default Collision Profiles

| Profile | Purpose | Blocks |
|---------|---------|--------|
| `BlockAll` | Default terrain | All channels |
| `OverlapAll` | Overlap queries | None |
| `VoxelEditor` | Editor interaction | Editor tools |

---

## Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| Falling through terrain | Missing collision invoker | Add UVoxelCollisionInvokerComponent |
| Collision hitches | bWaitForVoxelWorld = true | Increase invoker radius |
| Low FPS | Nanite tessellation | Reduce NaniteMaxTessellationLOD |
| Memory issues | Too many chunks | Reduce LODQuality or invoker radius |
| Visual artifacts | LOD transitions | Increase QualityExponent |
| No Lumen GI | bEnableLumen = false | Enable if needed (performance cost) |
| Raytracing issues | bEnableRaytracing = false | Enable for RT effects |

---

## Chaos Physics Integration

VoxelPlugin collision works with Chaos physics:

1. **Collision meshes** are generated per chunk
2. **BodySetup** created for Chaos physics state
3. **FBodyInstance** configures physical properties
4. **Collision shapes** support complex terrain geometry

### NotifyHit Support

Voxel collision components support physics callbacks:

```cpp
// On your pawn
virtual void NotifyHit(
    UPrimitiveComponent* MyComp,
    AActor* Other,
    UPrimitiveComponent* OtherComp,
    bool bSelfMoved,
    FVector HitLocation,
    FVector HitNormal,
    FVector NormalImpulse,
    const FHitResult& Hit) override;
```

---

## Related Topics

- [04-voxelworld-layerstack.md](04-voxelworld-layerstack.md) - VoxelWorld configuration
- [14-query-system.md](14-query-system.md) - Terrain queries
- [16-megamaterial-system.md](16-megamaterial-system.md) - Material rendering
