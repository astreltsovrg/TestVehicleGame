# VoxelGraph System

## Summary

The VoxelGraph system is a visual programming framework for procedural terrain generation. It uses a node-based approach where `FVoxelNode` structs define computation logic, connected through typed pins, and compiled into executable code via `FVoxelCompiledGraph`.

---

## Key Classes

| Class | Header | Purpose |
|-------|--------|---------|
| `UVoxelGraph` | VoxelGraph.h | Main graph asset (inherits UVoxelAsset) |
| `FVoxelNode` | VoxelNode.h | Base struct for all graph nodes |
| `UVoxelTerminalGraph` | VoxelGraph.h | Terminal/output graph container |
| `FVoxelCompiledGraph` | VoxelGraph.h | Compiled executable graph |
| `FVoxelPin` | VoxelPin.h | Node input/output connection |
| `FVoxelPinType` | VoxelPinType.h | Pin type information |
| `FVoxelGraphQuery` | VoxelGraphQuery.h | Execution context |

---

## UVoxelGraph Asset

The main graph asset that contains nodes, parameters, and terminal graphs.

### Key Properties

```cpp
UCLASS(meta = (VoxelAssetType, AssetColor=Blue))
class UVoxelGraph : public UVoxelAsset, public IVoxelParameterOverridesObjectOwner
{
    // Editor-only properties
    UPROPERTY(EditAnywhere, Category = "Config")
    FString Category;

    UPROPERTY(AssetRegistrySearchable, EditAnywhere, Category = "Config")
    TSet<FName> GraphTags;

    UPROPERTY(EditAnywhere, Category = "Config")
    FString Description;

    UPROPERTY(EditAnywhere, Category = "Config", AdvancedDisplay)
    bool bShowInContextMenu = true;

private:
    UPROPERTY()
    TObjectPtr<UVoxelGraph> PrivateBaseGraph;  // Inheritance support

    UPROPERTY()
    TMap<FGuid, TObjectPtr<UVoxelTerminalGraph>> GuidToTerminalGraph;

    UPROPERTY()
    TMap<FGuid, FVoxelParameter> GuidToParameter;
};
```

### Key Methods

| Method | Description |
|--------|-------------|
| `GetMainTerminalGraph()` | Get the primary output terminal |
| `GetEditorTerminalGraph()` | Get editor preview terminal |
| `GetCompiledGraph(DependencyCollector)` | Get compiled executable |
| `FindParameter(Guid)` | Find parameter by GUID |
| `GetBaseGraphs()` | Get inheritance chain (includes self) |
| `IsFunctionLibrary()` | Check if graph is a function library |

### Graph Inheritance

VoxelGraphs support inheritance:
```cpp
// Get base graph (unsafe - doesn't check loops)
UVoxelGraph* BaseGraph = Graph->GetBaseGraph_Unsafe();

// Get full inheritance chain (safe)
TVoxelInlineArray<const UVoxelGraph*, 1> Chain = Graph->GetBaseGraphs();
```

---

## FVoxelNode Base Class

The foundation for all node types. Nodes are structs, not UObjects.

### Class Declaration

```cpp
USTRUCT(meta = (Abstract))
struct FVoxelNode : public FVoxelVirtualStruct, public IVoxelNodeInterface
{
    GENERATED_BODY()
    DECLARE_VIRTUAL_STRUCT_PARENT(FVoxelNode, GENERATED_VOXEL_NODE_BODY)

    UPROPERTY(EditAnywhere, Category = "Config")
    FGuid NodeGuid;

    // Core computation function - override this
    virtual void Compute(FVoxelGraphQuery Query) const VOXEL_PURE_VIRTUAL();

    // Compile-time computation for specific pin
    virtual void ComputeNoCachePin(FVoxelGraphQuery Query, int32 PinIndex) const;

    // Node behavior flags
    virtual bool IsPureNode() const { return false; }
    virtual bool HasGuid() const { return false; }
    virtual bool CanBeQueried() const { return false; }
    virtual bool CanBeDuplicated() const { return true; }
    virtual bool CanBeDeleted() const { return true; }
};
```

### Pin System

Nodes declare pins using typed references:

```cpp
// Pin reference types
struct FPinRef_Input;           // Basic input pin
struct FPinRef_Output;          // Basic output pin
struct FVariadicPinRef_Input;   // Variadic input (multiple pins)

template<typename T> struct TPinRef_Input;   // Typed input
template<typename T> struct TPinRef_Output;  // Typed output
```

### Creating Pins

```cpp
// In custom node initialization
FPinRef_Input CreateInputPin(
    const FVoxelPinType& Type,
    FName Name,
    const FVoxelPinMetadata& Metadata = {},
    EVoxelPinFlags Flags = EVoxelPinFlags::None);

FPinRef_Output CreateOutputPin(
    const FVoxelPinType& Type,
    FName Name,
    const FVoxelPinMetadata& Metadata = {},
    EVoxelPinFlags Flags = EVoxelPinFlags::None);

// Variadic pins (can have multiple connections)
FVariadicPinRef_Input CreateVariadicInputPin(
    const FVoxelPinType& Type,
    FName Name,
    const FVoxelPinMetadata& Metadata,
    int32 MinNum,
    EVoxelPinFlags Flags = EVoxelPinFlags::None);
```

---

## Creating Custom Nodes

### Basic Custom Node

```cpp
// MyCustomNode.h
#pragma once
#include "VoxelNode.h"
#include "MyCustomNode.generated.h"

USTRUCT(Category = "Custom")
struct FVoxelNode_MyCustom : public FVoxelNode
{
    GENERATED_BODY()
    GENERATED_VOXEL_NODE_BODY()

    // Input pins
    VOXEL_INPUT_PIN(FVoxelFloatBuffer, InputValue, 0.f);
    VOXEL_INPUT_PIN(float, Multiplier, 1.f);

    // Output pins
    VOXEL_OUTPUT_PIN(FVoxelFloatBuffer, Result);

    virtual void Compute(FVoxelGraphQuery Query) const override;
};

// MyCustomNode.cpp
void FVoxelNode_MyCustom::Compute(FVoxelGraphQuery Query) const
{
    // Get input values
    const FVoxelFloatBuffer& Input = Query.Get(InputValue);
    const float Mult = Query.Get(Multiplier);

    // Create output buffer
    FVoxelFloatBuffer OutputBuffer;
    OutputBuffer.SetNum(Input.Num());

    // Process
    for (int32 i = 0; i < Input.Num(); i++)
    {
        OutputBuffer[i] = Input[i] * Mult;
    }

    // Set output
    Query.Set(Result, MoveTemp(OutputBuffer));
}
```

### Pin Declaration Macros

```cpp
// Basic input pin with default value
VOXEL_INPUT_PIN(Type, Name, DefaultValue);

// Input pin without default
VOXEL_INPUT_PIN(Type, Name);

// Output pin
VOXEL_OUTPUT_PIN(Type, Name);

// Template pins (type determined by connection)
VOXEL_TEMPLATE_INPUT_PIN(Type, Name);
VOXEL_TEMPLATE_OUTPUT_PIN(Type, Name);
```

### Node Categories

Nodes are organized by category in the editor:

```cpp
USTRUCT(Category = "Math")          // Math operations
USTRUCT(Category = "Noise")         // Noise generation
USTRUCT(Category = "Sculpt")        // Sculpting operations
USTRUCT(Category = "Surface")       // Surface/material
USTRUCT(Category = "Custom")        // Custom nodes
```

---

## Buffer Types

VoxelGraph uses specialized buffer types for efficient data passing:

| Buffer Type | C++ Type | Description |
|------------|----------|-------------|
| `FVoxelFloatBuffer` | `TVoxelArray<float>` | Float values |
| `FVoxelIntBuffer` | `TVoxelArray<int32>` | Integer values |
| `FVoxelBoolBuffer` | `TVoxelArray<bool>` | Boolean values |
| `FVoxelVectorBuffer` | `TVoxelArray<FVector>` | 3D vectors |
| `FVoxelVector2DBuffer` | `TVoxelArray<FVector2D>` | 2D vectors |
| `FVoxelLinearColorBuffer` | `TVoxelArray<FLinearColor>` | Colors |
| `FVoxelSeedBuffer` | `TVoxelArray<FVoxelSeed>` | Random seeds |

### Buffer Operations

```cpp
// Create buffer
FVoxelFloatBuffer Buffer;
Buffer.SetNum(1024);

// Fill buffer
for (int32 i = 0; i < Buffer.Num(); i++)
{
    Buffer[i] = ComputeValue(i);
}

// Parallel operations
Voxel::ParallelFor(Buffer.Num(), [&](int32 Index)
{
    Buffer[Index] = ComputeValue(Index);
});
```

---

## Graph Execution

### FVoxelGraphQuery

The execution context passed to `Compute()`:

```cpp
struct FVoxelGraphQuery
{
    // Get input pin value
    template<typename T>
    const T& Get(const TPinRef_Input<T>& Pin) const;

    // Set output pin value
    template<typename T>
    void Set(const TPinRef_Output<T>& Pin, T&& Value) const;

    // Get query position (for spatial nodes)
    FVoxelVector GetPosition() const;

    // Get LOD level
    int32 GetLOD() const;

    // Access world context
    UWorld* GetWorld() const;
};
```

### Compilation Pipeline

```
UVoxelGraph
    │
    ▼
UVoxelTerminalGraph (output terminal)
    │
    ▼ [Compilation]
FVoxelCompiledGraph
    │
    ▼ [Execution]
FVoxelGraphQuery → FVoxelNode::Compute()
```

### Getting Compiled Graph

```cpp
// With dependency tracking
FVoxelDependencyCollector DependencyCollector;
TSharedRef<const FVoxelCompiledGraph> Compiled = Graph->GetCompiledGraph(DependencyCollector);
```

---

## Terminal Graphs

Terminal graphs define output types (height, volume, etc.):

### Main Terminal Graph GUID

```cpp
// Standard GUIDs for terminal graphs
constexpr FVoxelGuid GVoxelMainTerminalGraphGuid =
    VOXEL_GUID("00000000FFFFFFFF0000000029A672A2");

constexpr FVoxelGuid GVoxelEditorTerminalGraphGuid =
    VOXEL_GUID("00000000FFFFFFFF000000007FEB8321");
```

### Accessing Terminal Graphs

```cpp
// Main output terminal
UVoxelTerminalGraph& MainTerminal = Graph->GetMainTerminalGraph();

// Editor preview terminal
UVoxelTerminalGraph& EditorTerminal = Graph->GetEditorTerminalGraph();

// Find by GUID
UVoxelTerminalGraph* Terminal = Graph->FindTerminalGraph(Guid);
```

---

## Parameters System

Graphs support parameters that can be exposed and overridden:

```cpp
struct FVoxelParameter
{
    FName Name;
    FVoxelPinType Type;
    FVoxelPinValue DefaultValue;
    FString Description;
    // ...
};

// Find parameter
const FVoxelParameter* Param = Graph->FindParameter(ParameterGuid);

// Iterate parameters
Graph->ForeachParameter([](const FGuid& Guid, const FVoxelParameter& Param)
{
    UE_LOG(LogVoxel, Log, TEXT("Parameter: %s"), *Param.Name.ToString());
});
```

---

## Pin Flags

```cpp
enum class EVoxelPinFlags : uint32
{
    None             = 0,
    TemplatePin      = 1 << 0,  // Type determined by connection
    VariadicPin      = 1 << 1,  // Can have multiple connections
    ArrayPin         = 1 << 2,  // Array type
    NoDefault        = 1 << 3,  // No default value
    Hidden           = 1 << 4,  // Hidden in editor
    AdvancedDisplay  = 1 << 5,  // Advanced display
    // ...
};
```

---

## Common Patterns

### Pure Computation Node

```cpp
USTRUCT(Category = "Math")
struct FVoxelNode_Add : public FVoxelNode
{
    GENERATED_BODY()
    GENERATED_VOXEL_NODE_BODY()

    VOXEL_TEMPLATE_INPUT_PIN(FVoxelFloatBuffer, A);
    VOXEL_TEMPLATE_INPUT_PIN(FVoxelFloatBuffer, B);
    VOXEL_TEMPLATE_OUTPUT_PIN(FVoxelFloatBuffer, Result);

    virtual bool IsPureNode() const override { return true; }

    virtual void Compute(FVoxelGraphQuery Query) const override
    {
        const auto& BufferA = Query.Get(A);
        const auto& BufferB = Query.Get(B);

        FVoxelFloatBuffer Output;
        // ... add buffers ...
        Query.Set(Result, MoveTemp(Output));
    }
};
```

### Node with World Access

```cpp
USTRUCT(Category = "World")
struct FVoxelNode_WorldQuery : public FVoxelNode
{
    GENERATED_BODY()
    GENERATED_VOXEL_NODE_BODY()

    VOXEL_OUTPUT_PIN(FVoxelFloatBuffer, DistanceToPlayer);

    virtual void Compute(FVoxelGraphQuery Query) const override
    {
        UWorld* World = Query.GetWorld();
        if (!World) return;

        APawn* Player = World->GetFirstPlayerController()->GetPawn();
        // ... compute distance ...
    }
};
```

---

## Graph Specializations

VoxelPlugin includes specialized graph types:

| Graph Type | Purpose | Output Node |
|------------|---------|-------------|
| `UVoxelHeightGraph` | 2D height generation | OutputHeight |
| `UVoxelVolumeGraph` | 3D volume generation | OutputVolume |
| `UVoxelSculptGraph` | Sculpting operations | OutputSculptDistance |
| `UVoxelScatterGraph` | Foliage scattering | OutputScatter |

---

## Performance Considerations

1. **Buffer Reuse**: Avoid allocating new buffers in hot paths
2. **Parallel Processing**: Use `Voxel::ParallelFor` for large buffers
3. **Template Pins**: Minimize type conversions
4. **Pure Nodes**: Mark nodes as pure when possible for caching
5. **Dependency Tracking**: Use `FVoxelDependencyCollector` for change detection

---

## Related Topics

- [03-noise-generation.md](03-noise-generation.md) - Noise nodes
- [05-height-layer-system.md](05-height-layer-system.md) - Height graphs
- [06-volume-layer-system.md](06-volume-layer-system.md) - Volume graphs
