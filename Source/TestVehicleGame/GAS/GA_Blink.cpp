// Copyright Epic Games, Inc. All Rights Reserved.

#include "GA_Blink.h"
#include "AbilitySystemComponent.h"
#include "NitroAttributeSet.h"
#include "GE_BlinkCooldown.h"
#include "TestVehicleGamePawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "CollisionQueryParams.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"

UGA_Blink::UGA_Blink()
{
	// Instant ability, one per actor
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// Set cooldown effect (15 seconds)
	CooldownGameplayEffectClass = UGE_BlinkCooldown::StaticClass();
}

bool UGA_Blink::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	// Check base conditions (including cooldown)
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// Check if we have enough energy
	if (const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		const UNitroAttributeSet* Attributes = ASC->GetSet<UNitroAttributeSet>();
		if (Attributes)
		{
			return Attributes->GetEnergy() >= EnergyCost;
		}
	}

	return false;
}

void UGA_Blink::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Commit ability (applies cooldown)
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Call Blueprint event for visual feedback
	K2_ActivateAbility();

	// Get vehicle pawn
	ATestVehicleGamePawn* VehiclePawn = Cast<ATestVehicleGamePawn>(ActorInfo->AvatarActor.Get());
	if (!VehiclePawn)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Get mesh and body instance for physics state
	USkeletalMeshComponent* Mesh = VehiclePawn->GetMesh();
	if (!Mesh)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FBodyInstance* BodyInstance = Mesh->GetBodyInstance();
	if (!BodyInstance)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Capture current physics state
	FVector LinearVelocity = BodyInstance->GetUnrealWorldVelocity();
	FVector AngularVelocity = BodyInstance->GetUnrealWorldAngularVelocityInRadians();

	// Calculate destination with collision avoidance
	FVector BlinkDirection;
	FVector Destination = CalculateBlinkDestination(VehiclePawn, BlinkDirection);

	// Check if destination is different from current location (collision avoidance might return start)
	float DistanceTraveled = FVector::Dist(VehiclePawn->GetActorLocation(), Destination);
	if (DistanceTraveled < 100.0f)
	{
		// Couldn't find valid destination, cancel ability
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Deduct energy
	DeductEnergy();

	// Spawn VFX at start position (before teleport)
	FVector StartLocation = VehiclePawn->GetActorLocation();
	SpawnBlinkVFX(VehiclePawn->GetWorld(), StartLocation);

	// Execute the blink (handles client prediction + server RPC)
	ExecuteBlink(VehiclePawn, Destination, LinearVelocity, AngularVelocity);

	// Spawn VFX at end position (after teleport)
	SpawnBlinkVFX(VehiclePawn->GetWorld(), Destination);

	// End ability (instant)
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

FVector UGA_Blink::CalculateBlinkDestination(AActor* Vehicle, FVector& OutDirection) const
{
	if (!Vehicle)
	{
		return FVector::ZeroVector;
	}

	FVector Start = Vehicle->GetActorLocation();
	FVector Velocity = Vehicle->GetVelocity();

	// Determine blink direction
	if (bUseVelocityDirection && Velocity.SizeSquared() > FMath::Square(MinVelocityForDirection))
	{
		OutDirection = Velocity.GetSafeNormal();
	}
	else
	{
		OutDirection = Vehicle->GetActorForwardVector();
	}

	FVector IdealEnd = Start + OutDirection * BlinkDistance;

	UWorld* World = Vehicle->GetWorld();
	if (!World)
	{
		return IdealEnd;
	}

	// Line trace to check for obstacles
	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Vehicle);
	Params.bTraceComplex = false;

	bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		Start,
		IdealEnd,
		ECC_Visibility,
		Params
	);

	if (bHit)
	{
		// Stop before the obstacle with some offset
		return HitResult.Location - OutDirection * CollisionCheckRadius;
	}

	return IdealEnd;
}

bool UGA_Blink::IsLocationValid(UWorld* World, const FVector& Location, const FVector& VehicleExtent, AActor* VehicleToIgnore) const
{
	if (!World)
	{
		return false;
	}

	FCollisionShape Shape = FCollisionShape::MakeBox(VehicleExtent + FVector(CollisionCheckRadius));
	FCollisionQueryParams Params;
	Params.bTraceComplex = false;
	Params.AddIgnoredActor(VehicleToIgnore);

	// Check for blocking overlaps
	return !World->OverlapBlockingTestByChannel(
		Location,
		FQuat::Identity,
		ECC_Visibility,  // Use Visibility channel - less restrictive than ECC_Vehicle
		Shape,
		Params
	);
}

FVector UGA_Blink::FindValidLocation(UWorld* World, const FVector& Start,
	const FVector& End, const FVector& VehicleExtent, AActor* VehicleToIgnore) const
{
	FVector LastValid = Start;
	FVector TestPoint = End;

	// Binary search for valid position
	for (int32 i = 0; i < MaxCollisionSearchSteps; i++)
	{
		FVector Mid = (LastValid + TestPoint) * 0.5f;

		if (IsLocationValid(World, Mid, VehicleExtent, VehicleToIgnore))
		{
			LastValid = Mid;
		}
		else
		{
			TestPoint = Mid;
		}
	}

	return LastValid;
}

void UGA_Blink::ExecuteBlink(ATestVehicleGamePawn* VehiclePawn, const FVector& Destination,
	const FVector& LinearVelocity, const FVector& AngularVelocity)
{
	if (!VehiclePawn)
	{
		return;
	}

	// Client prediction: execute locally for immediate feedback
	if (!VehiclePawn->HasAuthority())
	{
		VehiclePawn->PerformBlinkTeleport(Destination, LinearVelocity, AngularVelocity);
	}

	// Server execution: call Server RPC (will also execute locally if we are server/host)
	VehiclePawn->Server_ExecuteBlink(Destination, LinearVelocity, AngularVelocity);
}

void UGA_Blink::DeductEnergy()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo)
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC)
	{
		return;
	}

	UNitroAttributeSet* Attributes = const_cast<UNitroAttributeSet*>(ASC->GetSet<UNitroAttributeSet>());
	if (Attributes)
	{
		float NewEnergy = FMath::Max(0.0f, Attributes->GetEnergy() - EnergyCost);
		Attributes->SetEnergy(NewEnergy);
	}
}

void UGA_Blink::SpawnBlinkVFX(UWorld* World, const FVector& Location)
{
	if (!World || !BlinkVFX)
	{
		return;
	}

	UGameplayStatics::SpawnEmitterAtLocation(
		World,
		BlinkVFX,
		Location,
		FRotator::ZeroRotator,
		FVector(1.0f),
		true,  // bAutoDestroy
		EPSCPoolMethod::None,
		true   // bAutoActivateSystem
	);
}
