// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_NitroBoost.generated.h"

class UGameplayEffect;

/**
 * Nitro Boost Ability
 * Activated by holding the nitro key (Z by default).
 * - Increases vehicle torque while active
 * - Consumes energy over time
 * - Ends when key is released or energy is depleted
 */
UCLASS()
class TESTVEHICLEGAME_API UGA_NitroBoost : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_NitroBoost();

	// Check if we have enough energy to activate
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	// Called when ability is activated
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// Called when input is released
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	// Called when ability ends
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** GameplayEffect class for torque boost (infinite duration, applies TorqueMultiplier) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nitro")
	TSubclassOf<UGameplayEffect> TorqueBoostEffect;

	/** GameplayEffect class for energy drain (periodic, reduces Energy) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nitro")
	TSubclassOf<UGameplayEffect> EnergyDrainEffect;

	/** Torque multiplier when nitro is active */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nitro")
	float TorqueMultiplier = 1.5f;

	/** Energy consumed per second */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nitro")
	float EnergyCostPerSecond = 20.0f;

	/** Minimum energy required to activate */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nitro")
	float MinEnergyToActivate = 5.0f;

private:
	/** Active effect handles for cleanup */
	FActiveGameplayEffectHandle TorqueBoostHandle;
	FActiveGameplayEffectHandle EnergyDrainHandle;

	/** Timer for checking energy level */
	FTimerHandle EnergyCheckTimer;

	/** Check if we still have energy, end ability if depleted */
	void CheckEnergyLevel();

	/** Apply the torque boost effect dynamically */
	void ApplyTorqueBoost();

	/** Remove the torque boost effect */
	void RemoveTorqueBoost();
};
