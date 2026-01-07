// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_Blink.generated.h"

class ATestVehicleGamePawn;
class UParticleSystem;

/**
 * Blink Ability - Instant Teleportation
 * Activated by pressing the blink key (C by default).
 * - Teleports the vehicle forward 100 meters
 * - Preserves velocity and physics state
 * - Collision avoidance finds valid destination
 * - Consumes 50 energy on activation
 * - Has a 15 second cooldown period
 */
UCLASS()
class TESTVEHICLEGAME_API UGA_Blink : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Blink();

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
	/** Distance to teleport in units (1000 = 10 meters) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Blink")
	float BlinkDistance = 1000.0f;

	/** Energy cost to activate the ability */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Blink")
	float EnergyCost = 50.0f;

	/** Use velocity direction instead of forward vector */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Blink")
	bool bUseVelocityDirection = true;

	/** Minimum velocity to use velocity direction (otherwise use forward) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Blink")
	float MinVelocityForDirection = 500.0f;

	/** Collision check radius at destination */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Blink")
	float CollisionCheckRadius = 200.0f;

	/** Maximum steps for binary search collision avoidance */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Blink")
	int32 MaxCollisionSearchSteps = 10;

	/** Particle effect to spawn at start and end positions */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Blink|VFX")
	TObjectPtr<UParticleSystem> BlinkVFX;

private:
	/** Spawn VFX at the given location */
	void SpawnBlinkVFX(UWorld* World, const FVector& Location);
	/** Calculate the blink destination with collision avoidance */
	FVector CalculateBlinkDestination(AActor* Vehicle, FVector& OutDirection) const;

	/** Check if location is valid for teleport (no overlaps) */
	bool IsLocationValid(UWorld* World, const FVector& Location, const FVector& VehicleExtent, AActor* VehicleToIgnore) const;

	/** Find nearest valid location along path using binary search */
	FVector FindValidLocation(UWorld* World, const FVector& Start, const FVector& End,
		const FVector& VehicleExtent, AActor* VehicleToIgnore) const;

	/** Execute the blink teleport (client prediction or server authoritative) */
	void ExecuteBlink(ATestVehicleGamePawn* VehiclePawn, const FVector& Destination,
		const FVector& LinearVelocity, const FVector& AngularVelocity);

	/** Deduct energy from the attribute set */
	void DeductEnergy();
};
