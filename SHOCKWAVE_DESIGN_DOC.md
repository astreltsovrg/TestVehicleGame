# Shockwave Ability - Design Document

## Overview

**Название:** Shockwave / Ударная волна
**Тип:** Instant ability с cooldown
**Кнопка:** X
**Цель:** Отталкивать окружающие объекты (конусы, другие машины) импульсной волной

---

## Gameplay Design

### Механика
1. Игрок нажимает **X**
2. Вокруг машины создаётся сферический импульс (радиус ~800 units)
3. Все физические объекты в радиусе получают радиальную силу от центра машины
4. Способность уходит на **cooldown 10 секунд**
5. Расходуется **30 единиц Energy** (опционально)

### Параметры (настраиваемые в Blueprint)

| Параметр | Значение | Описание |
|----------|----------|----------|
| `ImpulseRadius` | 800.0 | Радиус действия в units |
| `ImpulseStrength` | 150000.0 | Сила импульса (для конусов/легких объектов) |
| `CooldownDuration` | 10.0 sec | Время перезарядки |
| `EnergyCost` | 30.0 | Расход энергии (0 = без расхода) |
| `bAffectVehicles` | true | Влияет ли на другие машины |
| `VehicleImpulseMultiplier` | 0.5 | Множитель силы для машин |

### Условия активации
- Cooldown завершён (нет тега `Cooldown.Shockwave`)
- Достаточно Energy (если используется ресурс)
- Машина на земле (опционально)

---

## Technical Design (GAS)

### Новые файлы

```
TestVehicleGame/Source/TestVehicleGame/GAS/
├── GA_Shockwave.h          # Gameplay Ability
├── GA_Shockwave.cpp
├── GE_ShockwaveCooldown.h  # Cooldown Effect (опционально, можно в BP)
└── GE_ShockwaveCooldown.cpp
```

### Модификации существующих файлов

| Файл | Изменения |
|------|-----------|
| `NitroAttributeSet.h` | Добавить `Energy`, `MaxEnergy` атрибуты |
| `NitroAttributeSet.cpp` | Инициализация, clamp, replication |
| `TestVehicleGamePawn.h` | Добавить `ShockwaveAction` (UInputAction*), `ShockwaveInputID` |
| `TestVehicleGamePawn.cpp` | Bind input, handlers |

### Класс GA_Shockwave

```cpp
UCLASS()
class UGA_Shockwave : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_Shockwave();

    virtual bool CanActivateAbility(...) const override;
    virtual void ActivateAbility(...) override;

protected:
    // Radius of the shockwave
    UPROPERTY(EditDefaultsOnly, Category = "Shockwave")
    float ImpulseRadius = 800.0f;

    // Strength of the impulse
    UPROPERTY(EditDefaultsOnly, Category = "Shockwave")
    float ImpulseStrength = 150000.0f;

    // Cooldown duration in seconds
    UPROPERTY(EditDefaultsOnly, Category = "Shockwave")
    float CooldownDuration = 10.0f;

    // Energy cost (0 = no cost)
    UPROPERTY(EditDefaultsOnly, Category = "Shockwave")
    float EnergyCost = 30.0f;

    // Cooldown GameplayEffect class
    UPROPERTY(EditDefaultsOnly, Category = "Shockwave")
    TSubclassOf<UGameplayEffect> CooldownEffect;

private:
    void PerformShockwave(AActor* SourceActor);
    void ApplyCooldown();
};
```

### Логика PerformShockwave()

```cpp
void UGA_Shockwave::PerformShockwave(AActor* SourceActor)
{
    FVector Origin = SourceActor->GetActorLocation();

    // Find all overlapping actors with physics
    TArray<FOverlapResult> Overlaps;
    FCollisionShape Sphere = FCollisionShape::MakeSphere(ImpulseRadius);

    GetWorld()->OverlapMultiByChannel(
        Overlaps,
        Origin,
        FQuat::Identity,
        ECC_PhysicsBody,  // или custom channel
        Sphere
    );

    for (const FOverlapResult& Overlap : Overlaps)
    {
        if (UPrimitiveComponent* Comp = Overlap.GetComponent())
        {
            if (Comp->IsSimulatingPhysics())
            {
                // Направление от центра к объекту
                FVector Direction = (Comp->GetComponentLocation() - Origin).GetSafeNormal();

                // Сила уменьшается с расстоянием (опционально)
                float Distance = FVector::Dist(Origin, Comp->GetComponentLocation());
                float FalloffMultiplier = 1.0f - (Distance / ImpulseRadius);

                FVector Impulse = Direction * ImpulseStrength * FalloffMultiplier;

                // Добавляем вертикальную составляющую для "подброса"
                Impulse.Z += ImpulseStrength * 0.3f * FalloffMultiplier;

                Comp->AddImpulse(Impulse, NAME_None, true);
            }
        }
    }
}
```

### GameplayTags

```ini
# Добавить в DefaultGameplayTags.ini или через редактор
+GameplayTags=(Tag="Ability.Shockwave")
+GameplayTags=(Tag="Cooldown.Shockwave")
+GameplayTags=(Tag="State.Ability.ShockwaveActive")
```

### Cooldown через GAS

```cpp
// В конструкторе GA_Shockwave
AbilityCooldownTags.AddTag(FGameplayTag::RequestGameplayTag("Cooldown.Shockwave"));

// Override GetCooldownTags()
const FGameplayTagContainer* UGA_Shockwave::GetCooldownTags() const
{
    return &AbilityCooldownTags;
}

// Cooldown Effect применяется автоматически если указан CooldownGameplayEffectClass
```

---

## Input Setup

### Enhanced Input

1. Создать `IA_Shockwave` (Digital, Triggered)
2. Добавить в `IMC_Vehicle_Default`: X → IA_Shockwave
3. В `TestVehicleGamePawn`:
   - `UInputAction* ShockwaveAction`
   - Bind к `ShockwaveStarted()` → `AbilityLocalInputPressed(ShockwaveInputID)`

### Input ID

```cpp
// В TestVehicleGamePawn.h
static constexpr int32 NitroInputID = 1;
static constexpr int32 ShockwaveInputID = 2;  // NEW
```

---

## Energy System (Optional)

### Расширение NitroAttributeSet

```cpp
// Добавить в NitroAttributeSet.h
UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Energy, Category = "Energy")
FGameplayAttributeData Energy;
ATTRIBUTE_ACCESSORS(UNitroAttributeSet, Energy)

UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxEnergy, Category = "Energy")
FGameplayAttributeData MaxEnergy;
ATTRIBUTE_ACCESSORS(UNitroAttributeSet, MaxEnergy)
```

### Или: Отдельный AttributeSet

Можно создать `UEnergyAttributeSet` для разделения ответственности.

---

## Blueprint Assets (Editor)

### Создать:
1. `Content/VehicleTemplate/Input/Actions/IA_Shockwave.uasset`
2. `Content/VehicleTemplate/GAS/Abilities/BP_GA_Shockwave.uasset`
3. `Content/VehicleTemplate/GAS/Effects/GE_ShockwaveCooldown.uasset`
4. (Опционально) `Content/VehicleTemplate/GAS/Effects/GE_EnergyRegen.uasset`

### GE_ShockwaveCooldown

| Property | Value |
|----------|-------|
| Duration Policy | `Has Duration` |
| Duration Magnitude | `10.0` (секунд) |
| Granted Tags | `Cooldown.Shockwave` |

---

## Testing Checklist

### Базовый тест (конусы)
- [ ] Нажать X рядом с конусами
- [ ] Конусы разлетаются от машины
- [ ] Cooldown работает (X не срабатывает 10 сек)
- [ ] После cooldown можно снова активировать

### Параметры
- [ ] Радиус соответствует `ImpulseRadius`
- [ ] Сила достаточна для разброса конусов
- [ ] Falloff работает (дальние объекты летят слабее)

### Edge cases
- [ ] Не влияет на саму машину игрока
- [ ] Работает в воздухе (если разрешено)
- [ ] Несколько объектов в радиусе обрабатываются

---

## Visual/Audio (Future)

### VFX (Niagara)
- Расширяющееся кольцо/сфера
- Частицы разлетающиеся от центра
- Искажение воздуха (distortion)

### SFX
- Низкочастотный "бум"
- Свист воздуха

### Camera
- Небольшой shake при активации
- Замедление времени на 0.1 сек (juice)

---

## Implementation Order

1. **Input Setup** — IA_Shockwave, bind в Pawn
2. **GA_Shockwave C++** — базовая логика без cooldown
3. **Тест на конусах** — проверить физику
4. **Cooldown** — добавить GE и tags
5. **Energy (optional)** — если нужен ресурс
6. **Polish** — VFX, SFX, параметры

---

## Serena Commands (для имплементации)

```python
# Найти API для радиального импульса
search_for_pattern(substring_pattern="AddRadialImpulse", relative_path="Engine/Source/Runtime")

# Найти как работает OverlapMulti
find_symbol(name_path_pattern="OverlapMultiByChannel")

# Посмотреть существующий Cooldown в GAS
search_for_pattern(substring_pattern="CooldownGameplayEffect", relative_path="Engine/Plugins/Runtime/GameplayAbilities")

# Изучить структуру GameplayAbility
get_symbols_overview(relative_path="Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/Abilities/GameplayAbility.h", depth=1)
```
