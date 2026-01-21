# API Quick Reference

## One-Page Cheat Sheet for VoxelPlugin2

---

## World Setup

```cpp
// Get VoxelWorld in level
AVoxelWorld* World = /* from level */;

// Check if ready
if (World && World->IsVoxelWorldReady())
{
    // Use world...
}
```

---

## Volume Sculpting (Runtime)

### Blueprint Functions (UVoxelVolumeSculptBlueprintLibrary)

```cpp
// Sphere sculpt
SculptSphere(
    Actor,              // AVoxelVolumeSculptActor*
    Center,             // FVector (world position)
    Radius,             // float (cm)
    Mode,               // EVoxelVolumeBlendMode (Add/Remove/Set)
    Smoothness          // float (0-1, edge falloff)
);

// Cube sculpt
SculptCube(
    Actor,
    Center,             // FVector
    Size,               // FVector (half-extents)
    Rotation,           // FRotator
    Roundness,          // float (corner rounding)
    Mode,
    Smoothness
);

// Flatten terrain
Flatten(
    Actor,
    Center,             // FVector
    Normal,             // FVector (flatten direction)
    Radius,             // float
    Height,             // float (flatten height)
    Falloff,            // float (edge falloff)
    Type                // EVoxelFlattenType
);

// Smooth terrain
Smooth(
    Actor,
    Center,
    Radius,
    Strength,           // float (0-1)
    Brush               // UVoxelBrush* (optional)
);

// Paint surface
PaintSurface(
    Actor,
    Center,
    Radius,
    Strength,
    Mode,               // EVoxelPaintSurfaceMode
    SurfaceType,        // UVoxelSurfaceTypeInterface*
    Brush
);

// Apply sculpt graph
ApplySculptGraph(
    Actor,
    Center,
    Radius,
    Graph,              // UVoxelVolumeGraph*
    Rotation
);
```

### Blend Modes

```cpp
EVoxelVolumeBlendMode::Add     // Add material
EVoxelVolumeBlendMode::Remove  // Remove material
EVoxelVolumeBlendMode::Set     // Replace
```

---

## Persistence (Save/Load)

```cpp
// Get save data
FVoxelVolumeSculptSave Save;
bool bValid = Actor->K2_GetSave(Save);

// Check validity
if (bValid && Save.IsValidSave())
{
    // Store save...
    int64 Size = Save.GetSaveSize();
}

// Load save data
Actor->LoadFromSave(Save);

// Clear all sculpts
Actor->ClearSculptData();
```

---

## Terrain Queries

### Blueprint Functions (UVoxelQueryBlueprintLibrary)

```cpp
// Query single layer
FVoxelQueryResult Result;
K2_QueryVoxelLayer(
    WorldContextObject,
    LayerStack,         // UVoxelLayerStack*
    Layer,              // UVoxelLayer*
    Position,           // FVector
    Result              // out FVoxelQueryResult
);

// Access result
float Value = Result.Value;
FVector Normal = Result.Normal;
UVoxelSurfaceTypeInterface* Surface = Result.SurfaceType;
```

### Query Result

```cpp
struct FVoxelQueryResult
{
    float Value;                                    // Height or SDF
    FVector Normal;                                 // Surface normal
    UVoxelSurfaceTypeInterface* SurfaceType;       // Surface type
    TMap<UVoxelMetadata*, FVoxelPinValue> MetadataToValue;
};
```

---

## Heightmap Import

```cpp
// Import PNG heightmap
UVoxelHeightmap* Heightmap = FVoxelHeightmapImporter::ImportPNG(
    Package,
    AssetName,
    FilePath,
    Width,
    Height,
    MinHeight,          // World units
    MaxHeight           // World units
);

// Create heightmap stamp
FVoxelHeightmapStamp Stamp;
Stamp.Heightmap = Heightmap;
Stamp.Layer = HeightLayer;
Stamp.BlendMode = EVoxelHeightBlendMode::Max;
```

---

## Noise Generation (Graph Nodes)

### Advanced Noise 2D Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `Position` | - | FVector2D query position |
| `Amplitude` | 10000 | Height range (cm) |
| `FeatureScale` | 10000 | Base scale (cm) |
| `NumOctaves` | 8 | FBM layers |
| `Lacunarity` | 2.0 | Frequency multiplier |
| `Gain` | 0.5 | Amplitude decay |
| `Seed` | - | Random seed |

### Octave Types

```cpp
EVoxelAdvancedNoiseOctaveType::SmoothPerlin   // Standard
EVoxelAdvancedNoiseOctaveType::BillowyPerlin  // Soft hills
EVoxelAdvancedNoiseOctaveType::RidgedPerlin   // Sharp ridges
```

---

## Scatter System

### Scatter Actor Setup

```cpp
// Spawn scatter actor
AVoxelScatterActor* ScatterActor = World->SpawnActor<AVoxelScatterActor>(...);
ScatterActor->Graph = ScatterGraph;
ScatterActor->CreateRuntime();
```

### Point Attributes

```cpp
FVoxelPointAttributes::Position    // FVector
FVoxelPointAttributes::Rotation    // FQuat
FVoxelPointAttributes::Scale       // FVector
FVoxelPointAttributes::Mesh        // UStaticMesh*
FVoxelPointAttributes::Color       // FLinearColor
FVoxelPointAttributes::Density     // float
```

---

## PCG Integration

### Common PCG Nodes

| Node | Purpose |
|------|---------|
| `Voxel Sampler` | Generate points on terrain |
| `Voxel Query` | Get height/surface at points |
| `Voxel Projection` | Project points to terrain |
| `Call Voxel Graph` | Execute VoxelGraph on points |
| `Wait For Voxel World` | Synchronization |

### Voxel Sampler Settings

```cpp
UPCGVoxelSamplerSettings
├── PointsPerSquareMeter (float, default: 1.0)
├── ProjectionMode (EVoxelPCGProjectionMode)
├── bOutputSurfaceTypes (bool)
└── bOutputNormals (bool)
```

---

## Collision Setup

```cpp
// Add collision invoker to actor
UVoxelCollisionInvokerComponent* Invoker = NewObject<UVoxelCollisionInvokerComponent>(this);
Invoker->Radius = 5000.f;  // 50m radius
Invoker->bEnabled = true;
Invoker->RegisterComponent();
```

---

## Material System

### MegaMaterial Setup

```cpp
UVoxelMegaMaterial* MegaMaterial = NewObject<UVoxelMegaMaterial>(...);
MegaMaterial->SurfaceTypes.Add(GrassSurface);
MegaMaterial->SurfaceTypes.Add(RockSurface);
MegaMaterial->bEnableSmoothBlends = true;
```

### Surface Type Asset

```cpp
UVoxelSurfaceTypeAsset* Surface = NewObject<UVoxelSurfaceTypeAsset>(...);
Surface->Material = GrassMaterial;
Surface->BlendSmoothness = 0.5f;
Surface->bInvisible = false;
```

---

## Common Parameter Values

### Typical Scale Values (cm)

| Use Case | VoxelSize | FeatureScale |
|----------|-----------|--------------|
| Large terrain | 100-200 | 100000-500000 |
| Medium terrain | 50-100 | 50000-100000 |
| Detail terrain | 25-50 | 10000-50000 |
| Sculpting | 50-100 | N/A |

### Typical Noise Settings

| Terrain Type | Octaves | Lacunarity | Gain |
|--------------|---------|------------|------|
| Rolling hills | 4-6 | 2.0 | 0.5 |
| Mountains | 6-8 | 2.0 | 0.5 |
| Plains | 2-4 | 2.0 | 0.4 |
| Rocky | 6-8 | 2.5 | 0.6 |

---

## Blueprint Node Quick Reference

### Height Graph Output

```
[Output Height]
├── Height (float)
└── BlendModeOverride (optional)
```

### Volume Graph Output

```
[Output Volume]
├── Distance (float, SDF)
├── SurfaceType (optional)
└── BlendModeOverride (optional)
```

### Scatter Graph Output

```
[Scatter Mesh]
├── In (FVoxelPointSet)
├── Name (FName)
├── ChunkSize (int32, default: 64)
└── RenderDistance (float, default: 64)
```

---

## Error Handling Pattern

```cpp
// Check world validity
AVoxelWorld* World = GetVoxelWorld();
if (!World)
{
    UE_LOG(LogTemp, Warning, TEXT("No VoxelWorld found"));
    return;
}

if (!World->IsVoxelWorldReady())
{
    UE_LOG(LogTemp, Warning, TEXT("VoxelWorld not ready"));
    return;
}

// Proceed with operations...
```

---

## Async Operations

```cpp
// Most sculpt operations return FVoxelFuture
FVoxelFuture Future = SculptActor->SculptSphere(...);

// Operations are async - terrain updates automatically
// No need to wait unless specific ordering required
```

---

## Quick Links

| Topic | File |
|-------|------|
| World setup | 04-voxelworld-layerstack.md |
| Sculpting | 08-runtime-sculpting.md |
| Save/Load | 09-persistence-saveload.md |
| Noise | 03-noise-generation.md |
| Queries | 14-query-system.md |
| PCG | 12-pcg-integration.md |
| Materials | 16-megamaterial-system.md |
| Scatter | 15-scatter-foliage.md |
