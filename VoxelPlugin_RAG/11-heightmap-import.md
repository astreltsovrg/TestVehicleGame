# Heightmap Import System

## Summary

VoxelPlugin2 supports importing real-world terrain data through the `UVoxelHeightmap` asset system. This includes 16-bit PNG heightmaps, raw elevation data, and weightmaps for surface blending. The system provides both editor import tools and runtime sampling via VoxelGraph nodes.

---

## Key Classes

| Class | Header | Purpose |
|-------|--------|---------|
| `UVoxelHeightmap` | VoxelHeightmap.h | Main heightmap asset container |
| `UVoxelHeightmap_Height` | VoxelHeightmap_Height.h | Height data configuration |
| `UVoxelHeightmap_Weight` | VoxelHeightmap_Weight.h | Weight layer configuration |
| `FVoxelHeightmapImporter` | VoxelHeightmapImporter.h | Base importer class |
| `FVoxelHeightmapImporter_PNG` | VoxelHeightmapImporter.h | PNG file importer |
| `FVoxelHeightmapImporter_Raw` | VoxelHeightmapImporter.h | Raw file importer |
| `FVoxelHeightmapStamp` | VoxelHeightmapStamp.h | Stamp for heightmap terrain |
| `UVoxelHeightmapFunctionLibrary` | VoxelHeightmapFunctionLibrary.h | Graph sampling functions |
| `EVoxelHeightmapWeightType` | VoxelHeightmap_Weight.h | Weight blend mode enum |

---

## UVoxelHeightmap Asset

Main container asset for imported heightmap data.

```cpp
UCLASS(BlueprintType, meta = (VoxelAssetType, AssetColor=Red))
class VOXEL_API UVoxelHeightmap : public UVoxelAsset
{
    GENERATED_BODY()

public:
    // Horizontal scale (cm per pixel)
    UPROPERTY(EditAnywhere, Category = "Config", meta = (Units = cm))
    float ScaleXY = 100;

    // Default surface type when no weightmap applies
    UPROPERTY(EditAnywhere, Category = "Config")
    TObjectPtr<UVoxelSurfaceTypeInterface> DefaultSurfaceType;

    // Height data configuration
    UPROPERTY(VisibleAnywhere, Category = "Height", Instanced)
    TObjectPtr<UVoxelHeightmap_Height> Height;

    // Weight layers for surface blending
    UPROPERTY(VisibleAnywhere, Category = "Weight", Instanced)
    TArray<TObjectPtr<UVoxelHeightmap_Weight>> Weights;

public:
    FIntPoint GetSize() const;
    FVoxelBox GetBounds() const;
};
```

### Asset Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `ScaleXY` | `float` | 100 | Horizontal scale in cm per pixel |
| `DefaultSurfaceType` | `UVoxelSurfaceTypeInterface*` | nullptr | Fallback surface type |
| `Height` | `UVoxelHeightmap_Height*` | - | Height data sub-object |
| `Weights` | `TArray<UVoxelHeightmap_Weight*>` | - | Weight layer sub-objects |

---

## UVoxelHeightmap_Height

Configuration for height data within a heightmap asset.

```cpp
UCLASS(Within=VoxelHeightmap)
class VOXEL_API UVoxelHeightmap_Height : public UObject
{
    GENERATED_BODY()

public:
    // Source texture for height data
    UPROPERTY(EditAnywhere, Category = "Config")
    TSoftObjectPtr<UTexture2D> Texture;

    // Which channel to read from texture
    UPROPERTY(EditAnywhere, Category = "Config")
    EVoxelTextureChannel TextureChannel = EVoxelTextureChannel::R;

    // Compress float heights to 16-bit (saves memory)
    UPROPERTY(EditAnywhere, Category = "Config")
    bool bCompressTo16Bits = false;

    // Vertical scale (max height range in cm)
    UPROPERTY(EditAnywhere, Category = "Config", meta = (Units = cm))
    float ScaleZ = 64000;

    // Vertical offset in cm
    UPROPERTY(EditAnywhere, Category = "Config", meta = (Units = cm))
    float OffsetZ = 0;

    // Use bicubic interpolation for smoother sampling
    UPROPERTY(EditAnywhere, Category = "Config")
    bool bUseBicubic = true;

public:
    // Set height data from 16-bit unsigned values
    void SetHeights(
        int32 SizeX,
        int32 SizeY,
        TConstVoxelArrayView<uint16> Heights,
        bool bCompress = false);

    // Set height data from float values
    void SetHeights(
        int32 SizeX,
        int32 SizeY,
        TConstVoxelArrayView<float> Heights,
        bool bCompress = false);

    // Query methods
    FIntPoint GetSize() const;
    float GetHeight(const FVector2D& Position, bool bUseBicubic) const;
};
```

### Height Configuration

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `Texture` | `TSoftObjectPtr<UTexture2D>` | - | Source height texture |
| `TextureChannel` | `EVoxelTextureChannel` | R | Channel to sample (R, G, B, A) |
| `bCompressTo16Bits` | `bool` | false | Compress to save memory |
| `ScaleZ` | `float` | 64000 | Vertical range (640m default) |
| `OffsetZ` | `float` | 0 | Vertical offset |
| `bUseBicubic` | `bool` | true | Bicubic interpolation |

### Height Data Formula

```
WorldHeight = (NormalizedValue * ScaleZ) + OffsetZ

Where:
- NormalizedValue = PixelValue / 65535 (for 16-bit)
- ScaleZ = vertical range in cm
- OffsetZ = base elevation in cm
```

### Interpolation Modes

| Mode | Quality | Performance | Use Case |
|------|---------|-------------|----------|
| **Bilinear** (`bUseBicubic = false`) | Good | Fast | Real-time sampling |
| **Bicubic** (`bUseBicubic = true`) | Excellent | Slower | Static terrain |

---

## UVoxelHeightmap_Weight

Configuration for weight/splatmap layers.

```cpp
UCLASS(Within=VoxelHeightmap)
class VOXEL_API UVoxelHeightmap_Weight : public UObject
{
    GENERATED_BODY()

public:
    // Source texture for weight data
    UPROPERTY(EditAnywhere, Category = "Config")
    TSoftObjectPtr<UTexture2D> Texture;

    // Which channel to read
    UPROPERTY(EditAnywhere, Category = "Config")
    EVoxelTextureChannel TextureChannel = EVoxelTextureChannel::R;

    // How weights blend with other layers
    UPROPERTY(EditAnywhere, Category = "Config")
    EVoxelHeightmapWeightType Type = EVoxelHeightmapWeightType::AlphaBlended;

public:
    FIntPoint GetSize() const;
    float GetWeight(const FVector2D& Position) const;
};
```

### EVoxelHeightmapWeightType

```cpp
UENUM(BlueprintType)
enum class EVoxelHeightmapWeightType : uint8
{
    // Linearly interpolated one after the other
    // Each layer's alpha blends over previous layers
    AlphaBlended,

    // Weights normalized together
    // All weights sum to 1.0 (like Landscape layers)
    WeightBlended
};
```

### Weight Type Comparison

| Type | Formula | Use Case |
|------|---------|----------|
| **AlphaBlended** | `Result = Lerp(Previous, Current, Weight)` | Decal-like overlays |
| **WeightBlended** | `Result = Σ(Layer * NormalizedWeight)` | Standard terrain splatmaps |

---

## FVoxelHeightmapImporter

Base class for importing heightmap data from external files.

```cpp
class VOXELCORE_API FVoxelHeightmapImporter
{
public:
    virtual ~FVoxelHeightmapImporter() = default;

    // Factory method - creates appropriate importer for file type
    static TSharedPtr<FVoxelHeightmapImporter> MakeImporter(const FString& Path);

    // Import raw data from file
    static bool Import(
        const FString& Path,
        FString& OutError,
        FIntPoint& OutSize,
        int32& OutBitDepth,
        TArray64<uint8>& OutData);

    // Query file dimensions
    virtual FIntPoint GetSize() const = 0;

    // Query bit depth (8 or 16)
    virtual int32 GetBitDepth() const = 0;

    // Read raw pixel data
    virtual bool GetData(TArray64<uint8>& OutData) const = 0;
};
```

### PNG Importer

```cpp
class VOXELCORE_API FVoxelHeightmapImporter_PNG : public FVoxelHeightmapImporter
{
public:
    explicit FVoxelHeightmapImporter_PNG(const FString& Path);

    // Supports 8-bit and 16-bit PNG files
    virtual FIntPoint GetSize() const override;
    virtual int32 GetBitDepth() const override;
    virtual bool GetData(TArray64<uint8>& OutData) const override;
};
```

### Raw Importer

```cpp
class VOXELCORE_API FVoxelHeightmapImporter_Raw : public FVoxelHeightmapImporter
{
public:
    // Raw files require explicit dimensions
    FVoxelHeightmapImporter_Raw(
        const FString& Path,
        int32 SizeX,
        int32 SizeY,
        int32 BitDepth);

    virtual FIntPoint GetSize() const override;
    virtual int32 GetBitDepth() const override;
    virtual bool GetData(TArray64<uint8>& OutData) const override;
};
```

### Supported File Formats

| Format | Extension | Bit Depth | Notes |
|--------|-----------|-----------|-------|
| PNG | .png | 8 or 16 | Auto-detected |
| Raw | .raw, .r16 | 8 or 16 | Requires manual size |

---

## FVoxelHeightmapStamp

Stamp class for applying heightmap data to terrain.

```cpp
USTRUCT(meta = (ShortName = "Heightmap", Icon = "LandscapeEditor.Target_Heightmap"))
struct VOXEL_API FVoxelHeightmapStamp : public FVoxelHeightStamp
{
    GENERATED_BODY()

public:
    // The heightmap asset to use
    UPROPERTY(EditAnywhere, Category = "Config")
    TObjectPtr<UVoxelHeightmap> Heightmap;

    // Default surface when no weightmap applies
    UPROPERTY(EditAnywhere, Category = "Config")
    TObjectPtr<UVoxelSurfaceTypeInterface> DefaultSurfaceType;

    // Weightmap-to-surface type mappings
    UPROPERTY(EditAnywhere, Category = "Config")
    TArray<FVoxelHeightmapStampWeightmapSurfaceType> WeightmapSurfaceTypes;
};
```

### FVoxelHeightmapStampWeightmapSurfaceType

Maps weightmap indices to surface types.

```cpp
USTRUCT(BlueprintType)
struct FVoxelHeightmapStampWeightmapSurfaceType
{
    GENERATED_BODY()

    // Index of the weightmap in UVoxelHeightmap.Weights array
    UPROPERTY(EditAnywhere, Category = "Voxel", meta = (ClampMin = 0))
    int32 Index = 0;

    // Surface type to apply where this weight is dominant
    UPROPERTY(EditAnywhere, Category = "Voxel")
    TObjectPtr<UVoxelSurfaceTypeInterface> SurfaceType;
};
```

---

## UVoxelHeightmapFunctionLibrary

Graph functions for sampling heightmaps in VoxelGraph.

```cpp
UCLASS()
class VOXEL_API UVoxelHeightmapFunctionLibrary : public UVoxelFunctionLibrary
{
    GENERATED_BODY()

public:
    // Sample height values from heightmap at given positions
    UFUNCTION(Category = "Heightmap")
    FVoxelFloatBuffer SampleHeightmap(
        const FVoxelHeightmapRef& Heightmap,
        const FVoxelVector2DBuffer& Position,
        bool bUseBicubic = true) const;
};
```

### FVoxelHeightmapRef

Reference to a heightmap asset for graph nodes.

```cpp
USTRUCT(BlueprintType)
struct VOXEL_API FVoxelHeightmapRef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Voxel")
    TObjectPtr<UVoxelHeightmap> Heightmap;
};
```

---

## Import Workflow

### Editor Import (Recommended)

1. **Create Heightmap Asset**
   - Content Browser > Right-click
   - Select **Voxel > Heightmap**
   - Name: `HM_MyTerrain`

2. **Import Height Data**
   - Open heightmap asset
   - In Height section, click **Import**
   - Select PNG or Raw file
   - Configure ScaleZ and OffsetZ

3. **Import Weight Layers (Optional)**
   - In Weights section, click **Add**
   - Import splatmap textures
   - Set weight type (Alpha or Weight blended)

4. **Create Stamp**
   - Place Heightmap Stamp actor in level
   - Assign heightmap asset
   - Configure surface type mappings

### C++ Programmatic Import

```cpp
void AMyActor::ImportHeightmapFromFile()
{
    FString FilePath = TEXT("C:/Terrain/heightmap.png");
    FString Error;
    FIntPoint Size;
    int32 BitDepth;
    TArray64<uint8> RawData;

    // Import raw data
    if (!FVoxelHeightmapImporter::Import(FilePath, Error, Size, BitDepth, RawData))
    {
        UE_LOG(LogTemp, Error, TEXT("Import failed: %s"), *Error);
        return;
    }

    // Create heightmap asset
    UVoxelHeightmap* Heightmap = NewObject<UVoxelHeightmap>(
        GetTransientPackage(),
        NAME_None,
        RF_Transient);

    // Configure horizontal scale (1m per pixel)
    Heightmap->ScaleXY = 100.f;

    // Convert raw data to heights
    if (BitDepth == 16)
    {
        TArray<uint16> Heights;
        Heights.SetNumUninitialized(Size.X * Size.Y);
        FMemory::Memcpy(Heights.GetData(), RawData.GetData(), RawData.Num());

        Heightmap->Height->SetHeights(Size.X, Size.Y, Heights);
    }

    // Configure vertical scale
    Heightmap->Height->ScaleZ = 100000.f;  // 1km range
    Heightmap->Height->OffsetZ = -50000.f; // Center at 0
}
```

### Creating Heightmap Stamp at Runtime

```cpp
void AMyActor::CreateHeightmapTerrain()
{
    // Load heightmap asset
    UVoxelHeightmap* Heightmap = LoadObject<UVoxelHeightmap>(
        nullptr, TEXT("/Game/Terrain/HM_RealWorld"));

    if (!Heightmap) return;

    // Create stamp actor
    FActorSpawnParameters Params;
    AVoxelStampActor* StampActor = GetWorld()->SpawnActor<AVoxelStampActor>(
        AVoxelStampActor::StaticClass(),
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        Params);

    // Configure heightmap stamp
    UVoxelStampComponent* StampComp = StampActor->GetStampComponent();
    FVoxelHeightmapStamp& Stamp = StampComp->CreateStamp<FVoxelHeightmapStamp>();

    Stamp.Layer = MyHeightLayer;
    Stamp.Heightmap = Heightmap;
    Stamp.BlendMode = EVoxelHeightBlendMode::Override;

    // Map weightmaps to surface types
    FVoxelHeightmapStampWeightmapSurfaceType GrassMapping;
    GrassMapping.Index = 0;
    GrassMapping.SurfaceType = GrassSurface;
    Stamp.WeightmapSurfaceTypes.Add(GrassMapping);

    FVoxelHeightmapStampWeightmapSurfaceType RockMapping;
    RockMapping.Index = 1;
    RockMapping.SurfaceType = RockSurface;
    Stamp.WeightmapSurfaceTypes.Add(RockMapping);

    // Register with world
    StampComp->RegisterStamp();
}
```

---

## VoxelGraph Integration

### Sampling Heightmap in Graph

```
[Make Heightmap Ref (HM_RealWorld)]
    │
    └──► [Sample Heightmap]
              Position: [Get Position 2D]
              bUseBicubic: true
              │
              └──► [Output Height]
```

### Blending Heightmap with Noise

```
[Make Heightmap Ref]
    │
    └──► [Sample Heightmap] ──────────────┐
              Position: [Get Position 2D] │
                                          │
[Make Seed (12345)]                       │
    │                                     │
    └──► [Advanced Noise 2D] ─────────────┤
              Position: [Get Position 2D] │
              Amplitude: 5000             │
                                          │
                                     [Float +]
                                          │
                                          └──► [Output Height]
```

This adds procedural noise detail on top of imported heightmap data.

---

## Real-World Data Sources

### Common Height Data Formats

| Source | Resolution | Coverage | Format |
|--------|------------|----------|--------|
| **USGS 3DEP** | 1m - 30m | USA | GeoTIFF |
| **SRTM** | 30m - 90m | Global (60°N-56°S) | HGT |
| **ASTER GDEM** | 30m | Global | GeoTIFF |
| **Copernicus DEM** | 30m | Global | GeoTIFF |
| **LIDAR** | <1m | Local scans | LAS/LAZ |

### Conversion to PNG

Real-world elevation data typically requires conversion:

```python
# Example: Convert GeoTIFF to 16-bit PNG
import numpy as np
from PIL import Image
import rasterio

# Read GeoTIFF
with rasterio.open('elevation.tif') as src:
    data = src.read(1)  # First band

# Normalize to 0-65535 range
min_val, max_val = data.min(), data.max()
normalized = ((data - min_val) / (max_val - min_val) * 65535).astype(np.uint16)

# Save as 16-bit PNG
Image.fromarray(normalized).save('heightmap.png')

# Record scale for UE import
print(f"ScaleZ = {max_val - min_val} meters")
print(f"OffsetZ = {min_val} meters")
```

### Scale Calculations

```
Given:
- Real elevation range: 0m to 1000m
- Heightmap values: 0 to 65535

UVoxelHeightmap_Height settings:
- ScaleZ = 100000 (1000m in cm)
- OffsetZ = 0

Formula verification:
- Pixel 0 → Height = (0/65535) * 100000 + 0 = 0 cm
- Pixel 65535 → Height = (65535/65535) * 100000 + 0 = 100000 cm = 1000m ✓
```

---

## Weightmap Setup

### Creating Multi-Surface Terrain

1. **Prepare Splatmaps**
   - Create RGB texture where each channel represents a surface
   - R = Grass, G = Rock, B = Sand (example)
   - Values 0-255 represent weight

2. **Import Weight Layers**
   ```
   UVoxelHeightmap
   └── Weights
       ├── [0] Grass (R channel)
       ├── [1] Rock (G channel)
       └── [2] Sand (B channel)
   ```

3. **Configure Stamp Mappings**
   ```
   FVoxelHeightmapStamp
   └── WeightmapSurfaceTypes
       ├── [0] Index: 0, SurfaceType: Grass
       ├── [1] Index: 1, SurfaceType: Rock
       └── [2] Index: 2, SurfaceType: Sand
   ```

### Weight Blend Example

```cpp
void AMyActor::SetupWeightedHeightmap()
{
    UVoxelHeightmap* Heightmap = LoadObject<UVoxelHeightmap>(...);

    // Add weight layers
    UVoxelHeightmap_Weight* GrassWeight = NewObject<UVoxelHeightmap_Weight>(Heightmap);
    GrassWeight->Texture = GrassSplatmap;
    GrassWeight->TextureChannel = EVoxelTextureChannel::R;
    GrassWeight->Type = EVoxelHeightmapWeightType::WeightBlended;

    UVoxelHeightmap_Weight* RockWeight = NewObject<UVoxelHeightmap_Weight>(Heightmap);
    RockWeight->Texture = RockSplatmap;
    RockWeight->TextureChannel = EVoxelTextureChannel::G;
    RockWeight->Type = EVoxelHeightmapWeightType::WeightBlended;

    Heightmap->Weights.Add(GrassWeight);
    Heightmap->Weights.Add(RockWeight);
}
```

---

## Performance Considerations

### Memory Usage

| Resolution | 16-bit Height | 8-bit Weight (3 layers) | Total |
|------------|---------------|-------------------------|-------|
| 1024×1024 | 2 MB | 3 MB | 5 MB |
| 2048×2048 | 8 MB | 12 MB | 20 MB |
| 4096×4096 | 32 MB | 48 MB | 80 MB |
| 8192×8192 | 128 MB | 192 MB | 320 MB |

### Optimization Tips

1. **Use bCompressTo16Bits** - Halves memory for float heights
2. **Limit weight layers** - Each layer adds memory overhead
3. **Match VoxelSize to ScaleXY** - Avoid unnecessary interpolation
4. **Tile large terrains** - Split 8K+ heightmaps into tiles

### Streaming Considerations

```cpp
// Large heightmaps should use soft references
UPROPERTY(EditAnywhere, Category = "Config")
TSoftObjectPtr<UTexture2D> Texture;  // Loads on demand

// vs hard reference (always loaded)
UPROPERTY(EditAnywhere, Category = "Config")
TObjectPtr<UTexture2D> Texture;  // Always in memory
```

---

## Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| Flat terrain | ScaleZ too small | Increase ScaleZ value |
| Inverted terrain | Negative ScaleZ needed | Set negative ScaleZ or flip in editor |
| Blocky appearance | bUseBicubic = false | Enable bicubic interpolation |
| Memory issues | Heightmap too large | Use bCompressTo16Bits or tile |
| Wrong surface types | Weightmap index mismatch | Verify Index values in stamp |
| Seams between tiles | Edge mismatch | Ensure heightmaps overlap by 1 pixel |
| Terrain offset | Wrong OffsetZ | Calculate from real-world min elevation |

---

## Blueprint Usage

### Setting Up Heightmap in Blueprint

1. **Create Heightmap Asset**
   - Content Browser > Voxel > Heightmap

2. **Place Heightmap Stamp**
   - Modes panel > Voxel > Heightmap Stamp
   - Or drag `AVoxelStampActor` and add heightmap stamp

3. **Configure in Details Panel**
   ```
   Heightmap: HM_MyTerrain
   Layer: HL_MainTerrain
   BlendMode: Override
   DefaultSurfaceType: Grass
   WeightmapSurfaceTypes:
     [0] Index: 0, SurfaceType: Grass
     [1] Index: 1, SurfaceType: Rock
   ```

### Querying Height at Runtime (Blueprint)

```
Get Voxel World
    │
    └──► Query Height
              Position: Player Location (XY)
              │
              └──► Return Value (float, cm)
```

---

## Related Topics

- [03-noise-generation.md](03-noise-generation.md) - Procedural noise for blending
- [04-voxelworld-layerstack.md](04-voxelworld-layerstack.md) - Layer configuration
- [05-height-layer-system.md](05-height-layer-system.md) - Height layer fundamentals
- [07-stamp-system.md](07-stamp-system.md) - Stamp architecture
- [16-megamaterial-system.md](16-megamaterial-system.md) - Multi-material rendering
