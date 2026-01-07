// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_AfterburnerTrail.generated.h"

class UGameplayEffect;
class AAfterburnerFireActor;

/**
 * Afterburner Trail Ability
 * Activated by holding the afterburner key (V by default).
 * - Spawns fire actors at the vehicle rear every 0.1 seconds
 * - Each fire actor costs energy
 * - Fire damages enemies who enter the zone
 * - Ends when key is released or energy is depleted
 * - Has cooldown after ending
 */
UCLASS()
class TESTVEHICLEGAME_API UGA_AfterburnerTrail : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_AfterburnerTrail();

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
	/** Fire actor class to spawn (set in Blueprint) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterburner")
	TSubclassOf<AAfterburnerFireActor> FireActorClass;

	/** DOT effect class applied by fire actors */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterburner")
	TSubclassOf<UGameplayEffect> DOTEffectClass;

	/** Energy consumed per fire spawn */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterburner")
	float EnergyPerSpawn = 2.0f;

	/** Minimum energy required to activate */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterburner")
	float MinEnergyToActivate = 5.0f;

	/** Spawn interval in seconds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterburner")
	float SpawnInterval = 0.1f;

	/** Offset from vehicle center to spawn fires (negative X = behind) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Afterburner")
	FVector SpawnOffset = FVector(-200.0f, 0.0f, 0.0f);

private:
	/** Timer for spawning fire actors */
	FTimerHandle SpawnTimer;

	/** Check energy and spawn fire actor */
	void CheckEnergyAndSpawn();

	/** Spawn a fire actor at vehicle rear */
	void SpawnFireActor();

	/** Deduct energy for one spawn, returns false if insufficient */
	bool DeductEnergy();
};
