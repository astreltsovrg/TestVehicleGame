// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_BlinkCooldown.generated.h"

/**
 * Cooldown GameplayEffect for the Blink ability.
 * Grants "Cooldown.Blink" tag to the target for 15 seconds.
 */
UCLASS()
class TESTVEHICLEGAME_API UGE_BlinkCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_BlinkCooldown();

	// UObject interface
	virtual void PostInitProperties() override;
};
