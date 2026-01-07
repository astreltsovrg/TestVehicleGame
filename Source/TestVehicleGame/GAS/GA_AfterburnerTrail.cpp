// Copyright Epic Games, Inc. All Rights Reserved.

#include "GA_AfterburnerTrail.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "NitroAttributeSet.h"
#include "AfterburnerFireActor.h"
#include "GE_AfterburnerTrailCooldown.h"
#include "GE_AfterburnerDOT.h"
#include "TimerManager.h"
#include "Engine/World.h"

UGA_AfterburnerTrail::UGA_AfterburnerTrail()
{
	// Hold ability - requires input to stay active
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bRetriggerInstancedAbility = false;

	// Set cooldown effect
	CooldownGameplayEffectClass = UGE_AfterburnerTrailCooldown::StaticClass();

	// Set default DOT effect
	DOTEffectClass = UGE_AfterburnerDOT::StaticClass();
}

bool UGA_AfterburnerTrail::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	// Super checks cooldown tag
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// Check if we have enough energy
	if (const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		const UNitroAttributeSet* NitroAttributes = ASC->GetSet<UNitroAttributeSet>();
		if (NitroAttributes)
		{
			return NitroAttributes->GetEnergy() >= MinEnergyToActivate;
		}
	}

	return false;
}

void UGA_AfterburnerTrail::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Start spawning fire actors
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	if (!AvatarActor)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UWorld* World = AvatarActor->GetWorld();
	if (!World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Spawn first fire immediately
	CheckEnergyAndSpawn();

	// Set up periodic spawning timer
	World->GetTimerManager().SetTimer(SpawnTimer, this, &UGA_AfterburnerTrail::CheckEnergyAndSpawn, SpawnInterval, true);

	// Call Super to trigger Blueprint K2_ActivateAbility event
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UGA_AfterburnerTrail::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	// End the ability when input is released
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_AfterburnerTrail::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Clear the spawn timer
	if (AActor* AvatarActor = ActorInfo->AvatarActor.Get())
	{
		if (UWorld* World = AvatarActor->GetWorld())
		{
			World->GetTimerManager().ClearTimer(SpawnTimer);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_AfterburnerTrail::CheckEnergyAndSpawn()
{
	if (!IsActive())
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo)
	{
		return;
	}

	// Check and deduct energy
	if (!DeductEnergy())
	{
		// Out of energy, end ability
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, false);
		return;
	}

	// Spawn fire actor (server only)
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	if (AvatarActor && AvatarActor->HasAuthority())
	{
		SpawnFireActor();
	}
}

void UGA_AfterburnerTrail::SpawnFireActor()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo || !FireActorClass)
	{
		return;
	}

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	if (!AvatarActor)
	{
		return;
	}

	UWorld* World = AvatarActor->GetWorld();
	if (!World)
	{
		return;
	}

	// Calculate spawn location at vehicle rear
	FVector SpawnLocation = AvatarActor->GetActorLocation() +
		AvatarActor->GetActorRotation().RotateVector(SpawnOffset);

	// Spawn parameters
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = AvatarActor;
	SpawnParams.Instigator = Cast<APawn>(AvatarActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Spawn the fire actor
	AAfterburnerFireActor* FireActor = World->SpawnActor<AAfterburnerFireActor>(
		FireActorClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);

	if (FireActor)
	{
		// Initialize with spawner reference and DOT effect
		FireActor->Initialize(AvatarActor, DOTEffectClass);
	}
}

bool UGA_AfterburnerTrail::DeductEnergy()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC)
	{
		return false;
	}

	UNitroAttributeSet* Attributes = const_cast<UNitroAttributeSet*>(ASC->GetSet<UNitroAttributeSet>());
	if (!Attributes)
	{
		return false;
	}

	float CurrentEnergy = Attributes->GetEnergy();
	if (CurrentEnergy < EnergyPerSpawn)
	{
		return false;
	}

	Attributes->SetEnergy(CurrentEnergy - EnergyPerSpawn);
	return true;
}
