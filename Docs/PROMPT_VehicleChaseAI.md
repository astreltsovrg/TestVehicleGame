# Vehicle Chase AI Implementation Task

## Context
This is TestVehicleGame project - a vehicle demo using Chaos Vehicles physics in Unreal Engine 5.7.
The project has MCP (Model Context Protocol) tools for Blueprint manipulation and AI asset creation.

## Task
Create an AI-controlled vehicle that:
1. Chases the player vehicle
2. Stops when within 10 meters (1000 units) of the target
3. Resumes chase if player moves away

## Technical Requirements

### 1. C++ BTTask_VehicleChase
Create in `TestVehicleGame/Source/TestVehicleGame/AI/`:

```cpp
// BTTask_VehicleChase.h
UCLASS()
class UBTTask_VehicleChase : public UBTTask_BlueprintBase
{
    GENERATED_BODY()
public:
    UBTTask_VehicleChase();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

protected:
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector TargetActorKey;

    UPROPERTY(EditAnywhere, Category = "Chase")
    float StopDistance = 1000.0f; // 10 meters

    UPROPERTY(EditAnywhere, Category = "Chase")
    float MaxSpeed = 1.0f; // Throttle multiplier
};
```

**Logic in TickTask:**
- Get target actor location
- Get vehicle location and forward vector
- Calculate: angle to target, distance
- Steering = angle normalized to [-1, 1]
- Throttle = 1.0 if distance > StopDistance, else 0
- Apply via `UChaosWheeledVehicleMovementComponent::SetThrottleInput()` / `SetSteeringInput()`

### 2. AI Assets (create via MCP)
```python
# Blackboard
ai_asset(action="create_blackboard", path="/Game/AI/BB_VehicleChase", keys=[
    {"name": "TargetActor", "type": "object", "base_class": "Actor"}
])

# BehaviorTree
ai_asset(action="create_bt", path="/Game/AI/BT_VehicleChase",
         blackboard="/Game/AI/BB_VehicleChase")

# Add root Selector -> Sequence -> our C++ task
ai_asset(action="add_bt_node", path="/Game/AI/BT_VehicleChase",
         node_type="composite", node_class="Selector")
```

### 3. AI Vehicle Pawn
Option A: Duplicate existing vehicle Blueprint, set AIControllerClass
Option B: Create C++ `ATestVehicleGameAIPawn` with AIController setup

### 4. Spawn & Test
```python
# Start PIE
editor(action="start_pie")

# Spawn AI vehicle
ai_runtime(action="spawn", class="/Game/Vehicles/BP_AIVehicle.BP_AIVehicle_C",
           location={"x": 1000, "y": 0, "z": 100})

# Set target to player
ai_runtime(action="bb_set", actor="BP_AIVehicle_0", key="TargetActor", value="BP_SportsCar_0")

# Run behavior tree
ai_runtime(action="run_bt", actor="BP_AIVehicle_0", tree="/Game/AI/BT_VehicleChase")
```

## Files to Create/Modify
1. `Source/TestVehicleGame/AI/BTTask_VehicleChase.h`
2. `Source/TestVehicleGame/AI/BTTask_VehicleChase.cpp`
3. `Source/TestVehicleGame/TestVehicleGame.Build.cs` - add AIModule dependency
4. AI assets via MCP

## Key Classes Reference
- `AWheeledVehiclePawn` - base vehicle pawn
- `UChaosWheeledVehicleMovementComponent` - vehicle physics
- `UBTTaskNode` - BT task base class
- `FBlackboardKeySelector` - BB key reference
- `AAIController` - AI controller

## Success Criteria
- AI vehicle spawns and idles
- When player vehicle exists, AI chases it
- AI stops ~10m from player
- AI resumes if player drives away
