// Copyright Epic Games, Inc. All Rights Reserved.

#include "GA_Shockwave.h"
#include "AbilitySystemComponent.h"
#include "NitroAttributeSet.h"
#include "GE_ShockwaveCooldown.h"
#include "WheeledVehiclePawn.h"
#include "Components/PrimitiveComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"

UGA_Shockwave::UGA_Shockwave()
{
	// Instant ability, one per actor
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// Set cooldown effect
	CooldownGameplayEffectClass = UGE_ShockwaveCooldown::StaticClass();
}

bool UGA_Shockwave::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
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

void UGA_Shockwave::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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

	// Deduct energy
	DeductEnergy();

	// Perform the shockwave
	PerformShockwave();

	// End immediately (instant ability)
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_Shockwave::PerformShockwave()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo)
	{
		return;
	}

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	if (!AvatarActor)
	{
		return;
	}

	// Only apply physics impulses on the server!
	// Clients will see the results through physics replication.
	// This prevents double-impulse and ensures server authority over physics.
	if (!AvatarActor->HasAuthority())
	{
		return;
	}

	UWorld* World = AvatarActor->GetWorld();
	if (!World)
	{
		return;
	}

	FVector Origin = AvatarActor->GetActorLocation();

	// Set up collision query
	TArray<FOverlapResult> Overlaps;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(ImpulseRadius);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(AvatarActor);

	// Find all physics bodies in range
	World->OverlapMultiByChannel(
		Overlaps,
		Origin,
		FQuat::Identity,
		ECC_PhysicsBody,
		Sphere,
		QueryParams
	);

	// Apply impulse to each overlapping component
	for (const FOverlapResult& Overlap : Overlaps)
	{
		UPrimitiveComponent* Comp = Overlap.GetComponent();
		if (!Comp || !Comp->IsSimulatingPhysics())
		{
			continue;
		}

		AActor* HitActor = Overlap.GetActor();
		if (!HitActor)
		{
			continue;
		}

		// Calculate direction from origin to component
		FVector Direction = (Comp->GetComponentLocation() - Origin).GetSafeNormal();
		float Distance = FVector::Dist(Origin, Comp->GetComponentLocation());

		// Calculate falloff (closer = stronger)
		float Falloff = 1.0f - FMath::Clamp(Distance / ImpulseRadius, 0.0f, 1.0f);

		float FinalStrength = ImpulseStrength;

		// Check if this is a vehicle and apply multiplier
		if (bAffectVehicles && HitActor->IsA(AWheeledVehiclePawn::StaticClass()))
		{
			FinalStrength *= VehicleImpulseMultiplier;
		}
		else if (!bAffectVehicles && HitActor->IsA(AWheeledVehiclePawn::StaticClass()))
		{
			// Skip vehicles if bAffectVehicles is false
			continue;
		}

		// Calculate impulse with vertical lift component
		FVector Impulse = Direction * FinalStrength * Falloff;
		Impulse.Z += FinalStrength * 0.3f * Falloff; // Add vertical "lift"

		// Apply impulse (bVelChange = true means mass-independent)
		Comp->AddImpulse(Impulse, NAME_None, true);
	}
}

void UGA_Shockwave::DeductEnergy()
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
