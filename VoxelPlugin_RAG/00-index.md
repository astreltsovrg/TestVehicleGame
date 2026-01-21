# VoxelPlugin2 RAG Knowledge Base

## Overview

**VoxelPlugin2** is a state-of-the-art voxel terrain system for Unreal Engine 5, providing procedural terrain generation, runtime sculpting, and advanced rendering features.

| Property | Value |
|----------|-------|
| **Version** | 2.0p8 (2000008) |
| **Category** | Landscape |
| **UE Version** | 5.7 |
| **Documentation** | https://docs.voxelplugin.com |
| **Support** | https://discord.voxelplugin.com |

---

## Quick Start

### 1. Enable the Plugin
In your `.uproject` file:
```json
{
  "Plugins": [
    { "Name": "Voxel", "Enabled": true }
  ]
}
```

### 2. Create a Voxel World
1. Place `AVoxelWorld` actor in your level
2. Assign a `UVoxelLayerStack` to configure terrain layers
3. Add stamps (height graphs, volume sculpts) to generate terrain

### 3. Basic Runtime Sculpting
```cpp
// In Blueprint or C++
AVoxelVolumeSculptActor* SculptActor = GetSculptActor();
UVoxelVolumeSculptBlueprintLibrary::SculptSphere(
    SculptActor,
    HitLocation,      // Center
    500.f,            // Radius
    EVoxelSculptMode::Add,
    100.f             // Smoothness
);
```

---

## Documentation Index

### Phase 1: Foundation
| File | Topic | Description |
|------|-------|-------------|
| [01-architecture.md](01-architecture.md) | Architecture | Module structure, dependencies, core concepts |
| [02-voxelgraph-system.md](02-voxelgraph-system.md) | VoxelGraph | Node system, buffers, compilation |
| [03-noise-generation.md](03-noise-generation.md) | Noise | AdvancedNoise2D/3D, FBM parameters |

### Phase 2: World & Terrain
| File | Topic | Description |
|------|-------|-------------|
| [04-voxelworld-layerstack.md](04-voxelworld-layerstack.md) | VoxelWorld | AVoxelWorld, UVoxelLayerStack configuration |
| [05-height-layer-system.md](05-height-layer-system.md) | Height Layers | 2D heightfields, height graphs |
| [06-volume-layer-system.md](06-volume-layer-system.md) | Volume Layers | 3D volumetric terrain, SDF |

### Phase 3: Runtime Modification
| File | Topic | Description |
|------|-------|-------------|
| [07-stamp-system.md](07-stamp-system.md) | Stamps | FVoxelStamp, FVoxelStampRuntime, registration |
| [08-runtime-sculpting.md](08-runtime-sculpting.md) | **Runtime Sculpting** | Blueprint API for terrain editing |
| [09-persistence-saveload.md](09-persistence-saveload.md) | **Persistence** | Save/Load sculpt data |

### Phase 4: Advanced Features
| File | Topic | Description |
|------|-------|-------------|
| [10-spline-system.md](10-spline-system.md) | Splines | Roads, rivers, terrain paths |
| [11-heightmap-import.md](11-heightmap-import.md) | Heightmaps | Import real-world terrain data |
| [12-pcg-integration.md](12-pcg-integration.md) | PCG | UE5 Procedural Content Generation |
| [13-collision-rendering.md](13-collision-rendering.md) | Collision & Rendering | Physics, Nanite support |
| [14-query-system.md](14-query-system.md) | Queries | Runtime terrain sampling |
| [15-scatter-foliage.md](15-scatter-foliage.md) | Scatter | Procedural foliage placement |
| [16-megamaterial-system.md](16-megamaterial-system.md) | MegaMaterial | Multi-material terrain |

### Appendix
| File | Topic | Description |
|------|-------|-------------|
| [appendix/class-hierarchy.md](appendix/class-hierarchy.md) | Class Hierarchy | Full inheritance tree |
| [appendix/api-quick-reference.md](appendix/api-quick-reference.md) | API Reference | One-page cheat sheet |
| [appendix/common-patterns.md](appendix/common-patterns.md) | Patterns | Working code snippets |

---

## Use Case Routing

### Runtime Sculpting (Minecraft-like)
**Read:** 01 → 04 → 06 → 07 → **08** → **09**

Key features:
- Dig/add terrain at runtime
- Save/load player modifications
- Vehicle wheel tracks, destruction

### Procedural Terrain Generation
**Read:** 01 → **02** → **03** → 04 → 05 → 11

Key features:
- Noise-based landscapes
- Heightmap import
- Seed-based determinism

### Roads/Rivers/Paths
**Read:** 01 → 07 → **10**

Key features:
- Spline-based terrain modification
- Height and volume spline stamps

### Foliage/Instance Scattering
**Read:** 01 → 04 → **15** → 12

Key features:
- Procedural foliage placement
- PCG integration

---

## Module Overview

```
VoxelCore (foundation)
    ↓
VoxelGraph (node system)
    ↓
Voxel (world, stamps, sculpting)
    ↓
VoxelPCG (PCG integration)
```

| Module | Type | Headers | Purpose |
|--------|------|---------|---------|
| VoxelCore | Runtime | 145 | Containers, math, memory |
| VoxelGraph | Runtime | 134 | Node system, buffers |
| Voxel | Runtime | 320 | Main terrain system |
| VoxelPCG | Runtime | 30 | PCG integration |
| VoxelBlueprint | UncookedOnly | 5 | Blueprint wrappers |
| VoxelCoreEditor | Editor | 37 | Core editor tools |
| VoxelGraphEditor | Editor | 108 | Graph editor |
| VoxelEditor | Editor | 59 | Main editor UI |

---

## Key Classes

| Class | Module | Purpose |
|-------|--------|---------|
| `AVoxelWorld` | Voxel | Main terrain actor |
| `UVoxelLayerStack` | Voxel | Layer configuration |
| `UVoxelHeightLayer` | Voxel | 2D height layer |
| `UVoxelVolumeLayer` | Voxel | 3D volume layer |
| `FVoxelStamp` | Voxel | Base stamp struct |
| `FVoxelStampRuntime` | Voxel | Runtime stamp |
| `AVoxelVolumeSculptActor` | Voxel | Volume sculpting |
| `UVoxelGraph` | VoxelGraph | Visual graph asset |
| `FVoxelNode` | VoxelGraph | Base node struct |
| `UVoxelMegaMaterial` | Voxel | Multi-material rendering |

---

## Version Compatibility

- **Unreal Engine:** 5.7
- **VoxelPlugin:** 2.0p8
- **Dependencies:** PCG Plugin (required)
