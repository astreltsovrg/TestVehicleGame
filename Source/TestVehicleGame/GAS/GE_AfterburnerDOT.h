// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_AfterburnerDOT.generated.h"

/**
 * Periodic Damage Over Time effect for Afterburner fire zones.
 * Applied when entering fire zone, removed when exiting.
 * Grants "State.Burning" tag while active.
 *
 * Note: For actual damage, target needs AttributeSet with Health attribute.
 * Currently just grants tag for visual/audio feedback.
 */
UCLASS()
class TESTVEHICLEGAME_API UGE_AfterburnerDOT : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_AfterburnerDOT();

	// UObject interface
	virtual void PostInitProperties() override;
};
