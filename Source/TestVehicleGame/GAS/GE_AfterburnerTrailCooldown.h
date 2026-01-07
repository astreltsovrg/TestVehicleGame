// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_AfterburnerTrailCooldown.generated.h"

/**
 * Cooldown GameplayEffect for the Afterburner Trail ability.
 * Grants "Cooldown.AfterburnerTrail" tag to the target for 5 seconds.
 */
UCLASS()
class TESTVEHICLEGAME_API UGE_AfterburnerTrailCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_AfterburnerTrailCooldown();

	// UObject interface
	virtual void PostInitProperties() override;
};
