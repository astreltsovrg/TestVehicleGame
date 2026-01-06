// Copyright Epic Games, Inc. All Rights Reserved.

#include "NitroAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "TestVehicleGamePawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"

UNitroAttributeSet::UNitroAttributeSet()
{
	// Initialize default values
	InitEnergy(100.0f);
	InitMaxEnergy(100.0f);
	InitTorqueMultiplier(1.0f);
}

void UNitroAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UNitroAttributeSet, Energy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNitroAttributeSet, MaxEnergy, COND_None, REPNOTIFY_Always);
	// TorqueMultiplier is not replicated - it's a local meta-attribute
}

void UNitroAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Clamp Energy between 0 and MaxEnergy
	if (Attribute == GetEnergyAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxEnergy());
	}
	// Ensure MaxEnergy is always positive
	else if (Attribute == GetMaxEnergyAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	// Clamp TorqueMultiplier to reasonable bounds
	else if (Attribute == GetTorqueMultiplierAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.1f, 5.0f);
	}
}

void UNitroAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// Handle TorqueMultiplier changes - apply to vehicle physics
	if (Data.EvaluatedData.Attribute == GetTorqueMultiplierAttribute())
	{
		const float NewMultiplier = GetTorqueMultiplier();
		ApplyTorqueToVehicle(NewMultiplier);
	}
}

void UNitroAttributeSet::OnRep_Energy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNitroAttributeSet, Energy, OldValue);
}

void UNitroAttributeSet::OnRep_MaxEnergy(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UNitroAttributeSet, MaxEnergy, OldValue);
}

void UNitroAttributeSet::ApplyTorqueToVehicle(float Multiplier)
{
	// Get the owning actor
	AActor* OwningActor = nullptr;
	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		OwningActor = ASC->GetAvatarActor();
	}

	if (!OwningActor)
	{
		return;
	}

	// Cast to our vehicle pawn
	ATestVehicleGamePawn* VehiclePawn = Cast<ATestVehicleGamePawn>(OwningActor);
	if (!VehiclePawn)
	{
		return;
	}

	// Apply or restore torque
	if (FMath::IsNearlyEqual(Multiplier, 1.0f, 0.01f))
	{
		VehiclePawn->RestoreBaseTorque();
	}
	else
	{
		VehiclePawn->ApplyTorqueMultiplier(Multiplier);
	}
}
