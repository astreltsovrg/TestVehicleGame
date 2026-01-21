# Runtime Sculpting

## Summary

VoxelPlugin2 provides a comprehensive runtime sculpting system through `AVoxelVolumeSculptActor` and `UVoxelVolumeSculptBlueprintLibrary`. This enables Minecraft-style terrain editing at runtime with operations like SculptSphere, SculptCube, Flatten, Smooth, PaintSurface, and custom graph-based sculpting. All operations are asynchronous and return `FVoxelFuture` for completion tracking.

---

## Key Classes

| Class | Header | Purpose |
|-------|--------|---------|
| `AVoxelVolumeSculptActor` | VoxelVolumeSculptActor.h | Main actor for runtime sculpting |
| `UVoxelVolumeSculptComponent` | VoxelVolumeSculptActor.h | Component managing sculpt stamp |
| `UVoxelVolumeSculptBlueprintLibrary` | VoxelVolumeSculptBlueprintLibrary.h | Blueprint API for sculpting |
| `FVoxelVolumeModifier` | VoxelVolumeModifier.h | Base class for custom modifiers |
| `FVoxelVolumeSculptStamp` | VoxelVolumeSculptStamp.h | Runtime-editable volume stamp |
| `FVoxelVolumeSculptSave` | VoxelSculptSave.h | Serializable sculpt data |
| `FVoxelToolBrush` | VoxelToolBrush.h | Brush configuration for tools |

---

## Enums Reference

### EVoxelSculptMode

Controls add/remove behavior:

```cpp
UENUM(BlueprintType)
enum class EVoxelSculptMode : uint8
{
    Add,    // Add material (fill terrain)
    Remove  // Remove material (dig/carve)
};
```

### EVoxelLevelToolType

Controls flatten direction:

```cpp
UENUM(BlueprintType, meta = (VoxelSegmentedEnum))
enum class EVoxelLevelToolType : uint8
{
    Additive,    // Only grow surface (fill below plane)
    Subtractive, // Only shrink surface (remove above plane)
    Both         // Grow and shrink simultaneously
};
```

### EVoxelSDFMergeMode

Controls angle sculpting merge behavior:

```cpp
UENUM(BlueprintType)
enum class EVoxelSDFMergeMode : uint8
{
    Union,        // Additive - only grows surface
    Intersection, // Destructive - only shrinks surface
    Override      // Adds and removes at the same time
};
```

### EVoxelBrushType

Brush mask types:

```cpp
UENUM(BlueprintType, meta = (VoxelSegmentedEnum))
enum class EVoxelBrushType : uint8
{
    Circular, // Simple circular brush with falloff
    Alpha,    // Texture mask oriented with stroke
    Pattern   // Tiled texture mask across landscape
};
```

---

## AVoxelVolumeSculptActor

The main actor for runtime terrain editing.

### Setup

1. Place `AVoxelVolumeSculptActor` in level
2. Configure the associated `FVoxelVolumeSculptStamp` in Details panel
3. Assign to a VolumeLayer in your LayerStack
4. Call sculpting functions from Blueprint or C++

### Key Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `Scale` | float | 100 | Internal resolution scale (cm per voxel) |
| `bUseFastDistances` | bool | false | Use 8-bit (true) vs 32-bit (false) distances |
| `bEnableDiffing` | bool | true | Track changes for incremental updates |
| `StackOverride` | UVoxelLayerStack* | nullptr | Alternative stack for distance queries |

### bUseFastDistances Tradeoff

| Setting | Pros | Cons |
|---------|------|------|
| **false** (32-bit) | High precision, smooth surfaces | More memory usage |
| **true** (8-bit) | Lower memory, faster operations | Lower precision, potential artifacts |

**Recommendation:** Use `false` for final quality, `true` for prototyping or memory-constrained scenarios.

### bEnableDiffing Tradeoff

| Setting | Pros | Cons |
|---------|------|------|
| **true** | Clean updates when moving stamps | Slower editing (~5x) |
| **false** | Much faster editing | Visible chunk seams if stamps move |

**Recommendation:** Use `true` unless editing performance is critical and stamps won't move.

### Blueprint Methods

```cpp
// Get the underlying sculpt stamp reference
UFUNCTION(BlueprintCallable)
FVoxelVolumeSculptStampRef GetStamp() const;

// Get external save asset (if any)
UFUNCTION(BlueprintCallable)
UVoxelVolumeSculptSaveAsset* GetExternalSaveAsset() const;

// Set external save asset (clears existing data)
UFUNCTION(BlueprintCallable)
void SetExternalSaveAsset(UVoxelVolumeSculptSaveAsset* NewExternalSaveAsset);
```

### C++ Methods

```cpp
// Apply a custom modifier
FVoxelFuture ApplyModifier(const TSharedRef<FVoxelVolumeModifier>& Modifier);

// Clear all sculpt data
FVoxelFuture ClearSculptData();

// Clear cached data (frees memory, subsequent edits slower)
virtual void ClearSculptCache() override;

// Save/Load operations
TVoxelFuture<FVoxelVolumeSculptSave> GetSave(bool bCompress = true) const;
FVoxelFuture LoadFromSave(const FVoxelVolumeSculptSave& Save);
```

---

## Blueprint Sculpting API

All functions are in `UVoxelVolumeSculptBlueprintLibrary` and return `FVoxelFuture` for async completion.

### SculptSphere

Add or remove a sphere shape.

```cpp
UFUNCTION(BlueprintCallable, Category = "Voxel|Sculpt")
static FVoxelFuture SculptSphere(
    AVoxelVolumeSculptActor* SculptActor,
    const FVector Center,
    const float Radius = 1000.f,
    const EVoxelSculptMode Mode = EVoxelSculptMode::Add,
    const float Smoothness = 0.f);
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `SculptActor` | AVoxelVolumeSculptActor* | Required | Target sculpt actor |
| `Center` | FVector | Required | World space center |
| `Radius` | float | 1000 | Sphere radius in cm |
| `Mode` | EVoxelSculptMode | Add | Add or Remove material |
| `Smoothness` | float | 0 | Blend smoothness (0 = hard edge) |

### SculptCube

Add or remove a cube/box shape.

```cpp
UFUNCTION(BlueprintCallable, Category = "Voxel|Sculpt")
static FVoxelFuture SculptCube(
    AVoxelVolumeSculptActor* SculptActor,
    const FVector Center,
    const FVector Size = FVector(1000.f),
    const FRotator Rotation = FRotator::ZeroRotator,
    const float Roundness = 0.f,
    const EVoxelSculptMode Mode = EVoxelSculptMode::Add,
    const float Smoothness = 0.f);
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `SculptActor` | AVoxelVolumeSculptActor* | Required | Target sculpt actor |
| `Center` | FVector | Required | World space center |
| `Size` | FVector | (1000, 1000, 1000) | Box dimensions in cm |
| `Rotation` | FRotator | ZeroRotator | Orientation |
| `Roundness` | float | 0 | Corner rounding (0 = sharp) |
| `Mode` | EVoxelSculptMode | Add | Add or Remove material |
| `Smoothness` | float | 0 | Blend smoothness |

### Flatten

Flatten terrain to a horizontal plane.

```cpp
UFUNCTION(BlueprintCallable, Category = "Voxel|Sculpt")
static FVoxelFuture Flatten(
    AVoxelVolumeSculptActor* SculptActor,
    const FVector Center,
    const FVector Normal = FVector::UpVector,
    const float Radius = 500.f,
    const float Height = 1000.f,
    const float Falloff = 0.1f,
    const EVoxelLevelToolType Type = EVoxelLevelToolType::Additive);
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `SculptActor` | AVoxelVolumeSculptActor* | Required | Target sculpt actor |
| `Center` | FVector | Required | World space center |
| `Normal` | FVector | UpVector | Plane normal direction |
| `Radius` | float | 500 | Flatten area radius |
| `Height` | float | 1000 | Vertical extent to affect |
| `Falloff` | float | 0.1 | Edge falloff (0-1) |
| `Type` | EVoxelLevelToolType | Additive | Fill, carve, or both |

### Smooth

Smooth existing terrain surface.

```cpp
UFUNCTION(BlueprintCallable, Category = "Voxel|Sculpt")
static FVoxelFuture Smooth(
    AVoxelVolumeSculptActor* SculptActor,
    const FVector Center,
    const float Radius = 1000.f,
    const float Strength = 1.f,
    const FVoxelToolBrush Brush = FVoxelToolBrush());
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `SculptActor` | AVoxelVolumeSculptActor* | Required | Target sculpt actor |
| `Center` | FVector | Required | World space center |
| `Radius` | float | 1000 | Smoothing area radius |
| `Strength` | float | 1 | Smoothing intensity (0-1+) |
| `Brush` | FVoxelToolBrush | Default | Optional brush pattern |

### SculptSurface

Progressively sculpt surface (soft brush).

```cpp
UFUNCTION(BlueprintCallable, Category = "Voxel|Sculpt")
static FVoxelFuture SculptSurface(
    AVoxelVolumeSculptActor* SculptActor,
    const FVector Center,
    const float Radius = 500.f,
    const float Strength = 0.5f,
    const EVoxelSculptMode Mode = EVoxelSculptMode::Add,
    const FVoxelToolBrush Brush = FVoxelToolBrush());
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `SculptActor` | AVoxelVolumeSculptActor* | Required | Target sculpt actor |
| `Center` | FVector | Required | World space center |
| `Radius` | float | 500 | Brush radius |
| `Strength` | float | 0.5 | Sculpt speed/intensity |
| `Mode` | EVoxelSculptMode | Add | Add or Remove |
| `Brush` | FVoxelToolBrush | Default | Optional brush pattern |

### PaintSurface

Paint surface type/material on terrain.

```cpp
UFUNCTION(BlueprintCallable, Category = "Voxel|Sculpt")
static FVoxelFuture PaintSurface(
    AVoxelVolumeSculptActor* SculptActor,
    const FVector Center,
    const float Radius = 500.f,
    const float Strength = 0.05f,
    const EVoxelSculptMode Mode = EVoxelSculptMode::Add,
    UVoxelSurfaceTypeInterface* SurfaceTypeToPaint = nullptr,
    const FVoxelMetadataOverrides MetadatasToPaint = FVoxelMetadataOverrides(),
    const FVoxelToolBrush Brush = FVoxelToolBrush());
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `SculptActor` | AVoxelVolumeSculptActor* | Required | Target sculpt actor |
| `Center` | FVector | Required | World space center |
| `Radius` | float | 500 | Paint area radius |
| `Strength` | float | 0.05 | Paint intensity per stroke |
| `Mode` | EVoxelSculptMode | Add | Add or remove paint |
| `SurfaceTypeToPaint` | UVoxelSurfaceTypeInterface* | nullptr | Surface type to apply |
| `MetadatasToPaint` | FVoxelMetadataOverrides | Default | Additional metadata |
| `Brush` | FVoxelToolBrush | Default | Optional brush pattern |

### SculptAngle

Sculpt surface toward a plane angle.

```cpp
UFUNCTION(BlueprintCallable, Category = "Voxel|Sculpt")
static FVoxelFuture SculptAngle(
    AVoxelVolumeSculptActor* SculptActor,
    const FVector Center,
    const float Radius = 500.f,
    const float Strength = 1.f,
    const FVector& PlanePoint = FVector(0, 0, 0),
    const FVector& PlaneNormal = FVector(0, 0, 1),
    const EVoxelSDFMergeMode MergeMode = EVoxelSDFMergeMode::Override,
    const FVoxelToolBrush Brush = FVoxelToolBrush());
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `SculptActor` | AVoxelVolumeSculptActor* | Required | Target sculpt actor |
| `Center` | FVector | Required | World space center |
| `Radius` | float | 500 | Effect radius |
| `Strength` | float | 1 | Sculpt intensity |
| `PlanePoint` | FVector | Zero | Point on target plane |
| `PlaneNormal` | FVector | UpVector | Plane normal direction |
| `MergeMode` | EVoxelSDFMergeMode | Override | How to merge with existing |
| `Brush` | FVoxelToolBrush | Default | Optional brush pattern |

### ApplySculptGraph

Apply a custom sculpt graph for complex operations.

```cpp
UFUNCTION(BlueprintCallable, Category = "Voxel|Sculpt")
static FVoxelFuture ApplySculptGraph(
    AVoxelVolumeSculptActor* SculptActor,
    const FVector& Center,
    const float Radius = 500.f,
    const FVoxelVolumeSculptGraphWrapper& Graph = FVoxelVolumeSculptGraphWrapper(),
    const FRotator& Rotation = FRotator::ZeroRotator);
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `SculptActor` | AVoxelVolumeSculptActor* | Required | Target sculpt actor |
| `Center` | FVector | Required | World space center |
| `Radius` | float | 500 | Effect radius |
| `Graph` | FVoxelVolumeSculptGraphWrapper | Required | Sculpt graph to apply |
| `Rotation` | FRotator | ZeroRotator | Graph orientation |

---

## FVoxelToolBrush

Configurable brush for sculpting operations.

### Structure

```cpp
USTRUCT(BlueprintType)
struct FVoxelToolBrush : public FVoxelToolBrushBase
{
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector HitNormal = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector StrokeDirection = FVector::ForwardVector;
};

USTRUCT(BlueprintType)
struct FVoxelToolBrushBase
{
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EVoxelBrushType BrushType = EVoxelBrushType::Circular;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVoxelCircularBrush CircularBrushData;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVoxelAlphaBrush AlphaBrushData;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVoxelPatternBrush PatternBrushData;
};
```

### Brush Types

| Type | Use Case |
|------|----------|
| **Circular** | Standard round brush with falloff |
| **Alpha** | Texture mask that follows stroke direction |
| **Pattern** | Tiled texture across affected area |

### Circular Brush

```cpp
USTRUCT(BlueprintType)
struct FVoxelCircularBrush
{
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVoxelFalloff Falloff;
};
```

### Alpha Brush

```cpp
USTRUCT(BlueprintType)
struct FVoxelAlphaBrush
{
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UVoxelTexture> Texture;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EVoxelTextureChannel TextureChannel = EVoxelTextureChannel::R;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAutoRotateMask = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FixedRotation = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUse2DProjection = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVoxelFalloff Falloff;
};
```

---

## FVoxelVolumeModifier

Base class for creating custom sculpting operations.

### Class Declaration

```cpp
struct VOXEL_API FVoxelVolumeModifier : public FVoxelVirtualStruct
{
    GENERATED_BODY()
    GENERATED_VIRTUAL_STRUCT_BODY()

    // Called on game thread before Apply
    virtual void Initialize_GameThread();

    // Return bounds of the modification
    virtual FVoxelBox GetBounds() const VOXEL_PURE_VIRTUAL({});

    // Declare what this modifier writes
    virtual void GetUsage(
        bool& bWritesDistances,
        bool& bWritesSurfaceTypes,
        TVoxelSet<FVoxelMetadataRef>& MetadataRefsToWrite) const;

    // Apply the modification (called on worker thread)
    virtual void Apply(const FData& Data) const VOXEL_PURE_VIRTUAL();
};
```

### Built-in Modifiers

| Modifier | Purpose | Key Properties |
|----------|---------|----------------|
| `FVoxelSphereVolumeModifier` | Sphere add/remove | Center, Radius, Smoothness, Mode |
| `FVoxelCubeVolumeModifier` | Cube add/remove | Center, Size, Rotation, Roundness, Smoothness, Mode |
| `FVoxelFlattenVolumeModifier` | Flatten terrain | Center, Normal, Radius, Height, Falloff, Type |
| `FVoxelSmoothVolumeModifier` | Smooth surface | Center, Radius, Strength, Brush |
| `FVoxelSurfaceVolumeModifier` | Soft surface sculpt | Center, Radius, Strength, Mode, Brush |
| `FVoxelPaintVolumeModifier` | Paint surface type | Center, Radius, Strength, Mode, SurfaceTypeToPaint, Brush |
| `FVoxelAngleVolumeModifier` | Angle-based sculpt | Center, Radius, Strength, Plane, MergeMode, Brush |
| `FVoxelGraphVolumeModifier` | Graph-based sculpt | Transform, Radius, Graph |

### Custom Modifier Example

```cpp
USTRUCT()
struct FMyCustomModifier : public FVoxelVolumeModifier
{
    GENERATED_BODY()
    GENERATED_VIRTUAL_STRUCT_BODY()

public:
    UPROPERTY()
    FVector Center = FVector::ZeroVector;

    UPROPERTY()
    float Radius = 1000.f;

    UPROPERTY()
    float CustomParameter = 0.5f;

public:
    virtual FVoxelBox GetBounds() const override
    {
        return FVoxelBox(Center - FVector(Radius), Center + FVector(Radius));
    }

    virtual void Apply(const FData& Data) const override
    {
        // Data.Distances - read/write distance field
        // Data.SurfaceTypes - read/write surface types
        // Data.QueryBounds - bounds being processed

        for (int32 Index = 0; Index < Data.Distances.Num(); Index++)
        {
            const FVector Position = Data.GetPosition(Index);
            const float Distance = FVector::Dist(Position, Center);

            if (Distance < Radius)
            {
                // Apply custom logic
                const float Strength = 1.f - (Distance / Radius);
                Data.Distances[Index] -= CustomParameter * Strength * 100.f;
            }
        }
    }
};

// Usage:
void ApplyCustomSculpt(AVoxelVolumeSculptActor* Actor, FVector Location)
{
    const TSharedRef<FMyCustomModifier> Modifier = MakeShared<FMyCustomModifier>();
    Modifier->Center = Location;
    Modifier->Radius = 500.f;
    Modifier->CustomParameter = 0.8f;

    Actor->ApplyModifier(Modifier);
}
```

---

## Save/Load Operations

### Save Structure

```cpp
USTRUCT(BlueprintType)
struct VOXEL_API FVoxelVolumeSculptSave : public FVoxelSculptSaveBase
{
    // Inherited methods:
    bool IsValid() const;
    bool IsCompressed() const;
    int64 GetSize() const;
};
```

### Blueprint Save/Load

```cpp
// Check if save is valid
UFUNCTION(BlueprintPure)
static bool IsValidSave(FVoxelVolumeSculptSave Save);

// Check if save is compressed
UFUNCTION(BlueprintPure)
static bool IsCompressedSave(FVoxelVolumeSculptSave Save);

// Get save size in bytes
UFUNCTION(BlueprintPure)
static int64 GetSaveSize(FVoxelVolumeSculptSave Save);

// Get save data from actor
UFUNCTION(BlueprintCallable)
static FVoxelFuture K2_GetSave(
    FVoxelVolumeSculptSave& Save,
    AVoxelVolumeSculptActor* SculptActor,
    bool bCompress = true);

// Load save data into actor
UFUNCTION(BlueprintCallable)
static FVoxelFuture LoadFromSave(
    AVoxelVolumeSculptActor* SculptActor,
    FVoxelVolumeSculptSave Save);

// Clear all sculpt data
UFUNCTION(BlueprintCallable)
static FVoxelFuture ClearSculptData(AVoxelVolumeSculptActor* SculptActor);

// Clear sculpt cache (free memory)
UFUNCTION(BlueprintCallable)
static void ClearSculptCache(AVoxelVolumeSculptActor* SculptActor);
```

### C++ Save/Load Example

```cpp
void AMyGameMode::SaveTerrain(AVoxelVolumeSculptActor* SculptActor)
{
    // Get save data (async)
    SculptActor->GetSave(true).Then([this](const FVoxelVolumeSculptSave& Save)
    {
        if (Save.IsValid())
        {
            UE_LOG(LogTemp, Log, TEXT("Save size: %lld bytes (compressed: %s)"),
                Save.GetSize(),
                Save.IsCompressed() ? TEXT("yes") : TEXT("no"));

            // Store Save in your save game system
            MySaveGame->TerrainData = Save;
        }
    });
}

void AMyGameMode::LoadTerrain(AVoxelVolumeSculptActor* SculptActor)
{
    // Retrieve from your save game system
    const FVoxelVolumeSculptSave& Save = MySaveGame->TerrainData;

    if (Save.IsValid())
    {
        SculptActor->LoadFromSave(Save).Then([]()
        {
            UE_LOG(LogTemp, Log, TEXT("Terrain loaded successfully"));
        });
    }
}
```

---

## C++ Usage Examples

### Basic Sculpting Controller

```cpp
UCLASS()
class ATerrainSculptController : public AActor
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    AVoxelVolumeSculptActor* SculptActor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BrushRadius = 500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BrushStrength = 0.5f;

public:
    void Dig(const FVector& Location)
    {
        if (!SculptActor) return;

        UVoxelVolumeSculptBlueprintLibrary::SculptSphere(
            SculptActor,
            Location,
            BrushRadius,
            EVoxelSculptMode::Remove,
            100.f  // Smoothness
        );
    }

    void Build(const FVector& Location)
    {
        if (!SculptActor) return;

        UVoxelVolumeSculptBlueprintLibrary::SculptSphere(
            SculptActor,
            Location,
            BrushRadius,
            EVoxelSculptMode::Add,
            100.f  // Smoothness
        );
    }

    void FlattenArea(const FVector& Location)
    {
        if (!SculptActor) return;

        UVoxelVolumeSculptBlueprintLibrary::Flatten(
            SculptActor,
            Location,
            FVector::UpVector,
            BrushRadius,
            1000.f,  // Height
            0.2f,    // Falloff
            EVoxelLevelToolType::Both
        );
    }

    void SmoothArea(const FVector& Location)
    {
        if (!SculptActor) return;

        UVoxelVolumeSculptBlueprintLibrary::Smooth(
            SculptActor,
            Location,
            BrushRadius,
            BrushStrength
        );
    }
};
```

### Player Dig/Build System

```cpp
void APlayerCharacter::OnPrimaryAction()
{
    FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(
        Hit,
        GetActorLocation(),
        GetActorLocation() + GetControlRotation().Vector() * 1000.f,
        ECC_Visibility))
    {
        // Dig at hit location
        UVoxelVolumeSculptBlueprintLibrary::SculptSphere(
            TerrainSculptActor,
            Hit.Location,
            200.f,          // Small radius for precision
            EVoxelSculptMode::Remove,
            50.f            // Some smoothness
        );
    }
}

void APlayerCharacter::OnSecondaryAction()
{
    FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(
        Hit,
        GetActorLocation(),
        GetActorLocation() + GetControlRotation().Vector() * 1000.f,
        ECC_Visibility))
    {
        // Build at hit location
        UVoxelVolumeSculptBlueprintLibrary::SculptSphere(
            TerrainSculptActor,
            Hit.Location + Hit.Normal * 100.f,  // Offset outward
            200.f,
            EVoxelSculptMode::Add,
            50.f
        );
    }
}
```

---

## Blueprint Usage

### Basic Dig/Build

```
Event Primary Fire
    │
    ├── Line Trace by Channel
    │       Start: Camera Location
    │       End: Camera Location + Camera Forward * 1000
    │
    └── Branch (Hit?)
            │
            └── Sculpt Sphere
                    SculptActor: Terrain Sculpt Actor Reference
                    Center: Hit Location
                    Radius: 300
                    Mode: Remove
                    Smoothness: 100
```

### Continuous Sculpting

```
Event Tick
    │
    ├── Branch (Is Sculpt Button Held?)
    │       │
    │       └── Line Trace from Camera
    │               │
    │               └── Sculpt Surface
    │                       SculptActor: Terrain Sculpt Actor
    │                       Center: Hit Location
    │                       Radius: 400
    │                       Strength: 0.3 * Delta Time
    │                       Mode: Add/Remove based on input
```

### Save/Load in Blueprint

```
Function: Save Terrain
    │
    ├── Get Save (Async)
    │       SculptActor: Terrain Sculpt Actor
    │       bCompress: true
    │       │
    │       └── On Complete
    │               │
    │               └── Branch (Is Valid Save?)
    │                       │
    │                       └── Set Save Game Variable
    │                               TerrainSave = Save


Function: Load Terrain
    │
    ├── Get Save Game Variable: TerrainSave
    │       │
    │       └── Branch (Is Valid Save?)
    │               │
    │               └── Load From Save (Async)
    │                       SculptActor: Terrain Sculpt Actor
    │                       Save: TerrainSave
```

---

## Performance Optimization

### Best Practices

1. **Batch Operations**: Group multiple sculpts together when possible
2. **Reasonable Radii**: Keep radii as small as needed (larger = more computation)
3. **bUseFastDistances**: Enable for prototyping or less critical terrain
4. **bEnableDiffing**: Disable if stamps won't move and speed is critical
5. **ClearSculptCache**: Call periodically to free memory
6. **Async Handling**: Use `FVoxelFuture` properly to avoid blocking

### Performance Comparison

| Operation | Small (R=200) | Medium (R=500) | Large (R=1000) |
|-----------|---------------|----------------|----------------|
| SculptSphere | ~1ms | ~5ms | ~15ms |
| SculptCube | ~1ms | ~6ms | ~18ms |
| Flatten | ~2ms | ~8ms | ~25ms |
| Smooth | ~3ms | ~12ms | ~35ms |

*Approximate times on mid-range hardware; actual performance varies.*

---

## Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| No visible change | SculptActor not in LayerStack | Add to VolumeLayer in stack |
| Changes disappear | Save not persisted | Implement save/load system |
| Slow editing | bEnableDiffing = true | Set to false if appropriate |
| Blocky results | bUseFastDistances = true | Set to false for quality |
| Memory growth | Sculpt cache buildup | Call ClearSculptCache periodically |
| Stamp seams visible | bEnableDiffing = false + moved stamps | Enable diffing or don't move stamps |

---

## Related Topics

- [06-volume-layer-system.md](06-volume-layer-system.md) - Volume terrain fundamentals
- [07-stamp-system.md](07-stamp-system.md) - Stamp architecture
- [09-persistence-saveload.md](09-persistence-saveload.md) - Complete save/load system
- [16-megamaterial-system.md](16-megamaterial-system.md) - Surface type rendering
