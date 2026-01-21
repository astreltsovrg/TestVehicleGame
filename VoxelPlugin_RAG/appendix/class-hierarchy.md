# Class Hierarchy Reference

## Core Architecture

### Module Dependency Chain

```
VoxelCore (foundation)
    │
    └──► VoxelGraph (node system)
              │
              └──► Voxel (world, stamps, sculpting)
                        │
                        └──► VoxelPCG (procedural content)
```

---

## World & Layer System

```
UObject
├── UVoxelAsset
│   ├── UVoxelLayerStack
│   ├── UVoxelLayer (abstract)
│   │   ├── UVoxelHeightLayer
│   │   └── UVoxelVolumeLayer
│   ├── UVoxelMegaMaterial
│   └── UVoxelSurfaceTypeInterface
│       ├── UVoxelSurfaceTypeAsset
│       └── UVoxelSmartSurfaceType
│
AActor
└── AVoxelWorld
```

---

## Graph System

```
UObject
└── UVoxelGraph (abstract)
    ├── UVoxelTerminalGraph
    ├── UVoxelHeightGraph
    ├── UVoxelVolumeGraph
    ├── UVoxelScatterGraph
    ├── UVoxelSurfaceTypeGraph
    └── UVoxelPCGGraph

FVoxelVirtualStruct
└── FVoxelNode (abstract)
    ├── FVoxelNode_AdvancedNoise2D
    ├── FVoxelNode_AdvancedNoise3D
    ├── FVoxelNode_ScatterBase
    │   └── FVoxelNode_ScatterMesh
    ├── FVoxelNode_Generate2DPoints
    ├── FVoxelNode_ScatterPoints
    ├── FVoxelOutputNode (abstract)
    │   └── FVoxelOutputNode_OutputPoints
    └── (many more nodes...)
```

---

## Stamp System

### Stamp Assets

```
FVoxelVirtualStruct
└── FVoxelStamp (abstract)
    ├── FVoxelHeightStamp (abstract)
    │   ├── FVoxelHeightGraphStamp
    │   ├── FVoxelHeightmapStamp
    │   ├── FVoxelHeightSculptStamp
    │   └── FVoxelHeightSplineStamp
    │
    └── FVoxelVolumeStamp (abstract)
        ├── FVoxelVolumeGraphStamp
        ├── FVoxelVolumeSculptStamp
        └── FVoxelVolumeSplineStamp
```

### Stamp Runtime

```
FVoxelStampRuntime (abstract)
├── FVoxelHeightStampRuntime
│   ├── FVoxelHeightGraphStampRuntime
│   ├── FVoxelHeightmapStampRuntime
│   ├── FVoxelHeightSculptStampRuntime
│   └── FVoxelHeightSplineStampRuntime
│
└── FVoxelVolumeStampRuntime
    ├── FVoxelVolumeGraphStampRuntime
    ├── FVoxelVolumeSculptStampRuntime
    └── FVoxelVolumeSplineStampRuntime
```

### Stamp Actors

```
AActor
├── AVoxelStampActor
├── AVoxelHeightSculptActor
├── AVoxelVolumeSculptActor
├── AVoxelScatterActor
└── AVoxelSplineActor
```

---

## Sculpting System

### Modifiers

```
FVoxelVirtualStruct
├── FVoxelHeightModifier (abstract)
│   └── (various height modifiers)
│
└── FVoxelVolumeModifier (abstract)
    ├── FVoxelSphereVolumeModifier
    ├── FVoxelCubeVolumeModifier
    ├── FVoxelFlattenVolumeModifier
    ├── FVoxelSmoothVolumeModifier
    ├── FVoxelPaintSurfaceVolumeModifier
    └── FVoxelGraphVolumeModifier
```

### Save System

```
FVoxelVirtualStruct
├── FVoxelVolumeSculptSave
└── FVoxelHeightSculptSave

UObject
└── UVoxelSculptSaveAsset (abstract)
    ├── UVoxelVolumeSculptSaveAsset
    └── UVoxelHeightSculptSaveAsset
```

---

## Collision & Rendering

```
UActorComponent
└── USceneComponent
    └── UPrimitiveComponent
        ├── UVoxelCollisionComponent
        ├── UVoxelStaticMeshCollisionComponent
        └── UVoxelNaniteComponent

UActorComponent
└── UVoxelCollisionInvokerComponent

AActor
└── AVoxelCollisionBaker
```

---

## Query System

```
FVoxelVirtualStruct
└── FVoxelQuery (abstract)
    ├── FVoxelHeightQuery
    │   ├── FVoxelHeightBulkQuery
    │   └── FVoxelHeightSparseQuery
    │
    └── FVoxelVolumeQuery
        ├── FVoxelVolumeBulkQuery
        └── FVoxelVolumeSparseQuery

FVoxelQueryResult
```

---

## Buffer System

```
FVoxelVirtualStruct
└── FVoxelBuffer (abstract)
    ├── FVoxelFloatBuffer
    ├── FVoxelDoubleBuffer
    ├── FVoxelIntBuffer
    ├── FVoxelBoolBuffer
    ├── FVoxelVectorBuffer
    ├── FVoxelDoubleVectorBuffer
    ├── FVoxelVector2DBuffer
    ├── FVoxelQuaternionBuffer
    ├── FVoxelLinearColorBuffer
    ├── FVoxelSeedBuffer
    ├── FVoxelObjectBuffer
    └── FVoxelSurfaceTypeBuffer
```

---

## Subsystem Hierarchy

### World Subsystems

```
IVoxelWorldSubsystem
├── FVoxelScatterManager
├── FVoxelCollisionInvokerManager
└── (other world-level managers)
```

### Chunk Subsystems

```
FVoxelSubsystem (abstract)
├── FVoxelScatterSubsystem
├── FVoxelCollisionSubsystem
├── FVoxelRenderSubsystem
└── (other chunk-level subsystems)
```

---

## PCG Integration

```
UPCGSettings
└── UVoxelPCGSettings (abstract)
    ├── UPCGVoxelSamplerSettings
    ├── UPCGVoxelQuerySettings
    ├── UPCGVoxelProjectionSettings
    ├── UPCGCallVoxelGraphSettings
    ├── UPCGVoxelStampSpawnerSettings
    ├── UPCGWaitForVoxelWorldSettings
    └── UPCGCreateVoxelSplineSettings
```

---

## Material System

```
UVoxelAsset
├── UVoxelMegaMaterial
└── UVoxelSurfaceTypeInterface
    ├── UVoxelSurfaceTypeAsset
    └── UVoxelSmartSurfaceType

UMaterialExpression
└── UMaterialExpressionVoxelMegaMaterialSwitch

FVoxelMegaMaterialProxy (runtime)
FVoxelSurfaceTypeBlend (blend data)
```

---

## Spline System

```
UActorComponent
└── USceneComponent
    └── USplineComponent
        └── UVoxelSplineComponent

FVoxelStamp
├── FVoxelHeightSplineStamp
└── FVoxelVolumeSplineStamp
```

---

## Heightmap Import

```
UObject
├── UVoxelHeightmap
│   ├── UVoxelHeightmap_Height
│   └── UVoxelHeightmap_Weight
│
└── UVoxelHeightmapImporter (factory)

FVoxelHeightmapStamp
```

---

## Point System

```
FVoxelVirtualStruct
└── FVoxelPointSet

FVoxelPointAttributes (constants)
FVoxelScatterNodeRuntime
└── TVoxelScatterNodeRuntime<T>
    └── FVoxelScatterMeshNodeRuntime
```

---

## Runtime Classes

### Futures & Dependencies

```
FVoxelFuture (async operation)
FVoxelDependency (invalidation tracking)
FVoxelDependencyTracker
FVoxelDependencyCollector
```

### Runtime Proxies

```
FVoxelMegaMaterialProxy
FVoxelStampRuntimeProxy
FVoxelScatterActorRuntime
```

---

## Blueprint Library Classes

```
UBlueprintFunctionLibrary
├── UVoxelVolumeSculptBlueprintLibrary
├── UVoxelHeightSculptBlueprintLibrary
├── UVoxelQueryBlueprintLibrary
├── UVoxelHeightmapFunctionLibrary
└── UVoxelScatterFunctionLibrary (inside graph)
```

---

## Key Type Definitions

### Enums

```cpp
EVoxelHeightBlendMode      // Max, Min, Override
EVoxelVolumeBlendMode      // Add, Remove, Set
EVoxelMegaMaterialTarget   // NonNanite, NaniteWPO, etc.
EVoxelAdvancedNoiseOctaveType // SmoothPerlin, BillowyPerlin, RidgedPerlin
EVoxelHeightmapWeightType  // AlphaBlended, WeightBlended
```

### Core Types

```cpp
FVoxelSeed                 // Deterministic random seed
FVoxelBox                  // 3D bounding box
FVoxelFloatRange          // Min/max float range
FVoxelInt32Range          // Min/max int32 range
FVoxelSurfaceType         // Surface type identifier
FVoxelMaterialRenderIndex // Material index for rendering
```
