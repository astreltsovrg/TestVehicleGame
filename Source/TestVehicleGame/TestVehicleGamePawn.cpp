// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestVehicleGamePawn.h"
#include "TestVehicleGameWheelFront.h"
#include "TestVehicleGameWheelRear.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "SnapshotData.h"
#include "TestVehicleGame.h"
#include "TimerManager.h"
#include "AbilitySystemComponent.h"
#include "GAS/NitroAttributeSet.h"
#include "GAS/GA_NitroBoost.h"
#include "GAS/GA_Shockwave.h"
#include "GAS/GA_Blink.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"

#define LOCTEXT_NAMESPACE "VehiclePawn"

ATestVehicleGamePawn::ATestVehicleGamePawn()
{
	// construct the front camera boom
	FrontSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Front Spring Arm"));
	FrontSpringArm->SetupAttachment(GetMesh());
	FrontSpringArm->TargetArmLength = 0.0f;
	FrontSpringArm->bDoCollisionTest = false;
	FrontSpringArm->bEnableCameraRotationLag = true;
	FrontSpringArm->CameraRotationLagSpeed = 15.0f;
	FrontSpringArm->SetRelativeLocation(FVector(30.0f, 0.0f, 120.0f));

	FrontCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Front Camera"));
	FrontCamera->SetupAttachment(FrontSpringArm);
	FrontCamera->bAutoActivate = false;

	// construct the back camera boom
	BackSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Back Spring Arm"));
	BackSpringArm->SetupAttachment(GetMesh());
	BackSpringArm->TargetArmLength = 650.0f;
	BackSpringArm->SocketOffset.Z = 150.0f;
	BackSpringArm->bDoCollisionTest = false;
	BackSpringArm->bInheritPitch = false;
	BackSpringArm->bInheritRoll = false;
	BackSpringArm->bEnableCameraRotationLag = true;
	BackSpringArm->CameraRotationLagSpeed = 2.0f;
	BackSpringArm->CameraLagMaxDistance = 50.0f;

	BackCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Back Camera"));
	BackCamera->SetupAttachment(BackSpringArm);

	// Configure the car mesh
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetCollisionProfileName(FName("Vehicle"));

	// get the Chaos Wheeled movement component
	ChaosVehicleMovement = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());

	// Create Ability System Component
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

void ATestVehicleGamePawn::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// steering 
		EnhancedInputComponent->BindAction(SteeringAction, ETriggerEvent::Triggered, this, &ATestVehicleGamePawn::Steering);
		EnhancedInputComponent->BindAction(SteeringAction, ETriggerEvent::Completed, this, &ATestVehicleGamePawn::Steering);

		// throttle 
		EnhancedInputComponent->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &ATestVehicleGamePawn::Throttle);
		EnhancedInputComponent->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &ATestVehicleGamePawn::Throttle);

		// break 
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &ATestVehicleGamePawn::Brake);
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Started, this, &ATestVehicleGamePawn::StartBrake);
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Completed, this, &ATestVehicleGamePawn::StopBrake);

		// handbrake 
		EnhancedInputComponent->BindAction(HandbrakeAction, ETriggerEvent::Started, this, &ATestVehicleGamePawn::StartHandbrake);
		EnhancedInputComponent->BindAction(HandbrakeAction, ETriggerEvent::Completed, this, &ATestVehicleGamePawn::StopHandbrake);

		// look around 
		EnhancedInputComponent->BindAction(LookAroundAction, ETriggerEvent::Triggered, this, &ATestVehicleGamePawn::LookAround);

		// toggle camera 
		EnhancedInputComponent->BindAction(ToggleCameraAction, ETriggerEvent::Triggered, this, &ATestVehicleGamePawn::ToggleCamera);

		// reset the vehicle
		EnhancedInputComponent->BindAction(ResetVehicleAction, ETriggerEvent::Triggered, this, &ATestVehicleGamePawn::ResetVehicle);

		// nitro boost
		if (NitroAction)
		{
			EnhancedInputComponent->BindAction(NitroAction, ETriggerEvent::Started, this, &ATestVehicleGamePawn::NitroStarted);
			EnhancedInputComponent->BindAction(NitroAction, ETriggerEvent::Completed, this, &ATestVehicleGamePawn::NitroCompleted);
		}

		// shockwave ability
		if (ShockwaveAction)
		{
			EnhancedInputComponent->BindAction(ShockwaveAction, ETriggerEvent::Started, this, &ATestVehicleGamePawn::ShockwaveStarted);
			EnhancedInputComponent->BindAction(ShockwaveAction, ETriggerEvent::Completed, this, &ATestVehicleGamePawn::ShockwaveCompleted);
		}

		// blink ability
		if (BlinkAction)
		{
			EnhancedInputComponent->BindAction(BlinkAction, ETriggerEvent::Started, this, &ATestVehicleGamePawn::BlinkStarted);
			EnhancedInputComponent->BindAction(BlinkAction, ETriggerEvent::Completed, this, &ATestVehicleGamePawn::BlinkCompleted);
		}
	}
	else
	{
		UE_LOG(LogTestVehicleGame, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ATestVehicleGamePawn::BeginPlay()
{
	Super::BeginPlay();

	// set up the flipped check timer
	GetWorld()->GetTimerManager().SetTimer(FlipCheckTimer, this, &ATestVehicleGamePawn::FlippedCheck, FlipCheckTime, true);

	// Initialize GAS
	InitializeAbilitySystem();
	GrantDefaultAbilitiesAndEffects();

	// Store base torque for nitro system
	if (ChaosVehicleMovement)
	{
		StoredBaseTorque = ChaosVehicleMovement->EngineSetup.MaxTorque;
		bBaseTorqueStored = true;
	}
}

void ATestVehicleGamePawn::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	// clear the flipped check timer
	GetWorld()->GetTimerManager().ClearTimer(FlipCheckTimer);

	Super::EndPlay(EndPlayReason);
}

void ATestVehicleGamePawn::Tick(float Delta)
{
	Super::Tick(Delta);

	// add some angular damping if the vehicle is in midair
	bool bMovingOnGround = ChaosVehicleMovement->IsMovingOnGround();
	GetMesh()->SetAngularDamping(bMovingOnGround ? 0.0f : 3.0f);

	// realign the camera yaw to face front
	float CameraYaw = BackSpringArm->GetRelativeRotation().Yaw;
	CameraYaw = FMath::FInterpTo(CameraYaw, 0.0f, Delta, 1.0f);

	BackSpringArm->SetRelativeRotation(FRotator(0.0f, CameraYaw, 0.0f));
}

void ATestVehicleGamePawn::Steering(const FInputActionValue& Value)
{
	// route the input
	DoSteering(Value.Get<float>());
}

void ATestVehicleGamePawn::Throttle(const FInputActionValue& Value)
{
	// route the input
	DoThrottle(Value.Get<float>());
}

void ATestVehicleGamePawn::Brake(const FInputActionValue& Value)
{
	// route the input
	DoBrake(Value.Get<float>());
}

void ATestVehicleGamePawn::StartBrake(const FInputActionValue& Value)
{
	// route the input
	DoBrakeStart();
}

void ATestVehicleGamePawn::StopBrake(const FInputActionValue& Value)
{
	// route the input
	DoBrakeStop();
}

void ATestVehicleGamePawn::StartHandbrake(const FInputActionValue& Value)
{
	// route the input
	DoHandbrakeStart();
}

void ATestVehicleGamePawn::StopHandbrake(const FInputActionValue& Value)
{
	// route the input
	DoHandbrakeStop();
}

void ATestVehicleGamePawn::LookAround(const FInputActionValue& Value)
{
	// route the input
	DoLookAround(Value.Get<float>());
}

void ATestVehicleGamePawn::ToggleCamera(const FInputActionValue& Value)
{
	// route the input
	DoToggleCamera();
}

void ATestVehicleGamePawn::ResetVehicle(const FInputActionValue& Value)
{
	// route the input
	DoResetVehicle();
}

void ATestVehicleGamePawn::DoSteering(float SteeringValue)
{
	// add the input
	ChaosVehicleMovement->SetSteeringInput(SteeringValue);
}

void ATestVehicleGamePawn::DoThrottle(float ThrottleValue)
{
	// add the input
	ChaosVehicleMovement->SetThrottleInput(ThrottleValue);

	// reset the brake input
	ChaosVehicleMovement->SetBrakeInput(0.0f);
}

void ATestVehicleGamePawn::DoBrake(float BrakeValue)
{
	// add the input
	ChaosVehicleMovement->SetBrakeInput(BrakeValue);

	// reset the throttle input
	ChaosVehicleMovement->SetThrottleInput(0.0f);
}

void ATestVehicleGamePawn::DoBrakeStart()
{
	// call the Blueprint hook for the brake lights
	BrakeLights(true);
}

void ATestVehicleGamePawn::DoBrakeStop()
{
	// call the Blueprint hook for the brake lights
	BrakeLights(false);

	// reset brake input to zero
	ChaosVehicleMovement->SetBrakeInput(0.0f);
}

void ATestVehicleGamePawn::DoHandbrakeStart()
{
	// add the input
	ChaosVehicleMovement->SetHandbrakeInput(true);

	// call the Blueprint hook for the break lights
	BrakeLights(true);
}

void ATestVehicleGamePawn::DoHandbrakeStop()
{
	// add the input
	ChaosVehicleMovement->SetHandbrakeInput(false);

	// call the Blueprint hook for the break lights
	BrakeLights(false);
}

void ATestVehicleGamePawn::DoLookAround(float YawDelta)
{
	// rotate the spring arm
	BackSpringArm->AddLocalRotation(FRotator(0.0f, YawDelta, 0.0f));
}

void ATestVehicleGamePawn::DoToggleCamera()
{
	// toggle the active camera flag
	bFrontCameraActive = !bFrontCameraActive;

	FrontCamera->SetActive(bFrontCameraActive);
	BackCamera->SetActive(!bFrontCameraActive);
}

void ATestVehicleGamePawn::DoResetVehicle()
{
	// reset to a location slightly above our current one
	FVector ResetLocation = GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);

	// reset to our yaw. Ignore pitch and roll
	FRotator ResetRotation = GetActorRotation();
	ResetRotation.Pitch = 0.0f;
	ResetRotation.Roll = 0.0f;

	// teleport the actor to the reset spot and reset physics
	SetActorTransform(FTransform(ResetRotation, ResetLocation, FVector::OneVector), false, nullptr, ETeleportType::TeleportPhysics);

	GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
}

void ATestVehicleGamePawn::FlippedCheck()
{
	// check the difference in angle between the mesh's up vector and world up
	const float UpDot = FVector::DotProduct(FVector::UpVector, GetMesh()->GetUpVector());

	if (UpDot < FlipCheckMinDot)
	{
		// is this the second time we've checked that the vehicle is still flipped?
		if (bPreviousFlipCheck)
		{
			// reset the vehicle to upright
			DoResetVehicle();
		}

		// set the flipped check flag so the next check resets the car
		bPreviousFlipCheck = true;

	} else {

		// we're upright. reset the flipped check flag
		bPreviousFlipCheck = false;
	}
}

// ============================================================================
// GAS Integration
// ============================================================================

UAbilitySystemComponent* ATestVehicleGamePawn::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ATestVehicleGamePawn::InitializeAbilitySystem()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		// Create and register the Nitro attribute set
		NitroAttributes = NewObject<UNitroAttributeSet>(this);
		AbilitySystemComponent->AddSpawnedAttribute(NitroAttributes);
	}
}

void ATestVehicleGamePawn::GrantDefaultAbilitiesAndEffects()
{
	if (!AbilitySystemComponent || !HasAuthority())
	{
		return;
	}

	// Grant default abilities with appropriate InputID
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (AbilityClass)
		{
			// Determine InputID based on ability type
			int32 InputID = INDEX_NONE;
			if (AbilityClass->IsChildOf(UGA_NitroBoost::StaticClass()))
			{
				InputID = NitroInputID;
			}
			else if (AbilityClass->IsChildOf(UGA_Shockwave::StaticClass()))
			{
				InputID = ShockwaveInputID;
			}
			else if (AbilityClass->IsChildOf(UGA_Blink::StaticClass()))
			{
				InputID = BlinkInputID;
			}

			FGameplayAbilitySpec Spec(AbilityClass, 1, InputID, this);
			AbilitySystemComponent->GiveAbility(Spec);
		}
	}

	// Apply default effects
	for (const TSubclassOf<UGameplayEffect>& EffectClass : DefaultEffects)
	{
		if (EffectClass)
		{
			FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
			Context.AddSourceObject(this);
			FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(EffectClass, 1, Context);
			if (Spec.IsValid())
			{
				AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}
		}
	}
}

void ATestVehicleGamePawn::NitroStarted(const FInputActionValue& Value)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityLocalInputPressed(NitroInputID);
	}
}

void ATestVehicleGamePawn::NitroCompleted(const FInputActionValue& Value)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityLocalInputReleased(NitroInputID);
	}
}

void ATestVehicleGamePawn::ShockwaveStarted(const FInputActionValue& Value)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityLocalInputPressed(ShockwaveInputID);
	}
}

void ATestVehicleGamePawn::ShockwaveCompleted(const FInputActionValue& Value)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityLocalInputReleased(ShockwaveInputID);
	}
}

float ATestVehicleGamePawn::GetNitroFuel() const
{
	if (NitroAttributes)
	{
		return NitroAttributes->GetEnergy();
	}
	return 0.0f;
}

float ATestVehicleGamePawn::GetMaxNitroFuel() const
{
	if (NitroAttributes)
	{
		return NitroAttributes->GetMaxEnergy();
	}
	return 100.0f;
}

float ATestVehicleGamePawn::GetBaseTorque() const
{
	return StoredBaseTorque;
}

void ATestVehicleGamePawn::ApplyTorqueMultiplier(float Multiplier)
{
	if (ChaosVehicleMovement && bBaseTorqueStored)
	{
		const float NewTorque = StoredBaseTorque * Multiplier;
		ChaosVehicleMovement->SetMaxEngineTorque(NewTorque);
	}
}

void ATestVehicleGamePawn::RestoreBaseTorque()
{
	if (ChaosVehicleMovement && bBaseTorqueStored)
	{
		ChaosVehicleMovement->SetMaxEngineTorque(StoredBaseTorque);
	}
}

// ============================================================================
// Blink Ability
// ============================================================================

void ATestVehicleGamePawn::BlinkStarted(const FInputActionValue& Value)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityLocalInputPressed(BlinkInputID);
	}
}

void ATestVehicleGamePawn::BlinkCompleted(const FInputActionValue& Value)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityLocalInputReleased(BlinkInputID);
	}
}

void ATestVehicleGamePawn::Server_ExecuteBlink_Implementation(FVector Destination,
	FVector PreservedLinearVelocity, FVector PreservedAngularVelocity)
{
	// Server-authoritative execution
	PerformBlinkTeleport(Destination, PreservedLinearVelocity, PreservedAngularVelocity);

	// Notify all clients for VFX/sound
	Multicast_OnBlinkExecuted(Destination);
}

void ATestVehicleGamePawn::Multicast_OnBlinkExecuted_Implementation(FVector NewLocation)
{
	// This is called on all clients after the server executes the blink
	// Can be used for VFX/sound effects in the future
	// For now, this serves as a notification point
}

void ATestVehicleGamePawn::PerformBlinkTeleport(const FVector& Destination,
	const FVector& LinearVel, const FVector& AngularVel)
{
	if (!ChaosVehicleMovement)
	{
		return;
	}

	// Use the vehicle's snapshot system to preserve full physics state including wheels
	FWheeledSnaphotData Snapshot = ChaosVehicleMovement->GetSnapshot();

	// Update transform to new destination, keep same rotation
	Snapshot.Transform.SetLocation(Destination);

	// Preserve velocities (passed from ability, captured before teleport)
	Snapshot.LinearVelocity = LinearVel;
	Snapshot.AngularVelocity = AngularVel;

	// Apply snapshot - this properly handles:
	// - Body transform with TeleportPhysics
	// - Linear and angular velocity
	// - Wheel states (steering, suspension, rotation, angular velocity)
	// - Engine RPM and gear
	ChaosVehicleMovement->SetSnapshot(Snapshot);

	// Wake all physics bodies
	if (USkeletalMeshComponent* VehicleMesh = GetMesh())
	{
		VehicleMesh->WakeAllRigidBodies();
	}
}

#undef LOCTEXT_NAMESPACE