// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/BTTask_VehicleChase.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "WheeledVehiclePawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Kismet/GameplayStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BTTask_VehicleChase)

UBTTask_VehicleChase::UBTTask_VehicleChase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = "Vehicle Chase";

	// Явно включаем флаги
	bNotifyTick = true;           // Вызывать TickTask каждый кадр
	bNotifyTaskFinished = false;  // Не нужен OnTaskFinished
	bIgnoreRestartSelf = false;

	// Фильтр для Blackboard ключа - только Object типа Actor
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_VehicleChase, BlackboardKey), AActor::StaticClass());
}

void UBTTask_VehicleChase::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		BlackboardKey.ResolveSelectedKey(*BBAsset);
	}
}

EBTNodeResult::Type UBTTask_VehicleChase::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UE_LOG(LogTemp, Warning, TEXT("VehicleChase: ExecuteTask called"));

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController || !AIController->GetPawn())
	{
		UE_LOG(LogTemp, Warning, TEXT("VehicleChase: No AIController or Pawn!"));
		return EBTNodeResult::Failed;
	}

	AWheeledVehiclePawn* VehiclePawn = Cast<AWheeledVehiclePawn>(AIController->GetPawn());
	if (!VehiclePawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("VehicleChase: Pawn is not AWheeledVehiclePawn!"));
		return EBTNodeResult::Failed;
	}

	UE_LOG(LogTemp, Warning, TEXT("VehicleChase: ExecuteTask returning InProgress"));
	return EBTNodeResult::InProgress;
}

void UBTTask_VehicleChase::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return;
	}

	AWheeledVehiclePawn* VehiclePawn = Cast<AWheeledVehiclePawn>(AIController->GetPawn());
	if (!VehiclePawn)
	{
		return;
	}

	UChaosWheeledVehicleMovementComponent* Movement =
		Cast<UChaosWheeledVehicleMovementComponent>(VehiclePawn->GetVehicleMovement());
	if (!Movement)
	{
		return;
	}

	// Get target from blackboard
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AActor* Target = Cast<AActor>(BB->GetValueAsObject(GetSelectedBlackboardKey()));

	// Auto-find player if no target set
	if (!Target)
	{
		Target = UGameplayStatics::GetPlayerPawn(AIController->GetWorld(), 0);
		if (Target)
		{
			// Store in Blackboard for future use
			BB->SetValueAsObject(GetSelectedBlackboardKey(), Target);
			UE_LOG(LogTemp, Warning, TEXT("VehicleChase: Auto-targeted player: %s"), *Target->GetName());
		}
		else
		{
			// Still no player - wait
			Movement->SetThrottleInput(0.0f);
			Movement->SetSteeringInput(0.0f);
			Movement->SetBrakeInput(1.0f);
			return;
		}
	}

	FVector VehicleLocation = VehiclePawn->GetActorLocation();
	FVector TargetLocation = Target->GetActorLocation();
	float Distance = FVector::Dist(VehicleLocation, TargetLocation);

	// Debug: log if target seems invalid or very far
	if (Distance > 50000.0f) // > 500 meters
	{
		UE_LOG(LogTemp, Warning, TEXT("VehicleChase: Target very far! Distance=%.0f, Target=%s at %s"),
			Distance, *Target->GetName(), *TargetLocation.ToString());
	}

	// Calculate steering angle to target
	FVector ToTarget = (TargetLocation - VehicleLocation).GetSafeNormal();
	FVector Forward = VehiclePawn->GetActorForwardVector();

	// 2D steering calculation (ignore Z for ground vehicles)
	FVector ToTarget2D = FVector(ToTarget.X, ToTarget.Y, 0.0f).GetSafeNormal();
	FVector Forward2D = FVector(Forward.X, Forward.Y, 0.0f).GetSafeNormal();

	// Calculate signed angle using cross product and dot product
	float CrossZ = Forward2D.X * ToTarget2D.Y - Forward2D.Y * ToTarget2D.X;
	float Dot = Forward2D.X * ToTarget2D.X + Forward2D.Y * ToTarget2D.Y;
	float AngleToTarget = FMath::Atan2(CrossZ, Dot);

	// More aggressive steering - use 0.5 radians (~30 degrees) for full lock
	float Steering = FMath::Clamp(AngleToTarget / 0.5f, -1.0f, 1.0f);

	// Calculate throttle and brake based on distance and angle
	float Throttle = 0.0f;
	float Brake = 0.0f;

	// Check if target is behind us (angle > 90 degrees)
	bool bTargetBehind = Dot < 0.0f;
	float AbsAngle = FMath::Abs(AngleToTarget);

	// Get values from FValueOrBBKey_Float (supports both direct values and blackboard keys)
	const float StopDist = StopDistance.GetValue(OwnerComp);
	const float BrakeDist = BrakeDistance.GetValue(OwnerComp);

	if (Distance <= StopDist)
	{
		// Within stop distance - full brake
		Throttle = 0.0f;
		Brake = 1.0f;
	}
	else if (bTargetBehind)
	{
		// Target is behind - do a U-turn (not reverse!)
		// Apply full steering and moderate throttle to turn around
		Throttle = 0.5f;
		Brake = 0.0f;
		// Full steering in the direction of target
		Steering = (AngleToTarget > 0.0f) ? 1.0f : -1.0f;
	}
	else if (AbsAngle > 1.0f)
	{
		// Sharp turn needed (> ~57 degrees) - slow down and steer hard
		Throttle = 0.4f;
		Brake = 0.0f;
	}
	else if (Distance > BrakeDist)
	{
		// Far and facing target - full throttle
		Throttle = 1.0f;
		Brake = 0.0f;
	}
	else
	{
		// In braking zone - gradual slowdown
		float BrakeZoneRatio = (BrakeDist - Distance) / (BrakeDist - StopDist);
		Throttle = 1.0f - BrakeZoneRatio;
		Brake = BrakeZoneRatio;
	}

	// Apply inputs to vehicle
	Movement->SetSteeringInput(Steering);
	Movement->SetThrottleInput(Throttle);
	Movement->SetBrakeInput(Brake);
}
