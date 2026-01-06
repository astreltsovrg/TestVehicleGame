// Copyright Epic Games, Inc. All Rights Reserved.

#include "GA_NitroBoost.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "NitroAttributeSet.h"
#include "TestVehicleGamePawn.h"
#include "TimerManager.h"

UGA_NitroBoost::UGA_NitroBoost()
{
	// This ability is activated by input and requires input to stay active
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// Allow reactivation while on cooldown (if we implement one later)
	bRetriggerInstancedAbility = false;
}

bool UGA_NitroBoost::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
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

void UGA_NitroBoost::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Apply torque boost to the vehicle
	ApplyTorqueBoost();

	// Apply energy drain effect if specified
	if (EnergyDrainEffect)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(this);
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EnergyDrainEffect, GetAbilityLevel(), Context);
		if (Spec.IsValid())
		{
			EnergyDrainHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	// Start timer to check energy level
	if (AActor* AvatarActor = ActorInfo->AvatarActor.Get())
	{
		if (UWorld* World = AvatarActor->GetWorld())
		{
			World->GetTimerManager().SetTimer(EnergyCheckTimer, this, &UGA_NitroBoost::CheckEnergyLevel, 0.1f, true);
		}
	}
}

void UGA_NitroBoost::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	// End the ability when input is released
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_NitroBoost::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Clear the energy check timer
	if (AActor* AvatarActor = ActorInfo->AvatarActor.Get())
	{
		if (UWorld* World = AvatarActor->GetWorld())
		{
			World->GetTimerManager().ClearTimer(EnergyCheckTimer);
		}
	}

	// Remove active effects
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (ASC)
	{
		if (TorqueBoostHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(TorqueBoostHandle);
			TorqueBoostHandle.Invalidate();
		}
		if (EnergyDrainHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(EnergyDrainHandle);
			EnergyDrainHandle.Invalidate();
		}
	}

	// Remove torque boost from vehicle
	RemoveTorqueBoost();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_NitroBoost::CheckEnergyLevel()
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

	// Only server checks energy and ends ability
	// This prevents desync where client/server end at different times
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	if (!AvatarActor || !AvatarActor->HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC)
	{
		return;
	}

	const UNitroAttributeSet* NitroAttributes = ASC->GetSet<UNitroAttributeSet>();
	if (NitroAttributes && NitroAttributes->GetEnergy() <= 0.0f)
	{
		// Out of energy, end the ability (replicates to client)
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, false);
	}
}

void UGA_NitroBoost::ApplyTorqueBoost()
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

	// Apply via GameplayEffect if specified (GAS handles replication)
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (ASC && TorqueBoostEffect)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(this);
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(TorqueBoostEffect, GetAbilityLevel(), Context);
		if (Spec.IsValid())
		{
			TorqueBoostHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	// Apply torque change only on server - physics replication syncs the result
	if (AvatarActor->HasAuthority())
	{
		if (ATestVehicleGamePawn* VehiclePawn = Cast<ATestVehicleGamePawn>(AvatarActor))
		{
			VehiclePawn->ApplyTorqueMultiplier(TorqueMultiplier);
		}
	}
}

void UGA_NitroBoost::RemoveTorqueBoost()
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

	// Restore base torque only on server - physics replication syncs the result
	if (AvatarActor->HasAuthority())
	{
		if (ATestVehicleGamePawn* VehiclePawn = Cast<ATestVehicleGamePawn>(AvatarActor))
		{
			VehiclePawn->RestoreBaseTorque();
		}
	}
}
