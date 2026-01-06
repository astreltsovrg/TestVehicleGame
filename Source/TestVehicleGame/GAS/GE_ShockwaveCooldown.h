// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_ShockwaveCooldown.generated.h"

/**
 * Cooldown GameplayEffect for the Shockwave ability.
 * Grants "Cooldown.Shockwave" tag to the target for 10 seconds.
 */
UCLASS()
class TESTVEHICLEGAME_API UGE_ShockwaveCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_ShockwaveCooldown();

	// UObject interface
	virtual void PostInitProperties() override;
};
