# Stamp System

## Summary

The stamp system is the core mechanism for modifying voxel terrain. Stamps are configuration objects (`FVoxelStamp`) that define terrain modifications like adding hills, carving caves, or painting surfaces. They're managed through components (`UVoxelStampComponent`) and processed at runtime via `FVoxelStampRuntime`. This document covers stamp architecture, lifecycle, and registration.

---

## Key Classes

| Class | Header | Purpose |
|-------|--------|---------|
| `FVoxelStamp` | VoxelStamp.h | Base stamp configuration struct |
| `FVoxelStampRuntime` | VoxelStampRuntime.h | Runtime execution of stamp |
| `FVoxelStampRef` | VoxelStampRef.h | Serializable reference to stamp |
| `FVoxelInstancedStampRef` | VoxelStampRef.h | Deep-copy stamp reference |
| `UVoxelStampComponent` | VoxelStampComponent.h | Single stamp holder component |
| `UVoxelInstancedStampComponent` | VoxelInstancedStampComponent.h | Multiple stamps holder |
| `AVoxelStampActor` | VoxelStampActor.h | Convenience actor with stamp component |
| `FVoxelStampManager` | VoxelStampManager.h | World subsystem for stamp registration |
| `EVoxelStampBehavior` | VoxelStampBehavior.h | Stamp behavior flags |

---

## FVoxelStamp (Base Class)

The foundation struct for all stamp types.

```cpp
USTRUCT(meta = (Abstract))
struct VOXEL_API FVoxelStamp : public FVoxelVirtualStruct
{
    GENERATED_BODY()

public:
    // Transform relative to component/actor
    UPROPERTY(EditAnywhere, Category = "Config")
    FTransform Transform;

    // What aspects of terrain this stamp affects
    UPROPERTY(EditAnywhere, Category = "Config")
    EVoxelStampBehavior Behavior = EVoxelStampBehavior::AffectAll;

    // Priority within layer (higher = applied later)
    UPROPERTY(EditAnywhere, Category = "Config")
    int32 Priority = 0;

    // Edge smoothness in cm (smooth blending with other stamps)
    UPROPERTY(EditAnywhere, Category = "Config", meta = (Units = cm, ClampMin = 0))
    float Smoothness = 100;

    // Metadata overrides for this stamp
    UPROPERTY(EditAnywhere, Category = "Config")
    FVoxelMetadataOverrides MetadataOverrides;

    // Random seed for deterministic generation
    UPROPERTY(EditAnywhere, Category = "Config")
    FVoxelExposedSeed StampSeed;

    // LOD range this stamp applies to (inclusive)
    UPROPERTY(EditAnywhere, Category = "Config", DisplayName = "LOD Range", AdvancedDisplay)
    FInt32Interval LODRange = { 0, 32 };

    // Disable click selection in editor
    UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay)
    bool bDisableStampSelection = false;

    // Apply on void areas (no previous stamps)
    UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay)
    bool bApplyOnVoid = true;

    // Bounds extension multiplier for smoothness
    UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay, meta = (UIMin = 0, UIMax = 5))
    float BoundsExtension = 1.f;

    // Exclude from priority increment calculations
    UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay)
    bool bExcludeFromPriorityIncrements = false;

public:
    // Get serializable reference
    FVoxelStampRef GetStampRef() const;

    // Get owning component
    TVoxelObjectPtr<USceneComponent> GetComponent() const;

    // Resolve to runtime stamp
    TSharedPtr<const FVoxelStampRuntime> ResolveStampRuntime() const;
};
```

### Inherited Properties

All stamp types (Height, Volume) inherit these base properties. The `Transform` property is applied automatically during stamp processing.

---

## EVoxelStampBehavior

Flags controlling what aspects of terrain a stamp affects.

```cpp
UENUM(BlueprintType, meta = (Bitflags))
enum class EVoxelStampBehavior : uint8
{
    None = 0,

    // Affects terrain shape (distance field)
    AffectShape = 1 << 0,

    // Affects surface material
    AffectSurfaceType = 1 << 1,

    // Affects metadata
    AffectMetadata = 1 << 2,

    // Combined flags
    AffectAll = AffectShape | AffectSurfaceType | AffectMetadata,
    AffectShapeAndSurfaceType = AffectShape | AffectSurfaceType,
    AffectShapeAndMetadata = AffectShape | AffectMetadata,
    AffectSurfaceTypeAndMetadata = AffectSurfaceType | AffectMetadata,
};
```

### Behavior Use Cases

| Behavior | Use Case |
|----------|----------|
| `AffectAll` | Standard terrain modification |
| `AffectShape` only | Shape-only stamp (no material change) |
| `AffectSurfaceType` only | Paint-only stamp (no shape change) |
| `AffectMetadata` only | Metadata-only (grass density, etc.) |

---

## FVoxelStampRuntime

Runtime representation of a stamp, created from `FVoxelStamp` during registration.

```cpp
USTRUCT()
struct VOXEL_API FVoxelStampRuntime : public FVoxelVirtualStruct
{
    GENERATED_BODY()

public:
    // Get the source stamp configuration
    FORCEINLINE const FVoxelStamp& GetStamp() const;

    // Initialization on game thread
    virtual bool Initialize(FVoxelDependencyCollector& DependencyCollector);

    // Parallel initialization (for expensive operations)
    virtual bool Initialize_Parallel(FVoxelDependencyCollector& DependencyCollector);

    // Get stamp bounds in local space
    virtual FVoxelBox GetLocalBounds() const;

    // Check if full invalidation needed
    virtual bool ShouldFullyInvalidate(
        const FVoxelStampRuntime& PreviousRuntime,
        TVoxelArray<FVoxelBox>& OutLocalBoundsToInvalidate) const;

    // Check if stamp needs previous query state
    virtual bool ShouldUseQueryPrevious() const;

public:
    // Utility accessors
    bool FailedToInitialize() const;
    TVoxelObjectPtr<UWorld> GetWorld() const;
    TVoxelObjectPtr<USceneComponent> GetComponent() const;
    TConstVoxelArrayView<TVoxelObjectPtr<UVoxelLayer>> GetLayers() const;
    FInt32Interval GetLODRange() const;
    bool ShouldComputeLOD(int32 LOD) const;
    const FTransform& GetLocalToWorld() const;

public:
    // Behavior checks
    bool AffectShape() const;
    bool AffectSurfaceType() const;
    bool AffectMetadata() const;

    // Request regeneration
    void RequestUpdate() const;
};
```

### Static Creation

```cpp
// Bulk create multiple runtimes (efficient)
static void BulkCreate(
    TVoxelObjectPtr<UWorld> World,
    TVoxelArrayView<FStampInfo> StampInfos);

// Create single runtime
static TSharedPtr<FVoxelStampRuntime> Create(
    TVoxelObjectPtr<UWorld> World,
    const FVoxelStampRef& StampRef,
    TVoxelObjectPtr<USceneComponent> Component);
```

---

## Stamp Lifecycle

```
┌────────────────────────────────────────────────────────────────────────────┐
│                           STAMP LIFECYCLE                                   │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   1. CREATION                                                                │
│   ┌──────────────────┐                                                      │
│   │  FVoxelStamp     │  Created via FVoxelStampRef::New() or component     │
│   │  (Configuration) │  Contains: Transform, Priority, Smoothness, etc.    │
│   └────────┬─────────┘                                                      │
│            │                                                                 │
│   2. REGISTRATION                                                            │
│   ┌────────▼─────────┐                                                      │
│   │ UVoxelStamp      │  Component calls FVoxelStampRef::Register()         │
│   │ Component        │  → FVoxelStampManager::RegisterStamps()             │
│   └────────┬─────────┘                                                      │
│            │                                                                 │
│   3. RUNTIME CREATION                                                        │
│   ┌────────▼─────────┐                                                      │
│   │ FVoxelStamp      │  Manager creates runtime via BulkCreate()           │
│   │ Runtime          │  → Initialize() on game thread                       │
│   │                  │  → Initialize_Parallel() in background               │
│   └────────┬─────────┘                                                      │
│            │                                                                 │
│   4. LAYER ASSIGNMENT                                                        │
│   ┌────────▼─────────┐                                                      │
│   │ FVoxelStampLayer │  Runtime added to appropriate layer manager         │
│   │ Manager          │  → OnStampChanged broadcast                          │
│   └────────┬─────────┘                                                      │
│            │                                                                 │
│   5. TERRAIN GENERATION                                                      │
│   ┌────────▼─────────┐                                                      │
│   │  Stamp Query     │  During chunk generation:                            │
│   │                  │  → GetLocalBounds() to check overlap                 │
│   │                  │  → Apply() for height/volume queries                 │
│   └────────┬─────────┘                                                      │
│            │                                                                 │
│   6. UPDATE/UNREGISTER                                                       │
│   ┌────────▼─────────┐                                                      │
│   │  Update/Remove   │  FVoxelStampRef::Update() or Unregister()           │
│   │                  │  → Invalidates affected chunks                       │
│   └──────────────────┘                                                      │
│                                                                              │
└────────────────────────────────────────────────────────────────────────────┘
```

---

## FVoxelStampRef

Serializable smart reference to a stamp. Handles persistence and copy semantics.

```cpp
USTRUCT(BlueprintType, DisplayName = "Voxel Stamp")
struct VOXEL_API FVoxelStampRef
{
    GENERATED_BODY()

public:
    // Create new stamp reference
    static FVoxelStampRef New(const FVoxelStamp& StampToCopyFrom);

public:
    // Validity check
    bool IsValid() const;

    // Get stamp struct type
    UScriptStruct* GetStruct() const;

    // Type checking
    bool IsA(const UScriptStruct* BaseStruct) const;
    template<typename Type> bool IsA() const;

    // Access stamp data
    FVoxelStamp* operator->() const;
    FVoxelStamp& operator*() const;

    // Convert to shared pointer
    template<typename Type = FVoxelStamp>
    TSharedPtr<Type> ToSharedPtr() const;

    // Make independent copy
    FVoxelStampRef MakeCopy() const;

    // Resolve to runtime
    TSharedPtr<const FVoxelStampRuntime> ResolveStampRuntime() const;

public:
    // Registration
    bool IsRegistered() const;
    void Register(USceneComponent& Component) const;
    void Unregister() const;
    void Update() const;

    // Bulk operations
    static void BulkRegister(
        TConstVoxelArrayView<FVoxelStampRef> StampRefs,
        USceneComponent& Component);
    static void BulkUnregister(TConstVoxelArrayView<FVoxelStampRef> StampRefs);

public:
    // Serialization support
    bool Serialize(FArchive& Ar);
    bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};
```

### FVoxelInstancedStampRef

Deep-copy variant for instanced stamps (each copy is independent).

```cpp
USTRUCT()
struct VOXEL_API FVoxelInstancedStampRef : public FVoxelStampRef
{
    GENERATED_BODY()

    // Copy constructor creates deep copy
    FVoxelInstancedStampRef(const FVoxelInstancedStampRef& Other);
    FVoxelInstancedStampRef& operator=(const FVoxelInstancedStampRef& Other);
};
```

---

## UVoxelStampComponent

Component that holds a single stamp.

```cpp
UCLASS(BlueprintType, ClassGroup = Voxel, meta = (BlueprintSpawnableComponent))
class VOXEL_API UVoxelStampComponent : public UVoxelStampComponentBase
{
    GENERATED_BODY()

public:
    // Get current stamp
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    FVoxelStampRef GetStamp() const;

    // Set new stamp (returns old stamp)
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    FVoxelStampRef SetStamp(const FVoxelStampRef& NewStamp);

    // Set stamp from struct
    void SetStamp(const FVoxelStamp& NewStamp);

public:
    // Get new stamp priority for world
    static int32 GetNewStampPriority(const UWorld* World, const FVoxelStampRef& TargetStamp);
    static int32 GetNewStampPriority(const UWorld* World, const FVoxelStamp& TargetStamp);
};
```

---

## UVoxelInstancedStampComponent

Component that holds multiple stamps (for efficient batching).

```cpp
UCLASS(ClassGroup = Voxel, meta = (BlueprintSpawnableComponent))
class VOXEL_API UVoxelInstancedStampComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    // Update all stamps
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void UpdateAllStamps();

    // Update single stamp by index
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void UpdateStamp(int32 Index);

    // Update multiple stamps
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void UpdateStamps(const TArray<int32>& IndicesToUpdate);

    // Add new stamp (returns index)
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    int32 AddStamp(const FVoxelStampRef& NewStamp);

    // Get stamp by index
    UFUNCTION(BlueprintPure, Category = "Voxel")
    FVoxelStampRef GetStamp(int32 Index);

    // Set stamp at index (returns old stamp)
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    FVoxelStampRef SetStamp(int32 Index, const FVoxelStampRef& NewStamp);

    // Remove stamp (clears, keeps index)
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void RemoveStamp(int32 Index);

    // Clear all stamps
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void ClearStamps();

    // Get stamp count
    UFUNCTION(BlueprintCallable, Category = "Voxel")
    int32 NumStamps() const;

public:
    // Efficient bulk operations
    void Reserve(int32 NewNumStamps);
    void AddStamps_NoCopy(TVoxelArray<FVoxelStampRef>&& NewStamps);
    int32 FindStampIndex(const FVoxelStampRef& StampRef) const;
};
```

---

## AVoxelStampActor

Convenience actor with built-in stamp component.

```cpp
UCLASS(NotBlueprintable)
class VOXEL_API AVoxelStampActor : public AActor
{
    GENERATED_BODY()

public:
    // Get the stamp component
    UVoxelStampComponent& GetStampComponent() const;

    // Auto-update actor label from stamp
    UPROPERTY(EditAnywhere, Category = "Misc")
    bool bAutoUpdateLabel = true;

    UPROPERTY(EditAnywhere, Category = "Misc", meta = (EditCondition = "bAutoUpdateLabel"))
    FString LabelPrefix;

public:
    // Blueprint accessors
    UFUNCTION(BlueprintPure, Category = "Voxel")
    FVoxelStampRef GetStamp() const;

    UFUNCTION(BlueprintCallable, Category = "Voxel")
    FVoxelStampRef SetStamp(const FVoxelStampRef& NewStamp);

    UFUNCTION(BlueprintCallable, Category = "Voxel")
    void UpdateStamp();
};
```

---

## FVoxelStampManager

World subsystem managing stamp registration and layer assignment.

```cpp
class VOXEL_API FVoxelStampManager : public IVoxelWorldSubsystem
{
public:
    // Register stamps with component
    void RegisterStamps(
        TConstVoxelArrayView<FVoxelStampRef> StampRefs,
        TVoxelArrayView<FVoxelStampIndex> OutStampIndices,
        USceneComponent& Component);

    // Unregister stamps
    void UnregisterStamps(TConstVoxelArrayView<FVoxelStampIndex> StampIndices);

    // Update existing stamp
    void UpdateStamp(
        const FVoxelStampIndex& StampIndex,
        const FVoxelStampRef& StampRef,
        USceneComponent& Component);

    // Resolve stamp runtime
    TSharedPtr<const FVoxelStampRuntime> ResolveStampRuntime(const FVoxelStampIndex& StampIndex);

public:
    // Change notification
    FSimpleMulticastDelegate OnChanged;

    // Force pending updates
    void FlushUpdates();

    // Get/create layer manager
    TSharedRef<FVoxelStampLayerManager> FindOrAddLayer(TVoxelObjectPtr<UVoxelLayer> Layer);
};
```

---

## Stamp Transforms

### FVoxelHeightTransform (2D)

Used for height layer stamps.

```cpp
struct VOXEL_API FVoxelHeightTransform
{
    FVector2f Scale = FVector2f(1.f);
    FVector2f Rotation = FVector2f(1.f, 0.f);  // cos, sin
    FVector2d Position = FVector2d(0.f);

    FVector2f Rotation3D = FVector2f(0.f);  // tilt
    float ScaleZ = 1.f;
    float OffsetZ = 0.f;

    float MinHeight = -MAX_flt;
    float MaxHeight = MAX_flt;

    // Transform 2D position
    FVector2d Transform(const FVector2d& Point) const;
    FVector2d InverseTransform(const FVector2d& Point) const;

    // Transform height value
    template<typename T>
    T TransformHeight(T Height, const FVector2D& LocalPosition) const;
};
```

### FVoxelVolumeTransform (3D)

Used for volume layer stamps.

```cpp
struct VOXEL_API FVoxelVolumeTransform
{
    FVector3f Scale = FVector3f(1.f);
    FQuat4f Rotation = FQuat4f::Identity;
    FVector3d Position = FVector3d(0.f);

    float MaxDistance = Infinity;
    float DistanceScale = 1.f;

    // Transform 3D position
    FVector3d Transform(const FVector3d& Point) const;
    FVector3d InverseTransform(const FVector3d& Point) const;

    // Transform distance value
    float TransformDistance(float Distance) const;
};
```

---

## C++ Usage Examples

### Creating and Registering a Stamp

```cpp
void AMyActor::CreateVolumeStamp()
{
    // Get or create instanced stamp component
    UVoxelInstancedStampComponent* StampComp = FindComponentByClass<UVoxelInstancedStampComponent>();
    if (!StampComp)
    {
        StampComp = NewObject<UVoxelInstancedStampComponent>(this);
        StampComp->RegisterComponent();
    }

    // Create new stamp ref
    FVoxelShapeStamp ShapeStamp;
    ShapeStamp.Layer = MyVolumeLayer;
    ShapeStamp.BlendMode = EVoxelVolumeBlendMode::Additive;
    ShapeStamp.Priority = 100;
    ShapeStamp.Smoothness = 200.f;

    // Configure sphere shape
    FVoxelSphereShape Sphere;
    Sphere.Radius = 500.f;
    ShapeStamp.Shape.InitializeAs<FVoxelSphereShape>(Sphere);

    // Create stamp ref and add
    FVoxelStampRef StampRef = FVoxelStampRef::New(ShapeStamp);
    int32 Index = StampComp->AddStamp(StampRef);

    // Store index for later updates
    MyStampIndex = Index;
}
```

### Updating a Stamp

```cpp
void AMyActor::UpdateStampSize(float NewRadius)
{
    UVoxelInstancedStampComponent* StampComp = FindComponentByClass<UVoxelInstancedStampComponent>();
    if (!StampComp) return;

    // Get current stamp
    FVoxelStampRef StampRef = StampComp->GetStamp(MyStampIndex);
    if (!StampRef.IsValid()) return;

    // Modify if it's a shape stamp
    if (FVoxelShapeStamp* ShapeStamp = StampRef.As<FVoxelShapeStamp>())
    {
        if (FVoxelSphereShape* Sphere = ShapeStamp->Shape.TryGet<FVoxelSphereShape>())
        {
            Sphere->Radius = NewRadius;
        }
    }

    // Request update
    StampComp->UpdateStamp(MyStampIndex);
}
```

### Using AVoxelStampActor

```cpp
void AMyGameMode::SpawnTerrainFeature(FVector Location)
{
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AVoxelStampActor* StampActor = GetWorld()->SpawnActor<AVoxelStampActor>(
        AVoxelStampActor::StaticClass(),
        Location,
        FRotator::ZeroRotator,
        Params);

    // Configure stamp
    FVoxelHeightGraphStamp HeightStamp;
    HeightStamp.Layer = MainHeightLayer;
    HeightStamp.Graph = TerrainFeatureGraph;
    HeightStamp.BlendMode = EVoxelHeightBlendMode::Max;
    HeightStamp.Priority = 50;

    StampActor->SetStamp(HeightStamp);
}
```

### Batch Stamp Registration

```cpp
void AMySpawner::SpawnManyStamps(const TArray<FVector>& Locations)
{
    UVoxelInstancedStampComponent* StampComp = GetInstancedStampComponent();

    // Pre-allocate
    StampComp->Reserve(StampComp->NumStamps() + Locations.Num());

    // Create stamps
    TVoxelArray<FVoxelStampRef> NewStamps;
    NewStamps.Reserve(Locations.Num());

    for (const FVector& Location : Locations)
    {
        FVoxelShapeStamp ShapeStamp;
        ShapeStamp.Layer = VolumeLayer;
        ShapeStamp.Transform.SetLocation(Location);
        ShapeStamp.BlendMode = EVoxelVolumeBlendMode::Additive;

        FVoxelSphereShape Sphere;
        Sphere.Radius = FMath::RandRange(200.f, 500.f);
        ShapeStamp.Shape.InitializeAs<FVoxelSphereShape>(Sphere);

        NewStamps.Add(FVoxelStampRef::New(ShapeStamp));
    }

    // Bulk add (more efficient)
    StampComp->AddStamps_NoCopy(MoveTemp(NewStamps));
}
```

---

## Blueprint Usage

### Setting Up Stamp Actor

1. Create Blueprint inheriting from `AVoxelStampActor`
2. In Construction Script or BeginPlay:
   - Call `Get Stamp` to access current stamp
   - Modify stamp properties
   - Call `Update Stamp` to apply changes

### Using Instanced Stamp Component

1. Add `UVoxelInstancedStampComponent` to your actor
2. In Blueprint:
   ```
   Add Stamp → Returns Index
   Get Stamp (Index) → Returns Stamp Ref
   Set Stamp (Index, New Stamp) → Updates
   Remove Stamp (Index) → Clears
   Update Stamp (Index) → Refreshes terrain
   ```

---

## Stamp Hierarchy

```
FVoxelStamp (base)
├── FVoxelHeightStamp
│   ├── FVoxelHeightGraphStamp
│   ├── FVoxelHeightmapStamp
│   └── FVoxelHeightSculptStamp
└── FVoxelVolumeStamp
    ├── FVoxelVolumeGraphStamp
    ├── FVoxelShapeStamp
    ├── FVoxelMeshStamp
    └── FVoxelVolumeSculptStamp
```

---

## Priority System

Stamps with higher `Priority` values are applied later, overriding lower-priority stamps.

```
Priority Order (applied first to last):
  Priority 0: Base terrain noise
  Priority 10: Large features (mountains)
  Priority 50: Medium features (hills)
  Priority 100: Small features (rocks)
  Priority 200: Player modifications
  Priority 500: Roads (override mode)
```

### Auto-Priority

Use `GetNewStampPriority()` to automatically get next priority:

```cpp
int32 NewPriority = UVoxelStampComponent::GetNewStampPriority(GetWorld(), MyStampRef);
```

---

## Best Practices

### Performance

1. **Use Instanced Component** for many stamps of same type
2. **Batch updates** with `UpdateStamps()` instead of individual calls
3. **Minimize stamp count** by combining where possible
4. **Use appropriate LOD range** to skip distant stamps

### Organization

1. **Group stamps by layer** for predictable behavior
2. **Use consistent priority** scheme across project
3. **Set meaningful Behavior** flags (don't affect what you don't need)

### Memory

1. **Use `FVoxelStampRef`** for shared references
2. **Use `FVoxelInstancedStampRef`** only when deep copy needed
3. **Unregister unused stamps** to free runtime memory

---

## Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| Stamp not visible | Not registered | Ensure component is registered |
| Wrong layer | Layer is null | Assign Layer property |
| Overlapping stamps fight | Same priority | Adjust Priority values |
| Stamp appears in wrong place | Transform issue | Check component transform chain |
| Performance issues | Too many stamps | Use instanced component, reduce count |
| Stamp doesn't update | Update not called | Call `Update()` after changes |

---

## Related Topics

- [05-height-layer-system.md](05-height-layer-system.md) - Height stamps
- [06-volume-layer-system.md](06-volume-layer-system.md) - Volume stamps
- [08-runtime-sculpting.md](08-runtime-sculpting.md) - Sculpt stamps
- [09-persistence-saveload.md](09-persistence-saveload.md) - Stamp persistence
