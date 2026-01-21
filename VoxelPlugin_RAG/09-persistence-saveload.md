# Persistence & Save/Load System

## Summary

VoxelPlugin2 provides a complete persistence system for sculpted terrain data through `FVoxelVolumeSculptSave`, `FVoxelHeightSculptSave`, and their corresponding `UVoxelSculptSaveAsset` classes. The system supports compression, external asset storage, and both volume (3D) and height (2D) sculpt data. All save/load operations are asynchronous and return `FVoxelFuture`.

---

## Key Classes

| Class | Header | Purpose |
|-------|--------|---------|
| `FVoxelVolumeSculptSave` | VoxelSculptSave.h | Serializable 3D volume sculpt data |
| `FVoxelHeightSculptSave` | VoxelSculptSave.h | Serializable 2D height sculpt data |
| `FVoxelSculptSaveBase` | VoxelSculptSave.h | Base class with common save functionality |
| `UVoxelVolumeSculptSaveAsset` | VoxelSculptSaveAsset.h | Persistent asset for volume sculpt data |
| `UVoxelHeightSculptSaveAsset` | VoxelSculptSaveAsset.h | Persistent asset for height sculpt data |
| `AVoxelVolumeSculptActor` | VoxelVolumeSculptActor.h | 3D sculpt actor with save/load |
| `AVoxelHeightSculptActor` | VoxelHeightSculptActor.h | 2D sculpt actor with save/load |

---

## Save Data Structures

### FVoxelSculptSaveBase

Base class providing common save functionality:

```cpp
struct VOXEL_API FVoxelSculptSaveBase
{
    // Check if save contains valid data
    bool IsValid() const;

    // Check if save data is compressed
    bool IsCompressed() const;

    // Get save data size in bytes
    int64 GetSize() const;

    // Internal serialization support
    bool Serialize(FArchive& Ar);
    bool Identical(const FVoxelSculptSaveBase* Other, uint32 PortFlags) const;

protected:
    struct FData
    {
        bool bIsCompressed = false;
        TVoxelArray64<uint8> Data;  // 64-bit array for large terrains
    };
    TSharedPtr<FData> Data;
};
```

### FVoxelVolumeSculptSave

For 3D volume terrain:

```cpp
USTRUCT(BlueprintType)
struct VOXEL_API FVoxelVolumeSculptSave : public FVoxelSculptSaveBase
{
    GENERATED_BODY()
    // Inherits all FVoxelSculptSaveBase methods
};
```

### FVoxelHeightSculptSave

For 2D heightfield terrain:

```cpp
USTRUCT(BlueprintType)
struct VOXEL_API FVoxelHeightSculptSave : public FVoxelSculptSaveBase
{
    GENERATED_BODY()
    // Inherits all FVoxelSculptSaveBase methods
};
```

---

## Save Assets (External Storage)

Save assets provide persistent storage in the Content Browser, separate from level data.

### UVoxelVolumeSculptSaveAsset

```cpp
UCLASS(meta = (VoxelAssetType, AssetColor=Red))
class VOXEL_API UVoxelVolumeSculptSaveAsset : public UObject
{
    GENERATED_BODY()

public:
    // Internal resolution scale (cm per voxel)
    UPROPERTY(EditAnywhere, Category = "Config")
    float Scale = 100;

    // Use 8-bit (true) vs 32-bit (false) distances
    // Changing this clears existing data
    UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay)
    bool bUseFastDistances = false;

    // Track changes for incremental updates
    // Disabling makes editing ~5x faster but causes seams if stamps move
    UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay)
    bool bEnableDiffing = true;

    // Alternative stack for distance queries
    UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay)
    TObjectPtr<UVoxelLayerStack> StackOverride;

public:
    // Delegate for property change notifications
    FSimpleMulticastDelegate OnPropertyChanged;

    // Get the internal sculpt data
    TSharedRef<FVoxelVolumeSculptData> GetSculptData();
};
```

### UVoxelHeightSculptSaveAsset

```cpp
UCLASS(meta = (VoxelAssetType, AssetColor=Red))
class VOXEL_API UVoxelHeightSculptSaveAsset : public UObject
{
    GENERATED_BODY()

public:
    // Resolution scale in XY plane
    UPROPERTY(EditAnywhere, Category = "Config")
    float ScaleXY = 100;

    // Store height relative to previous stamps
    UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay)
    bool bRelativeHeight = false;

    // Alternative stack for height queries
    UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay)
    TObjectPtr<UVoxelLayerStack> StackOverride;

public:
    FSimpleMulticastDelegate OnPropertyChanged;
    TSharedRef<FVoxelHeightSculptData> GetSculptData();
};
```

### Creating Save Assets

1. **In Content Browser:**
   - Right-click > Voxel > Volume Sculpt Save Asset
   - Or: Right-click > Voxel > Height Sculpt Save Asset

2. **Drag into Scene:**
   - Drag the asset onto an existing SculptActor
   - Or create a new SculptActor and assign the asset

---

## Volume Sculpt Save/Load API

### Blueprint Library Functions

```cpp
// Check if save data is valid
UFUNCTION(BlueprintPure)
static bool IsValidSave(FVoxelVolumeSculptSave Save);

// Check if save data is compressed
UFUNCTION(BlueprintPure)
static bool IsCompressedSave(FVoxelVolumeSculptSave Save);

// Get save data size in bytes
UFUNCTION(BlueprintPure)
static int64 GetSaveSize(FVoxelVolumeSculptSave Save);

// Get save data from sculpt actor (async)
UFUNCTION(BlueprintCallable, Category = "Voxel")
static FVoxelFuture K2_GetSave(
    FVoxelVolumeSculptSave& Save,
    AVoxelVolumeSculptActor* SculptActor,
    bool bCompress = true);

// Load save data into sculpt actor (async)
UFUNCTION(BlueprintCallable, Category = "Voxel")
static FVoxelFuture LoadFromSave(
    AVoxelVolumeSculptActor* SculptActor,
    FVoxelVolumeSculptSave Save);

// Clear all sculpt data (async)
UFUNCTION(BlueprintCallable, Category = "Voxel|Sculpt")
static FVoxelFuture ClearSculptData(AVoxelVolumeSculptActor* SculptActor);

// Clear sculpt cache (frees memory, immediate)
UFUNCTION(BlueprintCallable, Category = "Voxel|Sculpt")
static void ClearSculptCache(AVoxelVolumeSculptActor* SculptActor);
```

### Actor Methods

```cpp
class AVoxelVolumeSculptActor
{
    // Get save data (async, returns future with save data)
    TVoxelFuture<FVoxelVolumeSculptSave> GetSave(bool bCompress = true) const;

    // Load from save data (async)
    FVoxelFuture LoadFromSave(const FVoxelVolumeSculptSave& Save);

    // Get external save asset (if any)
    UVoxelVolumeSculptSaveAsset* GetExternalSaveAsset() const;

    // Set external save asset (clears existing data)
    void SetExternalSaveAsset(UVoxelVolumeSculptSaveAsset* NewExternalSaveAsset);

    // Clear all sculpt data (async)
    FVoxelFuture ClearSculptData();

    // Clear cache to free memory
    virtual void ClearSculptCache() override;
};
```

---

## Height Sculpt Save/Load API

### Blueprint Library Functions

```cpp
// Identical API to volume sculpt, but with height types
UFUNCTION(BlueprintPure)
static bool IsValidSave(FVoxelHeightSculptSave Save);

UFUNCTION(BlueprintPure)
static bool IsCompressedSave(FVoxelHeightSculptSave Save);

UFUNCTION(BlueprintPure)
static int64 GetSaveSize(FVoxelHeightSculptSave Save);

UFUNCTION(BlueprintCallable, Category = "Voxel")
static FVoxelFuture K2_GetSave(
    FVoxelHeightSculptSave& Save,
    AVoxelHeightSculptActor* SculptActor,
    bool bCompress = true);

UFUNCTION(BlueprintCallable, Category = "Voxel")
static FVoxelFuture LoadFromSave(
    AVoxelHeightSculptActor* SculptActor,
    FVoxelHeightSculptSave Save);

UFUNCTION(BlueprintCallable, Category = "Voxel|Sculpt")
static FVoxelFuture ClearSculptData(AVoxelHeightSculptActor* SculptActor);

UFUNCTION(BlueprintCallable, Category = "Voxel|Sculpt")
static void ClearSculptCache(AVoxelHeightSculptActor* SculptActor);
```

---

## Storage Options Comparison

| Feature | Inline (Default) | External Save Asset |
|---------|------------------|---------------------|
| **Storage Location** | Level/Actor | Content Browser (.uasset) |
| **Shared Across Levels** | No | Yes |
| **Version Control** | Per-level | Separate asset |
| **Runtime Modification** | Yes | Yes |
| **Editor Preview** | Immediate | Immediate |
| **Memory** | Loaded with level | On-demand |

### When to Use Each

**Inline Storage (Default):**
- Single-level terrain
- Level-specific modifications
- Simple projects

**External Save Asset:**
- Terrain shared across multiple levels
- Separate version control for terrain
- Large terrain data (keeps level files smaller)
- Team collaboration (different artists for terrain vs. level)

---

## C++ Usage Examples

### Basic Save/Load

```cpp
void AMyGameMode::SaveTerrainToVariable(AVoxelVolumeSculptActor* SculptActor)
{
    // Get save (async operation)
    SculptActor->GetSave(true).Then([this](const FVoxelVolumeSculptSave& Save)
    {
        if (Save.IsValid())
        {
            // Store in class variable
            CachedTerrainSave = Save;

            UE_LOG(LogTemp, Log, TEXT("Terrain saved: %lld bytes (%s)"),
                Save.GetSize(),
                Save.IsCompressed() ? TEXT("compressed") : TEXT("uncompressed"));
        }
    });
}

void AMyGameMode::LoadTerrainFromVariable(AVoxelVolumeSculptActor* SculptActor)
{
    if (CachedTerrainSave.IsValid())
    {
        SculptActor->LoadFromSave(CachedTerrainSave).Then([]()
        {
            UE_LOG(LogTemp, Log, TEXT("Terrain loaded successfully"));
        });
    }
}
```

### Save to Custom Save Game

```cpp
// MySaveGame.h
UCLASS()
class UMyTerrainSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY()
    FVoxelVolumeSculptSave VolumeSculptData;

    UPROPERTY()
    FVoxelHeightSculptSave HeightSculptData;

    UPROPERTY()
    FString SaveVersion = TEXT("1.0");
};

// Usage
void AMyGameMode::SaveTerrainToSlot(
    AVoxelVolumeSculptActor* VolumeSculpt,
    AVoxelHeightSculptActor* HeightSculpt,
    const FString& SlotName)
{
    UMyTerrainSaveGame* SaveGame = Cast<UMyTerrainSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UMyTerrainSaveGame::StaticClass()));

    // Save volume data
    VolumeSculpt->GetSave(true).Then([SaveGame, HeightSculpt, SlotName, this]
        (const FVoxelVolumeSculptSave& VolumeSave)
    {
        SaveGame->VolumeSculptData = VolumeSave;

        // Then save height data
        HeightSculpt->GetSave(true).Then([SaveGame, SlotName]
            (const FVoxelHeightSculptSave& HeightSave)
        {
            SaveGame->HeightSculptData = HeightSave;

            // Write to disk
            if (UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, 0))
            {
                UE_LOG(LogTemp, Log, TEXT("Terrain saved to slot: %s"), *SlotName);
            }
        });
    });
}

void AMyGameMode::LoadTerrainFromSlot(
    AVoxelVolumeSculptActor* VolumeSculpt,
    AVoxelHeightSculptActor* HeightSculpt,
    const FString& SlotName)
{
    UMyTerrainSaveGame* SaveGame = Cast<UMyTerrainSaveGame>(
        UGameplayStatics::LoadGameFromSlot(SlotName, 0));

    if (SaveGame)
    {
        // Load volume data
        if (SaveGame->VolumeSculptData.IsValid())
        {
            VolumeSculpt->LoadFromSave(SaveGame->VolumeSculptData);
        }

        // Load height data
        if (SaveGame->HeightSculptData.IsValid())
        {
            HeightSculpt->LoadFromSave(SaveGame->HeightSculptData);
        }
    }
}
```

### Using External Save Assets

```cpp
void AMyGameMode::SetupExternalStorage(AVoxelVolumeSculptActor* SculptActor)
{
    // Load external save asset
    UVoxelVolumeSculptSaveAsset* SaveAsset = LoadObject<UVoxelVolumeSculptSaveAsset>(
        nullptr, TEXT("/Game/Terrain/MySaveAsset"));

    if (SaveAsset)
    {
        // Assign to sculpt actor (clears existing data)
        SculptActor->SetExternalSaveAsset(SaveAsset);
    }
}

void AMyGameMode::CheckExternalStorage(AVoxelVolumeSculptActor* SculptActor)
{
    UVoxelVolumeSculptSaveAsset* Asset = SculptActor->GetExternalSaveAsset();

    if (Asset)
    {
        UE_LOG(LogTemp, Log, TEXT("Using external save asset"));

        // All sculpt changes automatically saved to asset
        // No need for manual GetSave/LoadFromSave
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("Using inline storage"));

        // Manual save/load required
    }
}
```

### Auto-Save System

```cpp
void AMyGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Start auto-save timer
    GetWorldTimerManager().SetTimer(
        AutoSaveTimerHandle,
        this,
        &AMyGameMode::AutoSaveTerrain,
        AutoSaveIntervalSeconds,
        true  // Loop
    );
}

void AMyGameMode::AutoSaveTerrain()
{
    if (!VolumeSculptActor) return;

    // Check if there are changes worth saving
    static int64 LastSaveSize = 0;

    VolumeSculptActor->GetSave(true).Then([this](const FVoxelVolumeSculptSave& Save)
    {
        if (Save.IsValid() && Save.GetSize() != LastSaveSize)
        {
            LastSaveSize = Save.GetSize();

            // Save to quick-save slot
            UMyTerrainSaveGame* QuickSave = Cast<UMyTerrainSaveGame>(
                UGameplayStatics::CreateSaveGameObject(UMyTerrainSaveGame::StaticClass()));

            QuickSave->VolumeSculptData = Save;
            UGameplayStatics::SaveGameToSlot(QuickSave, TEXT("QuickSave"), 0);

            UE_LOG(LogTemp, Log, TEXT("Auto-saved terrain: %lld bytes"), Save.GetSize());
        }
    });
}
```

---

## Blueprint Usage

### Basic Save to Slot

```
Function: Save Terrain
    │
    ├── Get Save (K2_GetSave)
    │       SculptActor: Volume Sculpt Actor Reference
    │       bCompress: true
    │       │
    │       └── On Complete (Bind Event)
    │               │
    │               ├── Branch (Is Valid Save?)
    │               │       │
    │               │       └── Create Save Game Object
    │               │               Class: MyTerrainSaveGame
    │               │               │
    │               │               └── Set TerrainData = Save
    │               │                       │
    │               │                       └── Save Game to Slot
    │               │                               Slot Name: "PlayerTerrain"
    │               │                               User Index: 0
    │               │
    │               └── (False) Print String: "Save failed - invalid data"
```

### Basic Load from Slot

```
Function: Load Terrain
    │
    ├── Does Save Game Exist?
    │       Slot Name: "PlayerTerrain"
    │       │
    │       └── Branch
    │               │
    │               ├── (True) Load Game from Slot
    │               │           Slot Name: "PlayerTerrain"
    │               │           │
    │               │           └── Cast to MyTerrainSaveGame
    │               │                   │
    │               │                   └── Get TerrainData
    │               │                           │
    │               │                           └── Branch (Is Valid Save?)
    │               │                                   │
    │               │                                   └── Load From Save
    │               │                                           SculptActor: Volume Sculpt Actor
    │               │                                           Save: TerrainData
    │               │
    │               └── (False) Print String: "No save found"
```

### Display Save Info

```
Event: Display Save Info
    │
    ├── Get Save (K2_GetSave)
    │       SculptActor: Current Sculpt Actor
    │       bCompress: true
    │       │
    │       └── On Complete
    │               │
    │               ├── Get Save Size → Format as KB/MB
    │               │
    │               ├── Is Compressed Save? → Display compression status
    │               │
    │               └── Update UI Text
```

---

## Compression

### Compression Behavior

| bCompress | Result |
|-----------|--------|
| **true** | Data is LZ4 compressed, smaller size, slightly slower save |
| **false** | Uncompressed data, faster save, larger size |

### Typical Compression Ratios

| Terrain Type | Uncompressed | Compressed | Ratio |
|--------------|--------------|------------|-------|
| Simple edits | 1 MB | ~200 KB | 5:1 |
| Complex edits | 10 MB | ~2 MB | 5:1 |
| Dense sculpting | 50 MB | ~12 MB | 4:1 |

### Performance Considerations

```cpp
// Fast save (no compression) for frequent auto-saves
SculptActor->GetSave(false).Then([](const FVoxelVolumeSculptSave& Save)
{
    // Quick checkpoint save
});

// Compressed save for final saves
SculptActor->GetSave(true).Then([](const FVoxelVolumeSculptSave& Save)
{
    // Persistent save to disk
});
```

---

## Configuration Comparison

### Volume Sculpt Configuration

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `Scale` | float | 100 | Internal voxel size (cm) |
| `bUseFastDistances` | bool | false | 8-bit vs 32-bit precision |
| `bEnableDiffing` | bool | true | Track incremental changes |
| `StackOverride` | UVoxelLayerStack* | nullptr | Custom query stack |

### Height Sculpt Configuration

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `ScaleXY` | float | 100 | Horizontal resolution (cm) |
| `bRelativeHeight` | bool | false | Store relative to base terrain |
| `StackOverride` | UVoxelLayerStack* | nullptr | Custom query stack |

---

## Memory Management

### Cache Clearing

```cpp
// Clear cache to free memory (subsequent edits may be slower)
SculptActor->ClearSculptCache();

// Clear all data (removes all sculpt edits)
SculptActor->ClearSculptData();
```

### Memory Optimization Tips

1. **Use Compression**: Always compress for disk saves
2. **Clear Cache Periodically**: Call `ClearSculptCache()` after heavy editing sessions
3. **Use External Assets**: For large terrain, external assets keep level files small
4. **bUseFastDistances**: Enable for memory-constrained scenarios
5. **Limit Edit Radius**: Smaller edit radii generate less data

---

## Editor vs Runtime Differences

### Editor Behavior

- Changes auto-save when using external assets
- Undo/Redo support via standard UE transaction system
- Level save includes inline sculpt data
- Property changes trigger OnPropertyChanged delegate

### Runtime Behavior

- Manual save/load required for persistence
- No automatic undo/redo
- External assets still work but require explicit save to disk
- Memory management is application responsibility

### Editor-Only Warning

In the Editor, `GuidToBulkData.Reset()` may clear undo buffer data unexpectedly. Always test save/load thoroughly in both Editor and packaged builds.

---

## Common Patterns

### Multiple Save Slots

```cpp
void AMyGameMode::SaveToSlot(int32 SlotIndex)
{
    FString SlotName = FString::Printf(TEXT("TerrainSlot_%d"), SlotIndex);

    VolumeSculpt->GetSave(true).Then([SlotName](const FVoxelVolumeSculptSave& Save)
    {
        UMyTerrainSaveGame* SaveGame = NewObject<UMyTerrainSaveGame>();
        SaveGame->VolumeSculptData = Save;
        UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, 0);
    });
}

void AMyGameMode::LoadFromSlot(int32 SlotIndex)
{
    FString SlotName = FString::Printf(TEXT("TerrainSlot_%d"), SlotIndex);

    if (UMyTerrainSaveGame* SaveGame = Cast<UMyTerrainSaveGame>(
        UGameplayStatics::LoadGameFromSlot(SlotName, 0)))
    {
        if (SaveGame->VolumeSculptData.IsValid())
        {
            VolumeSculpt->LoadFromSave(SaveGame->VolumeSculptData);
        }
    }
}
```

### Server-Side Storage

```cpp
void AMyGameMode::SaveToServer(AVoxelVolumeSculptActor* SculptActor)
{
    SculptActor->GetSave(true).Then([this](const FVoxelVolumeSculptSave& Save)
    {
        if (Save.IsValid())
        {
            // Convert to byte array for network transmission
            TArray<uint8> RawData;
            FMemoryWriter Writer(RawData);
            const_cast<FVoxelVolumeSculptSave&>(Save).Serialize(Writer);

            // Send to server
            SendToServer(RawData);
        }
    });
}

void AMyGameMode::LoadFromServer(AVoxelVolumeSculptActor* SculptActor, const TArray<uint8>& RawData)
{
    // Deserialize
    FVoxelVolumeSculptSave Save;
    FMemoryReader Reader(RawData);
    Save.Serialize(Reader);

    if (Save.IsValid())
    {
        SculptActor->LoadFromSave(Save);
    }
}
```

---

## Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| Save data is empty | Sculpt actor has no edits | Check `IsValid()` before saving |
| Load has no effect | Data incompatible | Verify Scale/bUseFastDistances match |
| Large file size | Not using compression | Enable `bCompress = true` |
| Slow save/load | Large terrain, uncompressed | Use compression, consider chunking |
| Changes lost on play | No persistence implemented | Implement save/load system |
| Editor changes lost | External asset not saved | File > Save All or Ctrl+Shift+S |
| Memory growth | Cache accumulation | Call `ClearSculptCache()` periodically |

---

## Related Topics

- [08-runtime-sculpting.md](08-runtime-sculpting.md) - Sculpting operations
- [06-volume-layer-system.md](06-volume-layer-system.md) - Volume terrain
- [05-height-layer-system.md](05-height-layer-system.md) - Height terrain
- [07-stamp-system.md](07-stamp-system.md) - Stamp architecture
