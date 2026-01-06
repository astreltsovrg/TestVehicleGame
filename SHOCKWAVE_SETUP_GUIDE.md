# Shockwave Ability - Editor Setup Guide

## Prerequisites
- C++ code compiled successfully (TestVehicleGameEditor build passed)
- `Cooldown.Shockwave` tag added to `Config/DefaultGameplayTags.ini`

---

## Step 1: Create Input Action

1. Open Content Browser
2. Navigate to `Content/VehicleTemplate/Input/Actions/` (or create this folder)
3. Right-click → **Input** → **Input Action**
4. Name it `IA_Shockwave`
5. Open `IA_Shockwave` and configure:
   - **Value Type**: `Digital (bool)`
   - **Triggers**: Add `Pressed` trigger (optional, default works)
6. Save

---

## Step 2: Add Key Mapping

1. Open your Input Mapping Context (e.g., `IMC_Vehicle_Default`)
2. Click **+** to add new mapping
3. Set:
   - **Input Action**: `IA_Shockwave`
   - **Key**: `X`
4. Save

---

## Step 3: Configure Vehicle Blueprint

1. Open your vehicle Blueprint (e.g., `BP_SportsCar` or `BP_OffroadCar`)
2. Select the root component or open Class Defaults
3. Find **Input** section:
   - Set **Shockwave Action** → `IA_Shockwave`
4. Find **GAS** section:
   - In **Default Abilities** array, click **+**
   - Add `GA_Shockwave` class
   - In **Default Effects** array, click **+**
   - Add `GE_EnergyRegen` class (for energy regeneration)
5. Compile and Save Blueprint

---

## Step 4: Verify GameplayTags (Optional)

1. Go to **Project Settings** → **GameplayTags**
2. Verify `Cooldown.Shockwave` tag exists
3. If not, add it manually or check that `DefaultGameplayTags.ini` is loaded

---

## Testing

1. Play in Editor (PIE)
2. Drive near physics objects (cones, barrels, etc.)
3. Press **X** to activate Shockwave
4. Verify:
   - [ ] Objects are pushed away from vehicle
   - [ ] Energy is consumed (check HUD if available)
   - [ ] Cooldown works (X doesn't work for 10 seconds)
   - [ ] After cooldown, ability works again

---

## Troubleshooting

### Ability doesn't activate
- Check `ShockwaveAction` is set on vehicle Blueprint
- Check `GA_Shockwave` is in `DefaultAbilities` array
- Check energy level (need >= 30)
- Check cooldown isn't active

### Objects don't move
- Verify objects have **Simulate Physics** enabled
- Check collision channel (ECC_PhysicsBody)
- Try increasing `ImpulseStrength` in GA_Shockwave defaults

### Cooldown not working
- Verify `Cooldown.Shockwave` tag exists in GameplayTags
- Check GE_ShockwaveCooldown is properly configured

---

## Parameters (Tunable in Blueprint)

### Shockwave (GA_Shockwave)

| Property | Default | Description |
|----------|---------|-------------|
| `ImpulseRadius` | 800 | Radius of effect (units) |
| `ImpulseStrength` | 150000 | Force applied to objects |
| `EnergyCost` | 30 | Energy consumed per use |
| `bAffectVehicles` | true | Push other vehicles |
| `VehicleImpulseMultiplier` | 0.5 | Force multiplier for vehicles |

### Energy Regen (GE_EnergyRegen)

| Property | Default | Description |
|----------|---------|-------------|
| Period | 0.5s | Time between regen ticks |
| Regen Amount | +5 | Energy restored per tick |
| **Effective Rate** | **+10/sec** | Total energy per second |

To change defaults:
1. Create Blueprint child of `GA_Shockwave`
2. Override values in Class Defaults
3. Use the BP version in `DefaultAbilities`
