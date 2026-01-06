// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "AbilitySystemInterface.h"
#include "TestVehicleGamePawn.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputAction;
class UChaosWheeledVehicleMovementComponent;
class UAbilitySystemComponent;
class UNitroAttributeSet;
class UGameplayAbility;
class UGameplayEffect;
struct FInputActionValue;

/**
 *  Vehicle Pawn class
 *  Handles common functionality for all vehicle types,
 *  including input handling and camera management.
 *  
 *  Specific vehicle configurations are handled in subclasses.
 */
UCLASS(abstract)
class ATestVehicleGamePawn : public AWheeledVehiclePawn, public IAbilitySystemInterface
{
	GENERATED_BODY()

	/** Spring Arm for the front camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* FrontSpringArm;

	/** Front Camera component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FrontCamera;

	/** Spring Arm for the back camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* BackSpringArm;

	/** Back Camera component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* BackCamera;

	/** Cast pointer to the Chaos Vehicle movement component */
	TObjectPtr<UChaosWheeledVehicleMovementComponent> ChaosVehicleMovement;

	/** Ability System Component for GAS integration */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** Nitro Attribute Set (owned by ASC, cached here for convenience) */
	UPROPERTY()
	TObjectPtr<UNitroAttributeSet> NitroAttributes;

	/** Cached base torque value for restoration after nitro boost */
	float StoredBaseTorque = 0.0f;

	/** Whether we have cached the base torque */
	bool bBaseTorqueStored = false;

protected:

	/** Steering Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* SteeringAction;

	/** Throttle Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ThrottleAction;

	/** Brake Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* BrakeAction;

	/** Handbrake Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* HandbrakeAction;

	/** Look Around Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAroundAction;

	/** Toggle Camera Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ToggleCameraAction;

	/** Reset Vehicle Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ResetVehicleAction;

	/** Nitro Boost Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* NitroAction;

	/** Shockwave Ability Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ShockwaveAction;

	/** Blink Ability Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* BlinkAction;

	/** Default abilities to grant on spawn */
	UPROPERTY(EditDefaultsOnly, Category="GAS")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	/** Default effects to apply on spawn (e.g., nitro regen) */
	UPROPERTY(EditDefaultsOnly, Category="GAS")
	TArray<TSubclassOf<UGameplayEffect>> DefaultEffects;

	/** Keeps track of which camera is active */
	bool bFrontCameraActive = false;

	/** Keeps track of whether the car is flipped. If this is true for two flip checks, resets the vehicle automatically */
	bool bPreviousFlipCheck = false;

	/** Time between automatic flip checks */
	UPROPERTY(EditAnywhere, Category="Flip Check", meta = (Units = "s"))
	float FlipCheckTime = 3.0f;

	/** Minimum dot product value for the vehicle's up direction that we still consider upright */
	UPROPERTY(EditAnywhere, Category="Flip Check")
	float FlipCheckMinDot = -0.2f;

	/** Flip check timer */
	FTimerHandle FlipCheckTimer;

public:
	ATestVehicleGamePawn();

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// Begin Pawn interface

	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

	// End Pawn interface

	// Begin Actor interface

	/** Initialization */
	virtual void BeginPlay() override;

	/** Cleanup */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	/** Update */
	virtual void Tick(float Delta) override;

	// End Actor interface

protected:

	/** Handles steering input */
	void Steering(const FInputActionValue& Value);

	/** Handles throttle input */
	void Throttle(const FInputActionValue& Value);

	/** Handles brake input */
	void Brake(const FInputActionValue& Value);

	/** Handles brake start/stop inputs */
	void StartBrake(const FInputActionValue& Value);
	void StopBrake(const FInputActionValue& Value);

	/** Handles handbrake start/stop inputs */
	void StartHandbrake(const FInputActionValue& Value);
	void StopHandbrake(const FInputActionValue& Value);

	/** Handles look around input */
	void LookAround(const FInputActionValue& Value);

	/** Handles toggle camera input */
	void ToggleCamera(const FInputActionValue& Value);

	/** Handles reset vehicle input */
	void ResetVehicle(const FInputActionValue& Value);

	/** Handles nitro input start */
	void NitroStarted(const FInputActionValue& Value);

	/** Handles nitro input end */
	void NitroCompleted(const FInputActionValue& Value);

	/** Handles shockwave input start */
	void ShockwaveStarted(const FInputActionValue& Value);

	/** Handles shockwave input end */
	void ShockwaveCompleted(const FInputActionValue& Value);

	/** Handles blink input start */
	void BlinkStarted(const FInputActionValue& Value);

	/** Handles blink input end */
	void BlinkCompleted(const FInputActionValue& Value);

	/** Initialize the Ability System Component */
	void InitializeAbilitySystem();

	/** Grant default abilities and apply default effects */
	void GrantDefaultAbilitiesAndEffects();

	/** Input ID for nitro ability binding */
	static constexpr int32 NitroInputID = 1;

	/** Input ID for shockwave ability binding */
	static constexpr int32 ShockwaveInputID = 2;

	/** Input ID for blink ability binding */
	static constexpr int32 BlinkInputID = 3;

public:

	/** Handle steering input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoSteering(float SteeringValue);

	/** Handle throttle input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoThrottle(float ThrottleValue);

	/** Handle brake input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoBrake(float BrakeValue);

	/** Handle brake start input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoBrakeStart();

	/** Handle brake stop input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoBrakeStop();

	/** Handle handbrake start input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoHandbrakeStart();

	/** Handle handbrake stop input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoHandbrakeStop();

	/** Handle look input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoLookAround(float YawDelta);

	/** Handle toggle camera input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoToggleCamera();

	/** Handle reset vehicle input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoResetVehicle();

	/** Get current nitro fuel level */
	UFUNCTION(BlueprintCallable, Category="Nitro")
	float GetNitroFuel() const;

	/** Get maximum nitro fuel capacity */
	UFUNCTION(BlueprintCallable, Category="Nitro")
	float GetMaxNitroFuel() const;

	/** Get the base (non-boosted) torque value */
	float GetBaseTorque() const;

	/** Apply a torque multiplier to the vehicle (called by GAS) */
	void ApplyTorqueMultiplier(float Multiplier);

	/** Restore the base torque value (called when nitro ends) */
	void RestoreBaseTorque();

	// -------- Blink Teleport (Multiplayer) --------

	/** Server RPC - executes blink on server (authority) */
	UFUNCTION(Server, Reliable)
	void Server_ExecuteBlink(FVector Destination, FVector PreservedLinearVelocity,
		FVector PreservedAngularVelocity);

	/** Multicast RPC - notifies all clients of blink (for VFX/sound) */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_OnBlinkExecuted(FVector NewLocation);

	/** Performs the actual blink teleport with physics preservation */
	void PerformBlinkTeleport(const FVector& Destination, const FVector& LinearVel,
		const FVector& AngularVel);

protected:

	/** Called when the brake lights are turned on or off */
	UFUNCTION(BlueprintImplementableEvent, Category="Vehicle")
	void BrakeLights(bool bBraking);

	/** Checks if the car is flipped upside down and automatically resets it */
	UFUNCTION()
	void FlippedCheck();

public:
	/** Returns the front spring arm subobject */
	FORCEINLINE USpringArmComponent* GetFrontSpringArm() const { return FrontSpringArm; }
	/** Returns the front camera subobject */
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FrontCamera; }
	/** Returns the back spring arm subobject */
	FORCEINLINE USpringArmComponent* GetBackSpringArm() const { return BackSpringArm; }
	/** Returns the back camera subobject */
	FORCEINLINE UCameraComponent* GetBackCamera() const { return BackCamera; }
	/** Returns the cast Chaos Vehicle Movement subobject */
	FORCEINLINE const TObjectPtr<UChaosWheeledVehicleMovementComponent>& GetChaosVehicleMovement() const { return ChaosVehicleMovement; }
};
