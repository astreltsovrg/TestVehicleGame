// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/ValueOrBBKey.h"
#include "BTTask_VehicleChase.generated.h"

class UBehaviorTree;

/**
 * Behavior Tree task that makes a vehicle chase a target actor.
 * The vehicle will steer towards the target and apply throttle until within StopDistance.
 */
UCLASS()
class TESTVEHICLEGAME_API UBTTask_VehicleChase : public UBTTaskNode
{
	GENERATED_UCLASS_BODY()

	/** Blackboard key for the target actor */
	UPROPERTY(Category = Blackboard, EditAnywhere)
	struct FBlackboardKeySelector BlackboardKey;

	/** Distance at which the vehicle stops chasing (in Unreal units, 1000 = 10 meters) */
	UPROPERTY(Category = Node, EditAnywhere)
	FValueOrBBKey_Float StopDistance = 1000.0f;

	/** Distance at which the vehicle starts braking (should be > StopDistance) */
	UPROPERTY(Category = Node, EditAnywhere)
	FValueOrBBKey_Float BrakeDistance = 2000.0f;

	/** Maximum steering angle in radians used for normalizing steering input */
	UPROPERTY(Category = Node, EditAnywhere)
	FValueOrBBKey_Float MaxSteeringAngle = 1.5708f;

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	/** Get name of selected blackboard key */
	FName GetSelectedBlackboardKey() const { return BlackboardKey.SelectedKeyName; }
};
