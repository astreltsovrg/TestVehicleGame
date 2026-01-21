# VoxelPlugin2 Architecture Overview

## Summary

VoxelPlugin2 is a modular terrain system built on a layered architecture. The plugin consists of 11 modules with clear dependency chains, supporting runtime terrain generation, editing, and rendering with Nanite integration.

---

## Module Dependency Chain

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              RUNTIME MODULES                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│  ┌──────────────┐                                                        │
│  │  VoxelCore   │  Foundation: containers, math, ISPC, compression      │
│  │  (PostInit)  │  Dependencies: Core, Chaos, Renderer, zlib, PNG       │
│  └──────┬───────┘                                                        │
│         │                                                                 │
│         ▼                                                                 │
│  ┌──────────────┐                                                        │
│  │ VoxelGraph   │  Node system, buffers, compilation                     │
│  │  (Default)   │  Dependencies: VoxelCore, Chaos, PhysicsCore, Json     │
│  └──────┬───────┘                                                        │
│         │                                                                 │
│         ▼                                                                 │
│  ┌──────────────┐                                                        │
│  │    Voxel     │  World, stamps, layers, sculpting, rendering          │
│  │  (PostInit)  │  Dependencies: VoxelGraph, Chaos, PCG, Landscape      │
│  └──────┬───────┘                                                        │
│         │                                                                 │
│         ▼                                                                 │
│  ┌──────────────┐                                                        │
│  │  VoxelPCG    │  PCG framework integration                             │
│  │  (Default)   │  Dependencies: Voxel, PCG                              │
│  └──────────────┘                                                        │
│                                                                           │
└─────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────┐
│                              EDITOR MODULES                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│  VoxelCoreEditor ─► VoxelGraphEditor ─► VoxelEditor ─► VoxelPCGEditor   │
│                                                                           │
│  VoxelBlueprint (UncookedOnly) ─► VoxelBlueprintEditor                  │
│                                                                           │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Module Details

### VoxelCore (Foundation)
**Loading Phase:** PostConfigInit
**Headers:** ~145 files

Core utilities and data structures:
- **Containers:** VoxelArray, VoxelMap, VoxelSet, VoxelChunkedArray
- **Spatial Trees:** VoxelAABBTree, VoxelFastOctree, VoxelFastQuadtree
- **Math:** VoxelMathUtilities, VoxelGeometryUtilities
- **Memory:** VoxelAllocator, VoxelBufferPool
- **Compression:** zlib integration
- **ISPC:** Auto-compilation of ISPC shaders for SIMD operations

**Key Dependencies:**
```
Core, CoreUObject, RHI, Engine, Chaos, Renderer, zlib, UElibPNG
```

### VoxelGraph (Node System)
**Loading Phase:** Default
**Headers:** ~134 files

Visual graph system for procedural generation:
- **UVoxelGraph:** Main graph asset
- **FVoxelNode:** Base node struct with input/output pins
- **FVoxelBuffer:** Data buffers (float, vector, etc.)
- **Compilation:** Graph → executable code pipeline

**Key Dependencies:**
```
VoxelCore, Chaos, PhysicsCore, Json
```

### Voxel (Main Module)
**Loading Phase:** PostConfigInit
**Headers:** ~320 files (largest module)

Main terrain system:
- **AVoxelWorld:** Main terrain actor
- **UVoxelLayerStack:** Layer configuration
- **Stamps:** Height and volume modification
- **Sculpting:** Runtime terrain editing
- **Rendering:** Nanite integration, materials

**Key Dependencies:**
```
VoxelGraph, Chaos, Renderer, Slate, PhysicsCore, Landscape,
NavigationSystem, PCG, MeshDescription
```

### VoxelPCG (PCG Integration)
**Loading Phase:** Default
**Headers:** ~30 files

Integration with UE5 Procedural Content Generation:
- VoxelSampler nodes
- PCG point projections
- Terrain-aware spawning

---

## Core Class Hierarchy

### World System
```
AActor
└── AVoxelWorld
    ├── VoxelSize: int32 = 100 (cm)
    ├── LODQuality: FVoxelLODQuality
    ├── MegaMaterial: UVoxelMegaMaterial*
    ├── LayerStack: UVoxelLayerStack*
    ├── bEnableNanite: bool = true
    └── Runtime: TSharedPtr<FVoxelRuntime>
```

### Layer System
```
UObject
├── UVoxelLayerStack
│   └── Contains multiple UVoxelLayer instances
├── UVoxelHeightLayer
│   └── 2D heightfield terrain
└── UVoxelVolumeLayer
    └── 3D volumetric terrain (SDF)
```

### Stamp System
```
FVoxelVirtualStruct
└── FVoxelStamp (base)
    ├── FVoxelHeightStamp
    │   ├── FVoxelHeightGraphStamp
    │   ├── FVoxelHeightmapStamp
    │   └── FVoxelHeightSculptStamp
    └── FVoxelVolumeStamp
        ├── FVoxelShapeStamp
        ├── FVoxelMeshStamp
        └── FVoxelVolumeSculptStamp
```

---

## Data Flow

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           GENERATION PIPELINE                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│  ┌───────────────┐     ┌───────────────┐     ┌───────────────┐          │
│  │  UVoxelGraph  │────►│   FVoxelNode  │────►│ FVoxelBuffer  │          │
│  │   (Asset)     │     │  (Execution)  │     │   (Output)    │          │
│  └───────────────┘     └───────────────┘     └───────┬───────┘          │
│                                                       │                   │
│                                                       ▼                   │
│  ┌───────────────┐     ┌───────────────┐     ┌───────────────┐          │
│  │  FVoxelStamp  │◄────│FVoxelStampMgr │◄────│UVoxelLayerStk │          │
│  │  (Modifier)   │     │ (Registration)│     │ (Configuration)│          │
│  └───────┬───────┘     └───────────────┘     └───────────────┘          │
│          │                                                                │
│          ▼                                                                │
│  ┌───────────────┐     ┌───────────────┐     ┌───────────────┐          │
│  │FVoxelStampRtm │────►│  AVoxelWorld  │────►│ FVoxelRuntime │          │
│  │  (Runtime)    │     │   (Actor)     │     │ (Background)  │          │
│  └───────────────┘     └───────────────┘     └───────┬───────┘          │
│                                                       │                   │
│                                                       ▼                   │
│  ┌───────────────┐     ┌───────────────┐     ┌───────────────┐          │
│  │  Nanite Mesh  │◄────│VoxelChunkRndr │◄────│ LOD Selection │          │
│  │  (Rendering)  │     │   (Proxy)     │     │ (Streaming)   │          │
│  └───────────────┘     └───────────────┘     └───────────────┘          │
│                                                                           │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Key Properties: AVoxelWorld

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `VoxelSize` | int32 | 100 | Voxel size in cm (1m default) |
| `LODQuality` | FVoxelLODQuality | - | Min/Max quality for LOD |
| `QualityExponent` | double | 1.0 | LOD selection bias |
| `MegaMaterial` | UVoxelMegaMaterial* | nullptr | Multi-material rendering |
| `LayerStack` | UVoxelLayerStack* | nullptr | Layer configuration |
| `bEnableNanite` | bool | true | Enable Nanite rendering |
| `bCreateRuntimeOnBeginPlay` | bool | true | Auto-create runtime |
| `bWaitOnBeginPlay` | bool | true | Wait for generation |

### Collision Settings
| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `CollisionChunkSize` | int32 | 32 | Collision chunk size |
| `bDoubleSidedCollision` | bool | false | Double-sided collision |

### Nanite Settings
| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `bEnableTessellation` | bool | true | Nanite tessellation |
| `NaniteMaxTessellationLOD` | int32 | 2 | Max LOD for tessellation |
| `NanitePositionPrecision` | int32 | 6 | Position bit precision |

### Rendering Settings
| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `bEnableLumen` | bool | false | Lumen GI support |
| `bEnableRaytracing` | bool | false | RT effects support |
| `bGenerateMeshDistanceFields` | bool | false | MDF support |
| `RenderChunkSize` | EVoxelRenderChunkSize | Size32 | Render chunk size |

---

## Threading Model

VoxelPlugin2 uses a task-based async system with `FVoxelFuture`:

```cpp
// Asynchronous pattern
FVoxelFuture SculptSphere(...)
{
    return Voxel::AsyncTask([=]
    {
        // Background work
        return Result;
    })
    .Then_GameThread([=](Result)
    {
        // Game thread callback
    });
}
```

**Key Patterns:**
- `Voxel::AsyncTask()` - Queue background work
- `Voxel::GameTask()` - Queue game thread work
- `.Then()` - Chain async operations
- `.Then_GameThread()` - Chain with game thread callback

---

## Plugin Configuration

### In .uproject:
```json
{
  "Plugins": [
    {
      "Name": "Voxel",
      "Enabled": true
    },
    {
      "Name": "PCG",
      "Enabled": true
    }
  ]
}
```

### In Build.cs:
```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    "Voxel",
    "VoxelGraph"
});
```

---

## ISPC Integration

VoxelCore includes ISPC (Intel SPMD Program Compiler) integration for SIMD-optimized operations:

**Supported targets:**
- Windows x64: AVX512, AVX2, AVX, SSE4
- Linux x64: AVX512, AVX2, AVX, SSE4
- macOS x64: AVX512, AVX2, AVX, SSE4
- macOS ARM64: NEON
- iOS/VisionOS: NEON
- Android ARM64: NEON

ISPC files (`.ispc`, `.isph`) are auto-compiled during build.

---

## File Organization

```
Source/
├── VoxelCore/
│   ├── Public/
│   │   ├── VoxelMinimal.h              # Main include
│   │   ├── VoxelMinimal/
│   │   │   ├── Containers/             # VoxelArray, VoxelMap, etc.
│   │   │   └── Utilities/              # Math, geometry, etc.
│   │   └── ...
│   └── Private/
│
├── VoxelGraph/
│   ├── Public/
│   │   ├── VoxelGraph.h                # UVoxelGraph
│   │   ├── VoxelNode.h                 # FVoxelNode
│   │   ├── Buffer/                     # FVoxelBuffer types
│   │   └── Nodes/                      # Built-in nodes
│   └── Private/
│
├── Voxel/
│   ├── Public/
│   │   ├── VoxelWorld.h                # AVoxelWorld
│   │   ├── VoxelLayerStack.h           # UVoxelLayerStack
│   │   ├── VoxelStamp.h                # FVoxelStamp
│   │   ├── Graphs/                     # Height/Volume graphs
│   │   ├── Sculpt/                     # Sculpting system
│   │   ├── Collision/                  # Physics
│   │   ├── Render/                     # Rendering
│   │   └── ...
│   └── Private/
│
└── VoxelPCG/
    ├── Public/
    └── Private/
```

---

## Related Topics

- [02-voxelgraph-system.md](02-voxelgraph-system.md) - Node system details
- [04-voxelworld-layerstack.md](04-voxelworld-layerstack.md) - World configuration
- [07-stamp-system.md](07-stamp-system.md) - Stamp architecture
- [13-collision-rendering.md](13-collision-rendering.md) - Rendering details
