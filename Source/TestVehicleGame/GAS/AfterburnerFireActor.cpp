// Copyright Epic Games, Inc. All Rights Reserved.

#include "AfterburnerFireActor.h"
#include "Components/SphereComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"

AAfterburnerFireActor::AAfterburnerFireActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// Create damage zone collision sphere
	DamageZone = CreateDefaultSubobject<USphereComponent>(TEXT("DamageZone"));
	DamageZone->SetSphereRadius(DamageRadius);
	DamageZone->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	DamageZone->SetGenerateOverlapEvents(true);
	RootComponent = DamageZone;

	// Note: Visual effects (Niagara/Particle) should be added in Blueprint subclass
}

void AAfterburnerFireActor::Initialize(AActor* InSpawnerVehicle, TSubclassOf<UGameplayEffect> InDOTEffectClass)
{
	SpawnerVehicle = InSpawnerVehicle;
	DOTEffectClass = InDOTEffectClass;
	// Note: Spawner vehicle is filtered out in overlap callbacks
}

void AAfterburnerFireActor::BeginPlay()
{
	Super::BeginPlay();

	// Set auto-destroy timer
	SetLifeSpan(FireLifespan);

	// Bind overlap events
	DamageZone->OnComponentBeginOverlap.AddDynamic(this, &AAfterburnerFireActor::OnDamageZoneBeginOverlap);
	DamageZone->OnComponentEndOverlap.AddDynamic(this, &AAfterburnerFireActor::OnDamageZoneEndOverlap);

	// Check for actors already overlapping on spawn
	TArray<AActor*> OverlappingActors;
	DamageZone->GetOverlappingActors(OverlappingActors);
	for (AActor* Actor : OverlappingActors)
	{
		if (Actor && Actor != SpawnerVehicle.Get())
		{
			ApplyDOTToTarget(Actor);
		}
	}
}

void AAfterburnerFireActor::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	// Remove DOT from all affected targets before destruction
	for (auto& Pair : ActiveDOTEffects)
	{
		if (AActor* Target = Pair.Key.Get())
		{
			RemoveDOTFromTarget(Target);
		}
	}
	ActiveDOTEffects.Empty();

	Super::EndPlay(EndPlayReason);
}

void AAfterburnerFireActor::OnDamageZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Skip spawner vehicle (self damage prevention)
	if (!OtherActor || OtherActor == SpawnerVehicle.Get())
	{
		return;
	}

	// Only apply damage on server
	if (!HasAuthority())
	{
		return;
	}

	ApplyDOTToTarget(OtherActor);
}

void AAfterburnerFireActor::OnDamageZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor || !HasAuthority())
	{
		return;
	}

	RemoveDOTFromTarget(OtherActor);
}

void AAfterburnerFireActor::ApplyDOTToTarget(AActor* Target)
{
	if (!Target || !DOTEffectClass)
	{
		return;
	}

	// Check if target has ability system
	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Target);
	if (!ASI)
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = ASI->GetAbilitySystemComponent();
	if (!TargetASC)
	{
		return;
	}

	// Don't apply twice
	if (ActiveDOTEffects.Contains(Target))
	{
		return;
	}

	// Create and apply DOT effect
	FGameplayEffectContextHandle Context = TargetASC->MakeEffectContext();
	Context.AddSourceObject(this);
	if (SpawnerVehicle.IsValid())
	{
		Context.AddInstigator(SpawnerVehicle.Get(), Cast<APawn>(SpawnerVehicle.Get()));
	}

	FGameplayEffectSpecHandle Spec = TargetASC->MakeOutgoingSpec(DOTEffectClass, 1, Context);
	if (Spec.IsValid())
	{
		FActiveGameplayEffectHandle Handle = TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		if (Handle.IsValid())
		{
			ActiveDOTEffects.Add(Target, Handle);
		}
	}
}

void AAfterburnerFireActor::RemoveDOTFromTarget(AActor* Target)
{
	if (!Target)
	{
		return;
	}

	FActiveGameplayEffectHandle* HandlePtr = ActiveDOTEffects.Find(Target);
	if (!HandlePtr || !HandlePtr->IsValid())
	{
		ActiveDOTEffects.Remove(Target);
		return;
	}

	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Target);
	if (ASI)
	{
		if (UAbilitySystemComponent* TargetASC = ASI->GetAbilitySystemComponent())
		{
			TargetASC->RemoveActiveGameplayEffect(*HandlePtr);
		}
	}

	ActiveDOTEffects.Remove(Target);
}
