// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "AfterburnerFireActor.generated.h"

class USphereComponent;
class UAbilitySystemComponent;
class UGameplayEffect;

/**
 * Fire Actor spawned by Afterburner Trail ability.
 * Creates a damage zone that applies DOT to enemies who enter.
 * Auto-destroys after a set lifespan.
 */
UCLASS()
class TESTVEHICLEGAME_API AAfterburnerFireActor : public AActor
{
	GENERATED_BODY()

public:
	AAfterburnerFireActor();

	/** Initialize the fire actor with spawner reference and DOT effect */
	void Initialize(AActor* InSpawnerVehicle, TSubclassOf<UGameplayEffect> InDOTEffectClass);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	/** Collision sphere for damage detection */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fire")
	TObjectPtr<USphereComponent> DamageZone;

	/** Radius of the fire damage zone */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	float DamageRadius = 200.0f;

	/** How long the fire persists before auto-destroy */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire")
	float FireLifespan = 10.0f;

private:
	/** DOT effect class to apply to targets */
	UPROPERTY()
	TSubclassOf<UGameplayEffect> DOTEffectClass;

	/** Reference to spawner vehicle (to ignore for collision) */
	UPROPERTY()
	TWeakObjectPtr<AActor> SpawnerVehicle;

	/** Map of active DOT effect handles per target for cleanup */
	TMap<TWeakObjectPtr<AActor>, FActiveGameplayEffectHandle> ActiveDOTEffects;

	UFUNCTION()
	void OnDamageZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnDamageZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** Apply DOT effect to target with AbilitySystemComponent */
	void ApplyDOTToTarget(AActor* Target);

	/** Remove DOT effect from target */
	void RemoveDOTFromTarget(AActor* Target);
};
