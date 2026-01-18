// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/VehicleChaseAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Kismet/GameplayStatics.h"

AVehicleChaseAIController::AVehicleChaseAIController()
{
	PrimaryActorTick.bCanEverTick = false; // BT handles updates now
}

void AVehicleChaseAIController::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("VehicleChaseAI: BeginPlay, BehaviorTreeAsset = %s"),
		BehaviorTreeAsset ? *BehaviorTreeAsset->GetName() : TEXT("NULL"));
}

void AVehicleChaseAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	UE_LOG(LogTemp, Warning, TEXT("VehicleChaseAI: Possessed %s"), *InPawn->GetName());

	if (BehaviorTreeAsset)
	{
		// Initialize Blackboard BEFORE running BT (UseBlackboard creates the component)
		if (BlackboardAsset)
		{
			UBlackboardComponent* BBComp = nullptr;
			UseBlackboard(BlackboardAsset, BBComp);
			UE_LOG(LogTemp, Warning, TEXT("VehicleChaseAI: UseBlackboard with %s"), *BlackboardAsset->GetName());
		}

		// Run the Behavior Tree
		bool bSuccess = RunBehaviorTree(BehaviorTreeAsset);
		UE_LOG(LogTemp, Warning, TEXT("VehicleChaseAI: RunBehaviorTree returned %s"),
			bSuccess ? TEXT("TRUE") : TEXT("FALSE"));

		// Set initial Blackboard values AFTER BT is running (BB component now exists)
		if (bSuccess)
		{
			SetupBlackboardValues();
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("VehicleChaseAI: No BehaviorTreeAsset set!"));
	}
}

void AVehicleChaseAIController::SetupBlackboardValues()
{
	UBlackboardComponent* BB = GetBlackboardComponent();
	if (!BB)
	{
		UE_LOG(LogTemp, Warning, TEXT("VehicleChaseAI: No BlackboardComponent yet"));
		return;
	}

	// If no target specified, find player
	if (!TargetActor)
	{
		TargetActor = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		UE_LOG(LogTemp, Warning, TEXT("VehicleChaseAI: Auto-targeting player: %s"),
			TargetActor ? *TargetActor->GetName() : TEXT("NULL"));
	}

	// Set target in Blackboard
	if (TargetActor)
	{
		BB->SetValueAsObject(FName("TargetActor"), TargetActor);
		UE_LOG(LogTemp, Warning, TEXT("VehicleChaseAI: Set Blackboard TargetActor = %s"),
			*TargetActor->GetName());
	}
}
