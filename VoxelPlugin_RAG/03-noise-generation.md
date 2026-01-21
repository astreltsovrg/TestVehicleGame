# Noise Generation

## Summary

VoxelPlugin2 provides powerful multi-octave noise generation through `FVoxelNode_AdvancedNoise2D` and `FVoxelNode_AdvancedNoise3D`. These nodes support Fractional Brownian Motion (FBM) with configurable octave types, including Perlin, Cellular, Simplex, and Value noise variants.

---

## Key Classes

| Class | Header | Purpose |
|-------|--------|---------|
| `FVoxelNode_AdvancedNoise2D` | VoxelAdvancedNoiseNodes.h | 2D multi-octave noise (for height) |
| `FVoxelNode_AdvancedNoise3D` | VoxelAdvancedNoiseNodes.h | 3D multi-octave noise (for volume) |
| `FVoxelSeed` | VoxelSeed.h | Deterministic seed system |

---

## Octave Types

```cpp
UENUM(BlueprintType, DisplayName = "Advanced Noise Octave Type")
enum class EVoxelAdvancedNoiseOctaveType : uint8
{
    Default = 200,                // Use DefaultOctaveType

    // Perlin variants
    SmoothPerlin = 0,             // Perlin2D/3D
    BillowyPerlin = 1,            // abs(Perlin) * 2 - 1
    RidgedPerlin = 2,             // (1 - abs(Perlin)) * 2 - 1

    // Cellular (Voronoi) variants
    SmoothCellular = 3,           // Cellular2D/3D
    BillowyCellular = 4,          // abs(Cellular) * 2 - 1
    RidgedCellular = 5,           // (1 - abs(Cellular)) * 2 - 1

    // Simplex variants
    SmoothSimplex = 6,            // Simplex2D/3D
    BillowySimplex = 7,           // abs(Simplex) * 2 - 1
    RidgedSimplex = 8,            // (1 - abs(Simplex)) * 2 - 1

    // Value variants
    SmoothValue = 9,              // Value2D/3D
    BillowyValue = 10,            // abs(Value) * 2 - 1
    RidgedValue = 11,             // (1 - abs(Value)) * 2 - 1
};
```

### Variant Descriptions

| Variant | Formula | Visual Result |
|---------|---------|---------------|
| **Smooth** | noise(x) | Standard smooth terrain |
| **Billowy** | abs(noise) * 2 - 1 | Rolling hills, soft peaks |
| **Ridged** | (1 - abs(noise)) * 2 - 1 | Sharp mountain ridges |

---

## FVoxelNode_AdvancedNoise2D

**Purpose:** 2D multi-octave noise for height generation

### Input Pins

| Pin | Type | Default | Description |
|-----|------|---------|-------------|
| `Position` | FVoxelVector2DBuffer | Query position | 2D position to sample |
| `Amplitude` | FVoxelFloatBuffer | 10000.f | Height range (cm) - 100m default |
| `FeatureScale` | FVoxelFloatBuffer | 100000.f | Tiling scale (cm) - 1km default |
| `Lacunarity` | FVoxelFloatBuffer | 2.f | Frequency multiplier per octave |
| `Gain` | FVoxelFloatBuffer | 0.5f | Amplitude falloff per octave |
| `CellularJitter` | FVoxelFloatBuffer | 0.9f | Cell randomization (cellular noise) |
| `NumOctaves` | int32 | 10 | Number of FBM layers |
| `Seed` | FVoxelSeed | nullptr | Random seed for determinism |
| `DefaultOctaveType` | EVoxelAdvancedNoiseOctaveType | SmoothPerlin | Default noise type |
| `OctaveType` (variadic) | EVoxelAdvancedNoiseOctaveType | Default | Per-octave type override |
| `OctaveStrength` (variadic) | FVoxelFloatBuffer | 1.f | Per-octave amplitude multiplier |

### Output Pins

| Pin | Type | Description |
|-----|------|-------------|
| `Value` | FVoxelFloatBuffer | Combined noise value |

---

## FVoxelNode_AdvancedNoise3D

**Purpose:** 3D multi-octave noise for volume generation

### Input Pins

| Pin | Type | Default | Description |
|-----|------|---------|-------------|
| `Position` | FVoxelVectorBuffer | Query position | 3D position to sample |
| `Amplitude` | FVoxelFloatBuffer | 10000.f | Noise range |
| `FeatureScale` | FVoxelFloatBuffer | 100000.f | Tiling scale |
| `Lacunarity` | FVoxelFloatBuffer | 2.f | Frequency multiplier |
| `Gain` | FVoxelFloatBuffer | 0.5f | Amplitude falloff |
| `CellularJitter` | FVoxelFloatBuffer | 0.9f | Cell randomization |
| `NumOctaves` | int32 | 10 | FBM layers |
| `Seed` | FVoxelSeed | nullptr | Random seed |
| `DefaultOctaveType` | EVoxelAdvancedNoiseOctaveType | SmoothPerlin | Default type |
| `OctaveType` (variadic) | EVoxelAdvancedNoiseOctaveType | Default | Per-octave override |
| `OctaveStrength` (variadic) | FVoxelFloatBuffer | 1.f | Per-octave multiplier |

### Output Pins

| Pin | Type | Description |
|-----|------|-------------|
| `Value` | FVoxelFloatBuffer | Combined 3D noise value |

---

## Parameter Reference

### Amplitude & FeatureScale Relationship

```
Output Range: [-Amplitude/2, +Amplitude/2]
Spatial Period: ~FeatureScale (one full noise cycle)

Example: Amplitude=10000, FeatureScale=100000
  - Height varies ±50m (5000cm) from baseline
  - Features repeat roughly every 1km
```

### FBM (Fractional Brownian Motion)

The noise is computed as:
```
Total = 0
CurrentAmplitude = Amplitude
CurrentScale = FeatureScale

for octave = 0 to NumOctaves-1:
    noise_value = Noise(Position / CurrentScale, OctaveType[octave])
    Total += noise_value * CurrentAmplitude * OctaveStrength[octave]
    CurrentAmplitude *= Gain
    CurrentScale /= Lacunarity

return Total
```

### Lacunarity & Gain Guidelines

| Lacunarity | Effect |
|------------|--------|
| 2.0 | Standard doubling (recommended) |
| 1.5-1.8 | Softer detail progression |
| 2.2-2.5 | More aggressive detail |

| Gain | Effect |
|------|--------|
| 0.5 | Standard FBM (recommended) |
| 0.3-0.4 | Less detail, smoother terrain |
| 0.6-0.7 | More detail, rougher terrain |

---

## Seed System

VoxelPlugin uses `FVoxelSeed` for deterministic generation:

```cpp
// Seeds ensure identical terrain for same input
VOXEL_INPUT_PIN(FVoxelSeed, Seed, nullptr);

// Usage in graph: Make Seed node → connect to Seed pin
// Same seed + same parameters = identical output
```

### Seed Determinism Rules

1. Same `Seed` + same `Position` = identical `Value`
2. Seeds are 64-bit for high uniqueness
3. Different seeds produce completely different noise patterns
4. Null seed uses default (reproducible across sessions)

---

## Terrain Presets

### Smooth Rolling Hills
```
Amplitude: 5000 (50m)
FeatureScale: 200000 (2km)
NumOctaves: 4
Lacunarity: 2.0
Gain: 0.5
DefaultOctaveType: SmoothPerlin
```

### Mountain Ranges (Ridged)
```
Amplitude: 30000 (300m)
FeatureScale: 500000 (5km)
NumOctaves: 6
Lacunarity: 2.2
Gain: 0.45
DefaultOctaveType: RidgedPerlin
```

### Alien/Cellular Landscape
```
Amplitude: 15000 (150m)
FeatureScale: 150000 (1.5km)
NumOctaves: 5
Lacunarity: 2.0
Gain: 0.5
CellularJitter: 0.7
DefaultOctaveType: SmoothCellular
```

### High-Detail Natural Terrain
```
Amplitude: 20000 (200m)
FeatureScale: 300000 (3km)
NumOctaves: 8
Lacunarity: 2.0
Gain: 0.5
DefaultOctaveType: SmoothPerlin
OctaveType[0]: SmoothPerlin (base shape)
OctaveType[1]: SmoothPerlin (large features)
OctaveType[2]: RidgedPerlin (mountain ridges)
OctaveType[3-7]: SmoothPerlin (detail)
```

---

## Per-Octave Configuration

The variadic pins allow per-octave customization:

```
Example: Hybrid terrain with ridged mountains and smooth valleys

OctaveType[0]: SmoothPerlin    (base continent shape)
OctaveType[1]: RidgedPerlin    (mountain ridges)
OctaveType[2]: BillowyPerlin   (rolling hills)
OctaveType[3]: SmoothPerlin    (medium detail)
OctaveType[4+]: Default        (use DefaultOctaveType for fine detail)

OctaveStrength[0]: 1.0         (full amplitude for base)
OctaveStrength[1]: 1.2         (emphasize ridges)
OctaveStrength[2]: 0.8         (reduce hills)
OctaveStrength[3+]: 1.0        (normal detail)
```

---

## Integration Example

### Height Graph Setup

```
[Make Seed (12345)]
    │
    └──► [Advanced Noise 2D]
              Position: [Get Position 2D] ←── from query
              Amplitude: 50000
              FeatureScale: 200000
              NumOctaves: 6
              Gain: 0.5
              Lacunarity: 2.0
              DefaultOctaveType: SmoothPerlin
              │
              └──► [Output Height]
```

### C++ Usage

```cpp
// In custom node that uses noise
void FMyCustomNode::Compute(FVoxelGraphQuery Query) const
{
    // Get position buffer
    const FVoxelVector2DBuffer& Positions = Query.Get(PositionPin);

    // Configure noise parameters
    FVoxelNoiseParameters Params;
    Params.Amplitude = 10000.f;
    Params.FeatureScale = 100000.f;
    Params.NumOctaves = 6;
    Params.Lacunarity = 2.f;
    Params.Gain = 0.5f;
    Params.OctaveType = EVoxelAdvancedNoiseOctaveType::SmoothPerlin;

    // Generate noise (using internal functions)
    FVoxelFloatBuffer Heights;
    Heights.SetNum(Positions.Num());

    // ... noise computation ...

    Query.Set(HeightOutput, MoveTemp(Heights));
}
```

---

## Performance Considerations

1. **Reduce NumOctaves** for distant LODs (4-6 octaves sufficient for most cases)
2. **Cellular noise** is more expensive than Perlin
3. **Variadic pins** add per-octave overhead - use DefaultOctaveType when uniform
4. **FeatureScale** affects memory: smaller scale = more detail = more computation
5. **Buffer size** scales linearly with query region

---

## Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| Terrain looks "tiled" | FeatureScale too small | Increase FeatureScale |
| Terrain too smooth | NumOctaves too low | Increase NumOctaves (6-8) |
| Terrain too noisy | Gain too high | Reduce Gain (0.4-0.5) |
| Visible seams | Mismatched seeds | Ensure consistent Seed |
| Z-fighting at borders | Amplitude mismatch | Verify Amplitude consistency |

---

## Related Topics

- [02-voxelgraph-system.md](02-voxelgraph-system.md) - VoxelGraph basics
- [05-height-layer-system.md](05-height-layer-system.md) - Using noise in height layers
- [06-volume-layer-system.md](06-volume-layer-system.md) - 3D noise for volumes
