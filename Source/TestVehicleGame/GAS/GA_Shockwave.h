// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Shockwave.generated.h"

/**
 * Shockwave Ability
 * Activated by pressing the shockwave key (X by default).
 * - Creates a radial impulse that pushes physics objects away
 * - Affects props and vehicles within radius
 * - Consumes energy on activation
 * - Has a cooldown period
 */
UCLASS()
class TESTVEHICLEGAME_API UGA_Shockwave : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Shockwave();

	// Check if we have enough energy and cooldown is ready
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

protected:
	/** Radius of the shockwave effect in units */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shockwave")
	float ImpulseRadius = 800.0f;

	/** Strength of the impulse force */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shockwave")
	float ImpulseStrength = 150000.0f;

	/** Energy cost to activate the ability */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shockwave")
	float EnergyCost = 30.0f;

	/** Whether to affect other vehicles */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shockwave")
	bool bAffectVehicles = true;

	/** Impulse multiplier for vehicles (0.5 = 50% of normal strength) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shockwave")
	float VehicleImpulseMultiplier = 0.5f;

private:
	/** Apply radial impulse to nearby physics objects */
	void PerformShockwave();

	/** Deduct energy from the attribute set */
	void DeductEnergy();
};
