// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "VehicleChaseAIController.generated.h"

class UBehaviorTree;
class UBlackboardData;

/**
 * AI Controller that runs a Behavior Tree for vehicle chase behavior.
 * Automatically sets up Blackboard and starts BT on possess.
 */
UCLASS()
class TESTVEHICLEGAME_API AVehicleChaseAIController : public AAIController
{
	GENERATED_BODY()

public:
	AVehicleChaseAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void BeginPlay() override;

	/** The Behavior Tree to run */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	UBehaviorTree* BehaviorTreeAsset;

	/** The Blackboard asset to use (optional - if not set, uses BT's default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	UBlackboardData* BlackboardAsset;

	/** Target actor to chase (set in Blackboard) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	AActor* TargetActor;

protected:
	/** Set initial Blackboard values (target actor, etc.) */
	void SetupBlackboardValues();
};
