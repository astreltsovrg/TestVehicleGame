// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "NitroAttributeSet.generated.h"

// Macro for attribute accessors (standard GAS pattern)
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Attribute set for the vehicle ability system.
 * Manages Energy (shared resource for Nitro, Shockwave, etc.) and bridges GAS effects to physics.
 */
UCLASS()
class TESTVEHICLEGAME_API UNitroAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UNitroAttributeSet();

	// Current energy (0 to MaxEnergy) - shared resource for all abilities
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Energy, Category = "Energy")
	FGameplayAttributeData Energy;
	ATTRIBUTE_ACCESSORS(UNitroAttributeSet, Energy)

	// Maximum energy capacity
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxEnergy, Category = "Energy")
	FGameplayAttributeData MaxEnergy;
	ATTRIBUTE_ACCESSORS(UNitroAttributeSet, MaxEnergy)

	// Meta-attribute: Torque multiplier to apply to the vehicle
	// When this changes via GameplayEffect, we apply it to ChaosVehicleMovement
	UPROPERTY(BlueprintReadOnly, Category = "Nitro")
	FGameplayAttributeData TorqueMultiplier;
	ATTRIBUTE_ACCESSORS(UNitroAttributeSet, TorqueMultiplier)

	// Replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Clamp attribute values before they change
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	// React to attribute changes - this is where we apply torque to the vehicle
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

protected:
	UFUNCTION()
	void OnRep_Energy(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxEnergy(const FGameplayAttributeData& OldValue);

private:
	// Helper to apply torque multiplier to the owning vehicle
	void ApplyTorqueToVehicle(float Multiplier);
};
