# Spline System

## Summary

VoxelPlugin2 provides a powerful spline-based terrain modification system for creating roads, rivers, paths, and other linear features. The system works with both height (2D) and volume (3D) terrain through `FVoxelHeightSplineStamp` and `FVoxelVolumeSplineStamp`. Each spline stamp uses a specialized VoxelGraph that receives spline context, enabling procedural generation along the spline path.

---

## Key Classes

| Class | Header | Purpose |
|-------|--------|---------|
| `UVoxelSplineComponent` | VoxelSplineComponent.h | Spline component with Voxel metadata |
| `FVoxelHeightSplineStamp` | VoxelHeightSplineStamp.h | 2D height spline stamp |
| `FVoxelVolumeSplineStamp` | VoxelVolumeSplineStamp.h | 3D volume spline stamp |
| `UVoxelHeightSplineGraph` | VoxelHeightSplineGraph.h | Graph for height spline logic |
| `UVoxelVolumeSplineGraph` | VoxelVolumeSplineGraph.h | Graph for volume spline logic |
| `UVoxelSplineMetadata` | VoxelSplineMetadata.h | Per-point spline metadata |
| `UVoxelSplineStampFunctionLibrary` | VoxelSplineStampFunctionLibrary.h | Graph functions for spline queries |
| `FVoxelSplineParameter` | VoxelSplineParameters.h | Spline parameter definitions |

---

## UVoxelSplineComponent

Extended spline component with Voxel metadata support.

```cpp
UCLASS(meta = (BlueprintSpawnableComponent))
class VOXEL_API UVoxelSplineComponent : public USplineComponent
{
    GENERATED_BODY()

public:
    // Voxel-specific metadata for spline points
    UPROPERTY()
    TObjectPtr<UVoxelSplineMetadata> Metadata;

    // Implements USplineComponent metadata interface
    virtual USplineMetadata* GetSplinePointsMetadata() override;
    virtual const USplineMetadata* GetSplinePointsMetadata() const override;
};
```

### Key Features

- Inherits all USplineComponent functionality
- Adds `UVoxelSplineMetadata` for per-point custom data
- Works with VoxelGraph spline parameters
- Automatically used by spline stamp actors

---

## Spline Stamps

### FVoxelHeightSplineStamp

For 2D heightfield terrain (roads on ground):

```cpp
USTRUCT(meta = (ShortName = "Spline", Icon = "ClassIcon.SplineComponent", SortOrder = 1))
struct VOXEL_API FVoxelHeightSplineStamp final : public FVoxelHeightStamp
{
    GENERATED_BODY()

public:
    // Graph defining spline behavior
    UPROPERTY(EditAnywhere, Category = "Config")
    TObjectPtr<UVoxelHeightSplineGraph> Graph;

    // Parameter overrides for the graph
    UPROPERTY()
    FVoxelParameterOverrides ParameterOverrides;
};
```

### FVoxelVolumeSplineStamp

For 3D volumetric terrain (tunnels, caves):

```cpp
USTRUCT(meta = (ShortName = "Spline", Icon = "ClassIcon.SplineComponent", SortOrder = 1))
struct VOXEL_API FVoxelVolumeSplineStamp final : public FVoxelVolumeStamp
{
    GENERATED_BODY()

public:
    // Graph defining spline behavior
    UPROPERTY(EditAnywhere, Category = "Config")
    TObjectPtr<UVoxelVolumeSplineGraph> Graph;

    // Parameter overrides for the graph
    UPROPERTY()
    FVoxelParameterOverrides ParameterOverrides;
};
```

---

## Spline Graphs

### UVoxelHeightSplineGraph

Visual graph for height-based spline effects.

```cpp
UCLASS(BlueprintType, meta = (AssetSubMenu = "Graph"))
class VOXEL_API UVoxelHeightSplineGraph : public UVoxelGraph
{
    GENERATED_BODY()
    // Uses FVoxelOutputNode_OutputHeightSpline as output
};
```

### UVoxelVolumeSplineGraph

Visual graph for volume-based spline effects.

```cpp
UCLASS(BlueprintType, meta = (AssetSubMenu = "Graph"))
class VOXEL_API UVoxelVolumeSplineGraph : public UVoxelGraph
{
    GENERATED_BODY()
    // Uses FVoxelOutputNode_OutputVolumeSpline as output
};
```

### Creating Spline Graphs

1. Content Browser > Right-click
2. Voxel > Height Spline Graph (for 2D)
3. Or: Voxel > Volume Spline Graph (for 3D)
4. Double-click to open graph editor

---

## Output Nodes

### Output Height Spline

```cpp
USTRUCT()
struct VOXEL_API FVoxelOutputNode_OutputHeightSpline : public FVoxelOutputNode_OutputHeightBase
{
    GENERATED_BODY()

    // Max width of the spline effect
    VOXEL_INPUT_PIN(float, MaxWidth, 1000.f);

    // Inherited from OutputHeightBase:
    // - Height (float)
    // - BlendModeOverride (EVoxelHeightBlendMode)
    // - SurfaceType (UVoxelSurfaceTypeInterface*)
    // - Alpha (float)
};
```

### Output Volume Spline

```cpp
USTRUCT()
struct VOXEL_API FVoxelOutputNode_OutputVolumeSpline : public FVoxelOutputNode_OutputVolumeBase
{
    GENERATED_BODY()

    // Max width of the spline effect
    VOXEL_INPUT_PIN(float, MaxWidth, 1000.f);

    // Inherited from OutputVolumeBase:
    // - Distance (float)
    // - SurfaceType (UVoxelSurfaceTypeInterface*)
    // - Alpha (float)
    // - Bounds (FVoxelBox)
};
```

---

## Spline Function Library

Functions available in spline graphs for querying spline data.

### Position Queries

```cpp
// Get closest spline key (automatic 2D/3D selection)
FVoxelFloatBuffer GetClosestSplineKeyGeneric(const FVoxelVectorBuffer& Position) const;

// Get closest spline key (2D - for height splines)
FVoxelFloatBuffer GetClosestSplineKey2D(const FVoxelVector2DBuffer& Position) const;

// Get closest spline key (3D - for volume splines)
FVoxelFloatBuffer GetClosestSplineKey3D(const FVoxelVectorBuffer& Position) const;
```

### Spline Data Queries

```cpp
// Get total spline length
float GetSplineLength() const;

// Get world position at spline key
FVoxelVectorBuffer GetPositionAlongSpline(const FVoxelFloatBuffer& SplineKey) const;

// Get Z coordinate at spline key
FVoxelFloatBuffer GetHeightAlongSpline(const FVoxelFloatBuffer& SplineKey) const;

// Get distance from start (0 to GetSplineLength)
FVoxelFloatBuffer GetDistanceAlongSpline(const FVoxelFloatBuffer& SplineKey) const;

// Get full transform (position, rotation, scale)
FVoxelTransformBuffer GetTransformAlongSpline(const FVoxelFloatBuffer& SplineKey) const;
```

### Spline Parameter Queries

```cpp
// Get float parameter value at spline key
FVoxelFloatBuffer GetFloatSplineParameterValue(
    const FVoxelFloatSplineParameter& Parameter,
    const FVoxelFloatBuffer& SplineKey) const;

// Get Vector2D parameter value at spline key
FVoxelVector2DBuffer GetVector2DSplineParameterValue(
    const FVoxelVector2DSplineParameter& Parameter,
    const FVoxelFloatBuffer& SplineKey) const;

// Get Vector parameter value at spline key
FVoxelVectorBuffer GetVectorSplineParameterValue(
    const FVoxelVectorSplineParameter& Parameter,
    const FVoxelFloatBuffer& SplineKey) const;
```

---

## Spline Parameters

Custom data that varies along the spline.

### FVoxelFloatSplineParameter

```cpp
USTRUCT(DisplayName = "Float Spline Parameter")
struct VOXEL_API FVoxelFloatSplineParameter : public FVoxelSplineParameter
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Config")
    float Default = 0.f;
};
```

### FVoxelVector2DSplineParameter

```cpp
USTRUCT(DisplayName = "Vector 2D Spline Parameter")
struct VOXEL_API FVoxelVector2DSplineParameter : public FVoxelSplineParameter
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Config")
    FVector2D Default = FVector2D::ZeroVector;
};
```

### FVoxelVectorSplineParameter

```cpp
USTRUCT(DisplayName = "Vector Spline Parameter")
struct VOXEL_API FVoxelVectorSplineParameter : public FVoxelSplineParameter
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Config")
    FVector Default = FVector::ZeroVector;
};
```

---

## Spline Metadata

Per-point custom data stored with the spline.

### UVoxelSplineMetadata

```cpp
UCLASS(Within=VoxelSplineComponent)
class VOXEL_API UVoxelSplineMetadata : public USplineMetadata
{
    GENERATED_BODY()

public:
    // Map of parameter GUID to values per point
    UPROPERTY()
    TMap<FGuid, FVoxelSplineMetadataValues> GuidToValues;

    // Sync metadata with graph parameters
    void Fixup(const UVoxelGraph& Graph);

    // Get runtime-optimized metadata
    TSharedRef<FVoxelSplineMetadataRuntime> GetRuntime() const;
};
```

### Runtime Metadata

```cpp
struct VOXEL_API FVoxelSplineMetadataRuntime
{
    TVoxelMap<FGuid, TVoxelArray<float>> GuidToFloatValues;
    TVoxelMap<FGuid, TVoxelArray<FVector2f>> GuidToVector2DValues;
    TVoxelMap<FGuid, TVoxelArray<FVector3f>> GuidToVectorValues;
};
```

---

## Setup Workflow

### Creating a Road (Height Spline)

1. **Create Height Spline Graph**
   - Content Browser > Voxel > Height Spline Graph
   - Name: `HG_Road`

2. **Build the Graph**
   ```
   [Get Closest Spline Key 2D]
       Position: [Get Position 2D]
       │
       └──► [Get Distance From Spline]
               │
               └──► [Compare: Distance < RoadWidth]
                       │
                       ├──► True: [Get Height Along Spline] → Height
                       │
                       └──► False: [Get Query Previous Height] → Height

   [Output Height Spline]
       Height: (from above)
       MaxWidth: RoadWidth + Falloff
       BlendMode: Override
   ```

3. **Create Stamp Actor**
   - Place Height Spline Stamp Actor in level
   - Assign `HG_Road` graph
   - Configure spline points

4. **Edit Spline**
   - Select actor > Edit Spline in viewport
   - Add/move points to define road path

### Creating a Tunnel (Volume Spline)

1. **Create Volume Spline Graph**
   - Content Browser > Voxel > Volume Spline Graph
   - Name: `VG_Tunnel`

2. **Build the Graph**
   ```
   [Get Closest Spline Key 3D]
       Position: [Get Position]
       │
       └──► [Get Transform Along Spline]
               │
               └──► [Transform Position to Spline Local]
                       │
                       └──► [Make Cylinder SDF]
                               Radius: TunnelRadius
                               │
                               └──► [Negate] (to carve)
                                       │
                                       └──► [Output Volume Spline]
                                               Distance: (from above)
                                               MaxWidth: TunnelRadius + Margin
                                               BlendMode: Subtractive
   ```

---

## Graph Examples

### Simple Flat Road

```
Graph: HG_FlatRoad

[Get Position 2D] ───────────────────────────────────────┐
                                                          │
[Get Closest Spline Key 2D] ◄────────────────────────────┤
    │                                                     │
    ├──► [Get Height Along Spline] ─────────────────┐    │
    │                                                │    │
    └──► [Get Distance Along Spline] ◄──────────────┼────┘
            │                                        │
            └──► [Calculate Distance to Center]     │
                    │                               │
                    └──► [Smooth Step Falloff]     │
                            │                      │
                            └──► [Lerp]           │
                                    A: Query Previous Height
                                    B: Spline Height ◄────┘
                                    Alpha: Falloff
                                    │
                                    └──► [Output Height Spline]
                                            Height: (result)
                                            MaxWidth: RoadWidth
```

### Variable Width Road

Using spline parameters for width variation:

```
Parameters:
    - RoadWidth (Float Spline Parameter, Default: 500)

Graph:
[Get Closest Spline Key 2D] ───┬──► [Get Float Spline Parameter Value]
    │                          │        Parameter: RoadWidth
    │                          │        │
    │                          │        └──► CurrentWidth
    │                          │
    └──► [Get Height Along Spline] ──► TargetHeight

[Calculate Horizontal Distance to Spline]
    │
    └──► [Compare: Distance < CurrentWidth]
            │
            └──► [Select Height]
                    True: TargetHeight
                    False: QueryPreviousHeight
```

---

## Spline Segments

Internal representation for efficient queries.

```cpp
struct VOXEL_API FVoxelSplineSegment
{
    // Bounding box for spatial queries
    FVoxelBox Bounds;

    // ISPC-compatible segment data
    ispc::FSegment Segment;

    // Curve interpolation data
    FInterpCurvePoint<FQuat> RotationA, RotationB;
    FInterpCurvePoint<FVector> PositionA, PositionB;
    FInterpCurvePoint<FVector> ScaleA, ScaleB;

    // Per-parameter interpolation values
    TVoxelMap<FGuid, TPair<float, float>> GuidToFloatValue;
    TVoxelMap<FGuid, TPair<FVector2f, FVector2f>> GuidToVector2DValue;
    TVoxelMap<FGuid, TPair<FVector3f, FVector3f>> GuidToVectorValue;

    // Create segments from USplineComponent data
    static TVoxelArray<FVoxelSplineSegment> Create(
        const FSplineCurves& SplineCurves,
        const FVoxelSplineMetadataRuntime& MetadataRuntime);
};
```

### Interpolation Modes

```cpp
namespace ispc
{
    enum EInterpMode
    {
        InterpMode_Constant = 0,  // Step interpolation
        InterpMode_Linear = 1,    // Linear interpolation
        InterpMode_Cubic = 2      // Cubic spline interpolation
    };
}
```

---

## Runtime Structure

### Height Spline Runtime

```cpp
struct VOXEL_API FVoxelHeightSplineStampRuntime : public FVoxelHeightStampRuntime
{
    // Graph evaluator
    TVoxelNodeEvaluator<FVoxelOutputNode_OutputHeightSpline> Evaluator;

    // Spline properties
    float MaxWidth = 0.f;
    FVoxelFloatRange HeightRange;
    bool bRelativeHeightRange = false;

    // Spline data
    bool bClosedLoop = false;
    int32 ReparamStepsPerSegment = 0;
    float SplineLength = 0.f;
    FSplineCurves SplineCurves;
    TVoxelArray<FVoxelSplineSegment> Segments;

    // Metadata
    TSharedPtr<FVoxelRuntimeMetadataOverrides> MetadataOverrides;
    TSharedPtr<const FVoxelSplineMetadataRuntime> MetadataRuntime;
};
```

### Volume Spline Runtime

```cpp
struct VOXEL_API FVoxelVolumeSplineStampRuntime : public FVoxelVolumeStampRuntime
{
    // Same structure as height, but for 3D
    TVoxelNodeEvaluator<FVoxelOutputNode_OutputVolumeSpline> Evaluator;

    float MaxWidth = 0.f;
    bool bClosedLoop = false;
    int32 ReparamStepsPerSegment = 0;
    float SplineLength = 0.f;
    FSplineCurves SplineCurves;
    TVoxelArray<FVoxelSplineSegment> Segments;

    TSharedPtr<FVoxelRuntimeMetadataOverrides> MetadataOverrides;
    TSharedPtr<const FVoxelSplineMetadataRuntime> MetadataRuntime;
};
```

---

## Common Use Cases

### Roads

| Feature | Implementation |
|---------|----------------|
| Flat surface | Get spline height, blend with Override mode |
| Graded surface | Lerp between spline height and terrain |
| Variable width | Use float spline parameter |
| Surface type | Paint road material along spline |

### Rivers

| Feature | Implementation |
|---------|----------------|
| Carved channel | Subtractive blend mode |
| Water surface | Separate water mesh following spline |
| Variable depth | Float parameter for depth |
| Riverbed texture | Paint surface type in channel |

### Tunnels

| Feature | Implementation |
|---------|----------------|
| Cylindrical bore | Sphere/cylinder SDF, subtractive |
| Entrance smoothing | Smooth distance near entry points |
| Support pillars | Periodic additive stamps |
| Variable radius | Float parameter |

### Paths

| Feature | Implementation |
|---------|----------------|
| Hiking trail | Narrow width, subtle height change |
| Worn appearance | Texture parameter for roughness |
| Vegetation clearing | Scatter exclusion zone |

---

## Performance Tips

1. **MaxWidth**: Set accurately - too large wastes computation
2. **Segment Count**: Fewer points = faster (use smooth curves)
3. **Reparameterization**: Default steps usually sufficient
4. **Query Previous**: Use when blending with existing terrain
5. **Closed Loops**: Enable only when needed

---

## Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| Spline not affecting terrain | Graph not assigned | Assign Height/Volume Spline Graph |
| Effect cuts off early | MaxWidth too small | Increase MaxWidth in output node |
| Jagged edges | Insufficient spline points | Add more control points |
| Performance issues | MaxWidth too large | Reduce to actual needed width |
| Parameters not interpolating | Metadata not synced | Call Fixup() on metadata |
| Wrong blend behavior | Incorrect blend mode | Set appropriate blend mode in output |

---

## Related Topics

- [05-height-layer-system.md](05-height-layer-system.md) - Height terrain basics
- [06-volume-layer-system.md](06-volume-layer-system.md) - Volume terrain basics
- [07-stamp-system.md](07-stamp-system.md) - Stamp architecture
- [02-voxelgraph-system.md](02-voxelgraph-system.md) - Graph system
