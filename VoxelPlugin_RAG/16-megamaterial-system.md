# MegaMaterial System

## Summary

The MegaMaterial system provides unified multi-material terrain rendering using `UVoxelMegaMaterial`. It combines multiple surface types into a single material that handles blending, Nanite displacement, Lumen GI, and performance-optimized rendering. Surface types define individual materials with blend properties.

---

## Key Classes

| Class | Header | Purpose |
|-------|--------|---------|
| `UVoxelMegaMaterial` | VoxelMegaMaterial.h | Main multi-material asset |
| `UVoxelSurfaceTypeAsset` | VoxelSurfaceTypeAsset.h | Individual surface definition |
| `UVoxelSurfaceTypeInterface` | VoxelSurfaceTypeInterface.h | Base class for surface types |
| `UVoxelSurfaceTypeGraph` | VoxelSurfaceTypeGraph.h | Graph-based smart surface |
| `FVoxelMegaMaterialProxy` | VoxelMegaMaterialProxy.h | Runtime proxy |
| `FVoxelSurfaceTypeBlend` | VoxelSurfaceTypeBlend.h | Multi-layer surface blend |
| `UVoxelMegaMaterialGeneratedData` | VoxelMegaMaterialGeneratedData.h | Generated material data |
| `UVoxelMegaMaterialCache` | VoxelMegaMaterialCache.h | Editor cache management |
| `UMaterialExpressionVoxelMegaMaterialSwitch` | MaterialExpressionVoxelMegaMaterialSwitch.h | Material expression node |

---

## Architecture Overview

```
[UVoxelMegaMaterial]
    │
    ├── SurfaceTypes[]
    │       ├── UVoxelSurfaceTypeAsset (Grass)
    │       │       └── UMaterialInterface
    │       ├── UVoxelSurfaceTypeAsset (Rock)
    │       │       └── UMaterialInterface
    │       └── UVoxelSurfaceTypeAsset (Sand)
    │               └── UMaterialInterface
    │
    ├── Generated Materials (per target)
    │       ├── NonNanite Material
    │       ├── NaniteDisplacement Material
    │       ├── NaniteWPO Material
    │       └── Lumen Material
    │
    └── FVoxelMegaMaterialProxy (Runtime)
            └── Maps SurfaceType → RenderIndex → Material
```

---

## UVoxelMegaMaterial

Main asset combining multiple surface types into unified terrain rendering.

```cpp
UCLASS(BlueprintType, meta = (VoxelAssetType, AssetColor=Green))
class VOXEL_API UVoxelMegaMaterial : public UVoxelAsset
{
    GENERATED_BODY()

public:
    // Detection for surfaces not in array
    UPROPERTY(EditAnywhere, Category = "Surfaces")
    bool bDetectNewSurfaces = true;

    // Legacy material array (deprecated)
    UPROPERTY()
    TArray<TObjectPtr<UMaterialInterface>> Materials;

    // Surface type definitions
    UPROPERTY(EditAnywhere, Category = "Surfaces")
    TArray<TObjectPtr<UVoxelSurfaceTypeAsset>> SurfaceTypes;

    // Post-process function for all surfaces
    UPROPERTY(EditAnywhere, Category = "Global")
    TObjectPtr<UMaterialFunction> AttributePostProcess;

    // Non-Nanite material settings
    UPROPERTY(EditAnywhere, Category = "Non-Nanite")
    EVoxelMegaMaterialGenerationType NonNaniteMaterialType;

    UPROPERTY(EditAnywhere, Category = "Non-Nanite",
        meta = (EditCondition = "NonNaniteMaterialType == Custom"))
    TObjectPtr<UMaterialInterface> CustomNonNaniteMaterial;

    // Nanite displacement settings
    UPROPERTY(EditAnywhere, Category = "Nanite")
    EVoxelMegaMaterialGenerationType NaniteDisplacementMaterialType;

    UPROPERTY(EditAnywhere, Category = "Nanite",
        meta = (EditCondition = "NaniteDisplacementMaterialType == Custom"))
    TObjectPtr<UMaterialInterface> CustomNaniteDisplacementMaterial;

    // Lumen material settings
    UPROPERTY(EditAnywhere, Category = "Lumen")
    EVoxelMegaMaterialGenerationType LumenMaterialType;

    UPROPERTY(EditAnywhere, Category = "Lumen",
        meta = (EditCondition = "LumenMaterialType == Custom"))
    TObjectPtr<UMaterialInterface> CustomLumenMaterial;

    // Blend settings
    UPROPERTY(EditAnywhere, Category = "Misc")
    bool bEnableSmoothBlends = true;

    UPROPERTY(EditAnywhere, Category = "Misc")
    bool bEnableDitherNoiseTexture = false;

    UPROPERTY(EditAnywhere, Category = "Misc", AdvancedDisplay)
    TSoftObjectPtr<UTexture2D> DitherNoiseTexture;

    // Rendering options
    UPROPERTY(EditAnywhere, Category = "Misc", AdvancedDisplay)
    bool bGenerateMaskedMaterial = false;

    UPROPERTY(EditAnywhere, Category = "Misc", AdvancedDisplay)
    bool bGenerateTwoSidedMaterial = false;

    UPROPERTY(EditAnywhere, Category = "Misc", AdvancedDisplay)
    bool bSetHasPixelAnimation = false;

    UPROPERTY(EditAnywhere, Category = "Misc", AdvancedDisplay)
    bool bEnablePixelDepthOffset = false;

    UPROPERTY(EditAnywhere, Category = "Misc", AdvancedDisplay)
    TObjectPtr<UMaterialInterface> CustomOutputsMaterial;
};
```

### Generation Type

```cpp
UENUM()
enum class EVoxelMegaMaterialGenerationType
{
    Custom,     // Use custom material
    Generated   // Auto-generate from surface types
};
```

---

## EVoxelMegaMaterialTarget

Render targets for different rendering paths.

```cpp
UENUM()
enum class EVoxelMegaMaterialTarget : uint8
{
    NonNanite,              // Standard mesh rendering
    NaniteWPO,              // Nanite with World Position Offset
    NaniteDisplacement,     // Nanite displacement maps
    NaniteMaterialSelection,// Nanite material selection
    Lumen                   // Lumen global illumination
};
```

### Target Usage

| Target | Use Case | Notes |
|--------|----------|-------|
| `NonNanite` | Standard meshes | Default for non-Nanite rendering |
| `NaniteWPO` | Nanite + vertex animation | Displacement via WPO |
| `NaniteDisplacement` | Nanite tessellation | Hardware displacement |
| `NaniteMaterialSelection` | Material ID selection | Per-cluster materials |
| `Lumen` | Global illumination | Simplified for GI |

---

## UVoxelSurfaceTypeAsset

Individual surface type with material and blend properties.

```cpp
UCLASS(meta = (VoxelAssetType, AssetColor=Green))
class VOXEL_API UVoxelSurfaceTypeAsset : public UVoxelSurfaceTypeInterface
{
    GENERATED_BODY()

public:
    // Material for this surface
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    TObjectPtr<UMaterialInterface> Material;

    // Create holes in terrain
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bInvisible = false;

    // Blend smoothness (0-1)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config",
        meta = (UIMin = 0, UIMax = 1, ClampMin = 0))
    float BlendSmoothness = 0.5f;

    // Random seed for variations
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Config")
    FVoxelExposedSeed Seed;
};
```

### Surface Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `Material` | `UMaterialInterface*` | - | Base material |
| `bInvisible` | `bool` | false | Invisible surface (holes) |
| `BlendSmoothness` | `float` | 0.5 | Dither blend smoothness |
| `Seed` | `FVoxelExposedSeed` | - | Variation seed |

---

## FVoxelSurfaceTypeBlend

Multi-layer surface blend for smooth transitions.

```cpp
struct VOXEL_API FVoxelSurfaceTypeBlend
{
    static constexpr int32 MaxLayers = 15;

    // Get active layers
    TConstVoxelArrayView<FVoxelSurfaceTypeBlendLayer> GetLayers() const;

    // Check if null/empty
    bool IsNull() const;

    // Get dominant layer
    FVoxelSurfaceTypeBlendLayer GetTopLayer() const;

    // Get sorted by weight
    TVoxelArray<FVoxelSurfaceTypeBlendLayer> GetLayersSortedByWeight() const;

    // Create from single type
    static FVoxelSurfaceTypeBlend FromType(const FVoxelSurfaceType Type);

    // Interpolation
    static void Lerp(
        FVoxelSurfaceTypeBlend& OutResult,
        const FVoxelSurfaceTypeBlend& BlendA,
        const FVoxelSurfaceTypeBlend& BlendB,
        float Alpha);

    static void BilinearInterpolation(
        FVoxelSurfaceTypeBlend& OutResult,
        TConstVoxelArrayView<FVoxelSurfaceTypeBlend> Blends,
        float AlphaX,
        float AlphaY);

    static void TrilinearInterpolation(
        FVoxelSurfaceTypeBlend& OutResult,
        TConstVoxelArrayView<FVoxelSurfaceTypeBlend> Blends,
        float AlphaX,
        float AlphaY,
        float AlphaZ);
};
```

### FVoxelSurfaceTypeBlendLayer

```cpp
struct VOXEL_API FVoxelSurfaceTypeBlendLayer
{
    FVoxelSurfaceType Type;            // Surface type index
    FVoxelSurfaceTypeBlendWeight Weight; // Blend weight (0-1)
};
```

---

## FVoxelMegaMaterialProxy

Runtime proxy for material management.

```cpp
class VOXEL_API FVoxelMegaMaterialProxy
{
public:
    const TVoxelObjectPtr<UVoxelMegaMaterial> WeakMegaMaterial;
    const bool bDetectNewSurfaces;

    // Get default proxy
    static TSharedRef<FVoxelMegaMaterialProxy> Default();

    // Material mapping
    const TVoxelMap<FVoxelMaterialRenderIndex, TVoxelObjectPtr<UMaterialInterface>>&
        GetMaterialIndexToMaterial() const;

    // Metadata mapping
    TConstVoxelArrayView<FVoxelMetadataRef> GetMetadataIndexToMetadata() const;

    // Get target-specific material
    TVoxelObjectPtr<UMaterialInterface> GetTargetMaterial(
        EVoxelMegaMaterialTarget Target) const;

    // Surface type to render index
    FVoxelMaterialRenderIndex GetRenderIndex(FVoxelSurfaceType SurfaceType) const;

    // Get metadata for render index
    TConstVoxelArrayView<FVoxelMetadataRef> GetUsedMetadatas(
        FVoxelMaterialRenderIndex RenderIndex) const;

    // Convert blends to render materials
    TVoxelArray<FVoxelRenderMaterial> GetRenderMaterials(
        TConstVoxelArrayView<FVoxelSurfaceTypeBlend> SurfaceTypes) const;
};
```

---

## UVoxelSurfaceTypeGraph

Graph-based smart surface types with procedural logic.

```cpp
UCLASS(BlueprintType, meta = (AssetSubMenu = "Graph"))
class VOXEL_API UVoxelSurfaceTypeGraph : public UVoxelGraph
{
    GENERATED_BODY()

public:
    UVoxelSmartSurfaceType* GetPreviewSurface();

#if WITH_EDITORONLY_DATA
    UPROPERTY()
    EVoxelSmartSurfacePreviewShape PreviewShape = EVoxelSmartSurfacePreviewShape::Sphere;
#endif
};
```

### Creating Smart Surface

1. Content Browser > Right-click
2. **Voxel > Surface Type Graph**
3. Build graph logic for surface selection
4. Use in MegaMaterial or directly

---

## Material Expression

### UMaterialExpressionVoxelMegaMaterialSwitch

Switch between regular and MegaMaterial rendering.

```cpp
UCLASS()
class VOXEL_API UMaterialExpressionVoxelMegaMaterialSwitch : public UMaterialExpression
{
    GENERATED_BODY()

public:
    // Used when rendering as regular material
    UPROPERTY(meta = (ToolTip = "Input for regular material"))
    FExpressionInput Default;

    // Used when rendering through MegaMaterial
    UPROPERTY(meta = (ToolTip = "Input for mega material"))
    FExpressionInput Voxel;

    UPROPERTY()
    bool bIsMegaMaterial = false;
};
```

### Usage in Material Graph

```
[Texture Sample: Ground]
    │
    └──► [MegaMaterial Switch]
              Default: (texture output)
              Voxel: (voxel-specific logic)
              │
              └──► [Material Output]
```

---

## Generated Data

### UVoxelMegaMaterialGeneratedData

Stores auto-generated materials for each target.

```cpp
UCLASS()
class VOXEL_API UVoxelMegaMaterialGeneratedData : public UObject
{
    GENERATED_BODY()

public:
    // Surface info per render index
    UPROPERTY()
    TMap<FVoxelMaterialRenderIndex, FVoxelMegaMaterialSurfaceInfo> IndexToSurfaceInfo;

    // Generated materials per render index
    UPROPERTY()
    TMap<FVoxelMaterialRenderIndex, FVoxelMegaMaterialGeneratedMaterial>
        IndexToGeneratedMaterial;

    // Metadata array
    UPROPERTY()
    TArray<TObjectPtr<UVoxelMetadata>> MetadataIndexToMetadata;

    // Target-specific materials
    UPROPERTY()
    TMap<EVoxelMegaMaterialTarget, TObjectPtr<UMaterialInterface>> TargetToMaterial;

#if WITH_EDITOR
    void ForceRebuild();
    void QueueRebuild(bool bInteractive = false);
    void RebuildNow(bool bInteractive = false);
#endif
};
```

### FVoxelMegaMaterialSurfaceInfo

```cpp
USTRUCT()
struct FVoxelMegaMaterialSurfaceInfo
{
    UPROPERTY()
    TObjectPtr<UVoxelSurfaceTypeAsset> SurfaceType;

    UPROPERTY()
    TArray<TObjectPtr<UVoxelMetadata>> UsedMetadatas;
};
```

---

## Setup Workflow

### Creating MegaMaterial

1. **Create Surface Types**
   - Content Browser > Voxel > Surface Type Asset
   - Assign materials and set blend properties
   - Create for each terrain type (grass, rock, sand, etc.)

2. **Create MegaMaterial**
   - Content Browser > Voxel > MegaMaterial
   - Add surface types to SurfaceTypes array
   - Configure generation settings

3. **Configure Rendering**
   - Set NonNaniteMaterialType
   - Set NaniteDisplacementMaterialType
   - Set LumenMaterialType
   - Enable smooth blends if needed

4. **Assign to VoxelWorld**
   - In AVoxelWorld Details
   - Set MegaMaterial property
   - Or configure in LayerStack

### Surface Type Setup

```
UVoxelSurfaceTypeAsset: Grass
├── Material: M_Grass
├── bInvisible: false
├── BlendSmoothness: 0.5
└── Seed: 12345

UVoxelSurfaceTypeAsset: Rock
├── Material: M_Rock
├── bInvisible: false
├── BlendSmoothness: 0.3
└── Seed: 67890
```

---

## C++ Usage Examples

### Creating MegaMaterial Programmatically

```cpp
void AMyActor::CreateMegaMaterial()
{
    // Create MegaMaterial asset
    UVoxelMegaMaterial* MegaMaterial = NewObject<UVoxelMegaMaterial>(
        GetTransientPackage(),
        NAME_None,
        RF_Transient);

    // Load surface types
    UVoxelSurfaceTypeAsset* GrassSurface = LoadObject<UVoxelSurfaceTypeAsset>(
        nullptr, TEXT("/Game/Voxel/Surfaces/ST_Grass"));
    UVoxelSurfaceTypeAsset* RockSurface = LoadObject<UVoxelSurfaceTypeAsset>(
        nullptr, TEXT("/Game/Voxel/Surfaces/ST_Rock"));

    // Add surfaces
    MegaMaterial->SurfaceTypes.Add(GrassSurface);
    MegaMaterial->SurfaceTypes.Add(RockSurface);

    // Configure
    MegaMaterial->bEnableSmoothBlends = true;
    MegaMaterial->NonNaniteMaterialType = EVoxelMegaMaterialGenerationType::Generated;
    MegaMaterial->NaniteDisplacementMaterialType = EVoxelMegaMaterialGenerationType::Generated;
}
```

### Getting Proxy at Runtime

```cpp
void AMyActor::ProcessMegaMaterial(UVoxelMegaMaterial* MegaMaterial)
{
    if (!MegaMaterial)
    {
        return;
    }

    // Get runtime proxy
    TSharedRef<FVoxelMegaMaterialProxy> Proxy = MegaMaterial->GetProxy();

    // Get material for surface type
    FVoxelSurfaceType GrassType = /* get from somewhere */;
    FVoxelMaterialRenderIndex RenderIndex = Proxy->GetRenderIndex(GrassType);

    // Get actual material
    const auto& MaterialMap = Proxy->GetMaterialIndexToMaterial();
    UMaterialInterface* Material = MaterialMap.FindRef(RenderIndex).Get();

    // Use material...
}
```

### Working with Surface Blends

```cpp
void ProcessSurfaceBlend(const FVoxelSurfaceTypeBlend& Blend)
{
    if (Blend.IsNull())
    {
        return;
    }

    // Get layers
    for (const FVoxelSurfaceTypeBlendLayer& Layer : Blend.GetLayers())
    {
        FVoxelSurfaceType Type = Layer.Type;
        float Weight = Layer.Weight.ToFloat();

        UE_LOG(LogTemp, Log, TEXT("Surface: %s, Weight: %.2f"),
            *Layer.GetSurfaceName(), Weight);
    }

    // Get dominant surface
    FVoxelSurfaceTypeBlendLayer Top = Blend.GetTopLayer();
}
```

### Interpolating Blends

```cpp
FVoxelSurfaceTypeBlend LerpBlends(
    const FVoxelSurfaceTypeBlend& A,
    const FVoxelSurfaceTypeBlend& B,
    float Alpha)
{
    FVoxelSurfaceTypeBlend Result;
    FVoxelSurfaceTypeBlend::Lerp(Result, A, B, Alpha);
    return Result;
}
```

---

## Blueprint Usage

### Assigning MegaMaterial

1. Select `AVoxelWorld` actor
2. In Details panel > Rendering
3. Set **MegaMaterial** property
4. Materials auto-generate on save

### Creating Custom Surface Types

1. Right-click Content Browser
2. **Voxel > Surface Type Asset**
3. Set **Material** property
4. Adjust **BlendSmoothness** (0-1)
5. Add to MegaMaterial

### Using in Stamp Graphs

Surface types can be assigned in:
- Height stamps (via weightmaps)
- Volume stamps (via paint operations)
- Sculpt operations (via surface parameter)

---

## Performance Optimization

### Material Generation

| Setting | Performance | Quality |
|---------|-------------|---------|
| Generated | Automatic | Good |
| Custom | Manual control | Best |

### Blend Smoothness

| Value | Effect | Cost |
|-------|--------|------|
| 0.0 | Hard edges | Lowest |
| 0.5 | Balanced | Medium |
| 1.0 | Smooth dither | Highest |

### Best Practices

1. **Limit surface count** - Each surface adds shader complexity
2. **Use appropriate targets** - Don't enable unused targets
3. **Optimize base materials** - MegaMaterial inherits complexity
4. **Disable unused features** - Turn off smooth blends if not needed

---

## Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| Missing surfaces | Not in SurfaceTypes array | Add surface type to MegaMaterial |
| Black terrain | Material not generated | Force rebuild or check errors |
| Blend seams | BlendSmoothness too low | Increase smoothness value |
| Shader errors | Complex base materials | Simplify surface materials |
| Nanite issues | Wrong target type | Check NaniteDisplacementMaterialType |
| Lumen artifacts | Custom Lumen material | Use Generated or fix custom |

---

## MegaMaterial vs Individual Materials

| Feature | MegaMaterial | Individual Materials |
|---------|--------------|---------------------|
| Setup | Single asset | Multiple materials |
| Blending | Built-in dither | Manual blending |
| Performance | Optimized | Varies |
| Flexibility | Automatic | Full control |
| Nanite support | Integrated | Manual |

### When to Use MegaMaterial

- Multiple terrain surface types
- Automatic surface blending
- Nanite terrain rendering
- Lumen GI integration

### When to Use Custom Materials

- Single surface terrain
- Specialized rendering needs
- Maximum shader control
- Legacy material systems

---

## Related Topics

- [04-voxelworld-layerstack.md](04-voxelworld-layerstack.md) - World configuration
- [05-height-layer-system.md](05-height-layer-system.md) - Height terrain surfaces
- [06-volume-layer-system.md](06-volume-layer-system.md) - Volume terrain surfaces
- [08-runtime-sculpting.md](08-runtime-sculpting.md) - Surface painting
- [13-collision-rendering.md](13-collision-rendering.md) - Nanite rendering
