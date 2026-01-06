# Blink Ability - Design Document

## Overview

**Название:** Blink / Телепорт
**Тип:** Instant ability с cooldown
**Кнопка:** C
**Цель:** Мгновенно переместить машину вперёд на 100 метров, сохраняя скорость и состояние физики

---

## Gameplay Design

### Механика
1. Игрок нажимает **C**
2. Машина мгновенно телепортируется на 100 метров вперёд (по направлению движения или взгляда)
3. **Сохраняются:**
   - Линейная скорость (velocity)
   - Угловая скорость (angular velocity)
   - Состояние колёс (вращение, подвеска)
   - Ориентация машины
4. Способность уходит на **cooldown 15 секунд**
5. Расходуется **50 единиц Energy**

### Параметры (настраиваемые в Blueprint)

| Параметр | Значение | Описание |
|----------|----------|----------|
| `BlinkDistance` | 10000.0 (100м) | Дистанция телепорта в units |
| `CooldownDuration` | 15.0 sec | Время перезарядки |
| `EnergyCost` | 50.0 | Расход энергии |
| `bUseVelocityDirection` | true | Телепорт по направлению скорости (false = по forward вектору) |
| `CollisionCheckRadius` | 200.0 | Радиус проверки коллизий в точке назначения |
| `MaxCollisionSearchSteps` | 10 | Макс. попыток найти свободное место |

### Условия активации
- Cooldown завершён (нет тега `Cooldown.Blink`)
- Достаточно Energy (>= 50)
- Есть свободное место для телепорта (collision check)

### Edge Cases
- **Телепорт в стену:** Ищем ближайшее свободное место по траектории
- **Телепорт за пределы карты:** Ограничиваем World Bounds
- **Телепорт в воздухе:** Разрешено (машина продолжит падение)
- **Телепорт на склоне:** Сохраняем ориентацию, машина приземлится

---

## Technical Design (GAS + Multiplayer)

### Архитектура репликации

```
┌─────────────────┐     RPC      ┌─────────────────┐
│     Client      │ ──────────►  │     Server      │
│  (Prediction)   │              │  (Authority)    │
└─────────────────┘              └─────────────────┘
        │                               │
        │ Local Prediction              │ Validate & Execute
        ▼                               ▼
  ┌───────────┐                  ┌───────────────┐
  │ Predict   │                  │ Server Blink  │
  │ Teleport  │                  │ + Replicate   │
  └───────────┘                  └───────────────┘
        │                               │
        │ Correction if needed          │ Multicast
        ▼                               ▼
  ┌───────────┐                  ┌───────────────┐
  │  Smooth   │                  │ Other Clients │
  │ Reconcile │                  │  See Teleport │
  └───────────┘                  └───────────────┘
```

### Новые файлы

```
TestVehicleGame/Source/TestVehicleGame/GAS/
├── GA_Blink.h              # Gameplay Ability
├── GA_Blink.cpp
├── GE_BlinkCooldown.h      # Cooldown Effect
└── GE_BlinkCooldown.cpp
```

### Модификации существующих файлов

| Файл | Изменения |
|------|-----------|
| `TestVehicleGamePawn.h` | Добавить `BlinkAction`, `BlinkInputID`, Server RPC |
| `TestVehicleGamePawn.cpp` | Bind input, RPC implementation |
| `DefaultGameplayTags.ini` | Добавить `Cooldown.Blink` |

---

## Класс GA_Blink

```cpp
UCLASS()
class UGA_Blink : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_Blink();

    virtual bool CanActivateAbility(...) const override;
    virtual void ActivateAbility(...) override;

protected:
    // Distance to teleport in units (10000 = 100 meters)
    UPROPERTY(EditDefaultsOnly, Category = "Blink")
    float BlinkDistance = 10000.0f;

    // Energy cost
    UPROPERTY(EditDefaultsOnly, Category = "Blink")
    float EnergyCost = 50.0f;

    // Use velocity direction instead of forward vector
    UPROPERTY(EditDefaultsOnly, Category = "Blink")
    bool bUseVelocityDirection = true;

    // Minimum velocity to use velocity direction (otherwise use forward)
    UPROPERTY(EditDefaultsOnly, Category = "Blink")
    float MinVelocityForDirection = 500.0f;

    // Radius for collision check at destination
    UPROPERTY(EditDefaultsOnly, Category = "Blink")
    float CollisionCheckRadius = 200.0f;

    // Cooldown effect class
    UPROPERTY(EditDefaultsOnly, Category = "Blink")
    TSubclassOf<UGameplayEffect> CooldownEffectClass;

private:
    // Calculate destination with collision avoidance
    FVector CalculateBlinkDestination(AActor* Vehicle, FVector& OutDirection) const;

    // Check if location is valid for teleport
    bool IsLocationValid(UWorld* World, const FVector& Location, const FVector& VehicleExtent) const;

    // Find nearest valid location along path
    FVector FindValidLocation(UWorld* World, const FVector& Start, const FVector& End,
                              const FVector& VehicleExtent) const;

    // Execute the blink (called on server)
    void ExecuteBlink(AActor* Vehicle, const FVector& Destination);

    // Deduct energy
    void DeductEnergy();
};
```

---

## Multiplayer Implementation

### Стратегия репликации

**Chaos Vehicles** используют физическую репликацию через `FNetworkPhysicsComponent`.
Для телепорта нужно:

1. **Server Authority:** Телепорт выполняется ТОЛЬКО на сервере
2. **Physics State Sync:** После телепорта синхронизируем physics state
3. **Client Prediction:** Клиент может предсказать телепорт, сервер корректирует

### Server RPC в Pawn

```cpp
// TestVehicleGamePawn.h
UFUNCTION(Server, Reliable)
void Server_ExecuteBlink(FVector Destination, FVector PreservedVelocity,
                         FVector PreservedAngularVelocity);

UFUNCTION(NetMulticast, Unreliable)
void Multicast_OnBlinkExecuted(FVector NewLocation);
```

### Логика ExecuteBlink (Server)

```cpp
void UGA_Blink::ExecuteBlink(AActor* Vehicle, const FVector& Destination)
{
    AWheeledVehiclePawn* VehiclePawn = Cast<AWheeledVehiclePawn>(Vehicle);
    if (!VehiclePawn) return;

    UChaosWheeledVehicleMovementComponent* Movement =
        VehiclePawn->GetVehicleMovementComponent();
    if (!Movement) return;

    // 1. Capture current physics state
    FBodyInstance* BodyInstance = VehiclePawn->GetMesh()->GetBodyInstance();
    FVector CurrentVelocity = BodyInstance->GetUnrealWorldVelocity();
    FVector CurrentAngularVelocity = BodyInstance->GetUnrealWorldAngularVelocityInRadians();
    FRotator CurrentRotation = VehiclePawn->GetActorRotation();

    // 2. Teleport the vehicle
    VehiclePawn->SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);

    // 3. Restore physics state
    BodyInstance->SetLinearVelocity(CurrentVelocity, false);
    BodyInstance->SetAngularVelocityInRadians(CurrentAngularVelocity, false);

    // 4. Wake physics (important!)
    BodyInstance->WakeInstance();

    // 5. Notify clients (for VFX, sound, etc.)
    if (ATestVehicleGamePawn* TVGPawn = Cast<ATestVehicleGamePawn>(VehiclePawn))
    {
        TVGPawn->Multicast_OnBlinkExecuted(Destination);
    }
}
```

### Collision Check

```cpp
FVector UGA_Blink::CalculateBlinkDestination(AActor* Vehicle, FVector& OutDirection) const
{
    FVector Start = Vehicle->GetActorLocation();
    FVector Velocity = Vehicle->GetVelocity();

    // Determine direction
    if (bUseVelocityDirection && Velocity.SizeSquared() > FMath::Square(MinVelocityForDirection))
    {
        OutDirection = Velocity.GetSafeNormal();
    }
    else
    {
        OutDirection = Vehicle->GetActorForwardVector();
    }

    FVector IdealEnd = Start + OutDirection * BlinkDistance;

    // Get vehicle bounds
    FVector Origin, Extent;
    Vehicle->GetActorBounds(false, Origin, Extent);

    // Check if ideal location is valid
    UWorld* World = Vehicle->GetWorld();
    if (IsLocationValid(World, IdealEnd, Extent))
    {
        return IdealEnd;
    }

    // Find valid location along path
    return FindValidLocation(World, Start, IdealEnd, Extent);
}

bool UGA_Blink::IsLocationValid(UWorld* World, const FVector& Location,
                                 const FVector& VehicleExtent) const
{
    FCollisionShape Shape = FCollisionShape::MakeBox(VehicleExtent);
    FCollisionQueryParams Params;

    // Check for overlaps
    return !World->OverlapBlockingTestByChannel(
        Location,
        FQuat::Identity,
        ECC_Vehicle,  // or appropriate channel
        Shape,
        Params
    );
}

FVector UGA_Blink::FindValidLocation(UWorld* World, const FVector& Start,
                                      const FVector& End, const FVector& VehicleExtent) const
{
    // Binary search for valid position
    FVector LastValid = Start;
    FVector TestPoint = End;

    for (int32 i = 0; i < MaxCollisionSearchSteps; i++)
    {
        FVector Mid = (LastValid + TestPoint) * 0.5f;

        if (IsLocationValid(World, Mid, VehicleExtent))
        {
            LastValid = Mid;
        }
        else
        {
            TestPoint = Mid;
        }
    }

    return LastValid;
}
```

---

## Input Setup

### Enhanced Input

1. Создать `IA_Blink` (Digital, Triggered)
2. Добавить в `IMC_Vehicle_Default`: C → IA_Blink
3. В `TestVehicleGamePawn`:
   - `UInputAction* BlinkAction`
   - Bind к `BlinkStarted()` → `AbilityLocalInputPressed(BlinkInputID)`

### Input ID

```cpp
// В TestVehicleGamePawn.h
static constexpr int32 NitroInputID = 1;
static constexpr int32 ShockwaveInputID = 2;
static constexpr int32 BlinkInputID = 3;  // NEW
```

---

## Network Considerations

### Latency Handling

| Подход | Описание | Плюсы | Минусы |
|--------|----------|-------|--------|
| **Server Only** | Телепорт только на сервере | Простота, авторитетность | Задержка для клиента |
| **Client Prediction** | Клиент предсказывает | Мгновенный отклик | Сложность reconciliation |
| **Hybrid** | Predict + Server Verify | Баланс | Средняя сложность |

**Рекомендация:** Hybrid подход - клиент предсказывает телепорт, сервер валидирует и корректирует при необходимости.

### Physics Replication

Chaos Vehicles используют `FNetworkPhysicsComponent` для репликации физики:

```cpp
// После телепорта на сервере, physics state автоматически реплицируется
// через стандартный механизм Chaos Vehicle
```

### Важные моменты

1. **ETeleportType::TeleportPhysics** - критически важен для правильной работы физики после телепорта
2. **WakeInstance()** - будит физическое тело после телепорта
3. **SetLinearVelocity/SetAngularVelocity** - восстанавливает скорость ПОСЛЕ телепорта

---

## GameplayTags

```ini
# Добавить в DefaultGameplayTags.ini
+GameplayTags=(Tag="Ability.Blink",DevComment="Blink teleport ability")
+GameplayTags=(Tag="Cooldown.Blink",DevComment="Cooldown for Blink ability")
```

---

## Visual/Audio (Future)

### VFX (Niagara)
- **Start:** Эффект "сжатия" машины, искры
- **Trail:** Линия следа между start и end
- **End:** Эффект "расширения", вспышка

### SFX
- Звук "woosh" + электрический разряд
- Звук появления в новой точке

### Camera
- Небольшой FOV zoom при активации
- Screen warp effect (post-process)

---

## Testing Checklist

### Базовый тест
- [ ] Нажать C, машина телепортируется вперёд
- [ ] Скорость сохраняется после телепорта
- [ ] Cooldown работает (15 сек)
- [ ] Energy расходуется (50 единиц)

### Collision тесты
- [ ] Телепорт перед стеной - останавливается у стены
- [ ] Телепорт в узком месте - находит ближайшую валидную точку
- [ ] Телепорт с обрыва - работает, машина падает

### Multiplayer тесты
- [ ] Host видит свой телепорт мгновенно
- [ ] Client видит свой телепорт с минимальной задержкой
- [ ] Другие игроки видят телепорт
- [ ] Скорость корректно реплицируется
- [ ] Нет рассинхрона позиции после телепорта

### Edge cases
- [ ] Телепорт на максимальной скорости
- [ ] Телепорт в воздухе (после прыжка)
- [ ] Телепорт во время переворота
- [ ] Быстрое нажатие (спам) - cooldown блокирует

---

## Implementation Order

1. **GameplayTags** — добавить `Cooldown.Blink`
2. **GE_BlinkCooldown** — cooldown effect (15 сек)
3. **GA_Blink C++** — базовая логика телепорта (single player)
4. **Тест single player** — проверить работу
5. **Server RPC** — добавить серверную репликацию
6. **Multiplayer test** — проверить на dedicated server
7. **Collision avoidance** — умная проверка препятствий
8. **Polish** — VFX, SFX, camera effects

---

## Полезные ссылки Serena

```python
# Найти как работает телепорт с физикой
search_for_pattern(substring_pattern="ETeleportType", relative_path="Engine/Source/Runtime")

# Посмотреть FBodyInstance методы
find_symbol(name_path_pattern="FBodyInstance/SetLinearVelocity")

# Найти примеры Server RPC в GAS
search_for_pattern(substring_pattern="Server, Reliable",
                   relative_path="Engine/Plugins/Runtime/GameplayAbilities")

# Изучить Network Physics Component
find_symbol(name_path_pattern="FNetworkPhysicsComponent")
```

---

## Потенциальные проблемы и решения

| Проблема | Решение |
|----------|---------|
| Машина застревает после телепорта | Использовать `ETeleportType::TeleportPhysics`, вызвать `WakeInstance()` |
| Скорость теряется | Сохранять velocity до телепорта, восстанавливать после |
| Рассинхрон в мультиплеере | Server authority + proper physics replication |
| Телепорт внутрь геометрии | Collision check + binary search для валидной позиции |
| Задержка на клиенте | Client prediction с server reconciliation |
