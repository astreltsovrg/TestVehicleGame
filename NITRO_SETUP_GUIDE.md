# Nitro Boost - Editor Setup Guide

This guide describes the steps needed to complete the Nitro Boost feature setup in Unreal Editor.

## Prerequisites

The C++ code is already compiled and ready. You need to create Blueprint assets and configure existing ones.

---

## Step 1: Create Input Action

1. Navigate to `Content/VehicleTemplate/Input/Actions/`
2. Right-click → Input → Input Action
3. Name it `IA_Nitro`
4. Open `IA_Nitro` and set:
   - **Value Type**: `Digital (bool)`
   - Save and close

---

## Step 2: Add Input Mapping

1. Open `Content/VehicleTemplate/Input/IMC_Vehicle_Default`
2. Click **+** to add a new mapping
3. Set:
   - **Action**: `IA_Nitro`
   - **Key**: `Z`
4. Save

---

## Step 3: Create GameplayEffects Folder

1. Navigate to `Content/VehicleTemplate/`
2. Create folder: `GAS`
3. Inside `GAS`, create folder: `Effects`

---

## Step 4: Create GE_NitroDrain Effect

1. Navigate to `Content/VehicleTemplate/GAS/Effects/`
2. Right-click → Blueprint Class → search for `GameplayEffect`
3. Name it `GE_NitroDrain`
4. Open and configure:

### Duration
| Property | Value |
|----------|-------|
| Duration Policy | `Infinite` |
| Period | `0.1` |
| Execute Periodic Effect on Application | `true` |

### Modifiers
Click **+** to add a modifier:
| Property | Value |
|----------|-------|
| Attribute | `NitroAttributeSet.NitroFuel` |
| Modifier Op | `Add` |
| Modifier Magnitude → Magnitude Calculation Type | `Scalable Float` |
| Modifier Magnitude → Scalable Float Magnitude | `-2.0` |

5. Save

---

## Step 5: Create GE_NitroRegen Effect

1. In `Content/VehicleTemplate/GAS/Effects/`
2. Right-click → Blueprint Class → `GameplayEffect`
3. Name it `GE_NitroRegen`
4. Open and configure:

### Duration
| Property | Value |
|----------|-------|
| Duration Policy | `Infinite` |
| Period | `0.2` |
| Execute Periodic Effect on Application | `false` |

### Modifiers
Click **+** to add a modifier:
| Property | Value |
|----------|-------|
| Attribute | `NitroAttributeSet.NitroFuel` |
| Modifier Op | `Add` |
| Modifier Magnitude → Magnitude Calculation Type | `Scalable Float` |
| Modifier Magnitude → Scalable Float Magnitude | `+2.0` |

5. Save

---

## Step 6: Create Abilities Folder and BP_GA_NitroBoost

1. In `Content/VehicleTemplate/GAS/`, create folder: `Abilities`
2. Navigate to `Content/VehicleTemplate/GAS/Abilities/`
3. Right-click → Blueprint Class → search for `GA_NitroBoost` (our C++ class)
4. Name it `BP_GA_NitroBoost`
5. Open and configure:

### Nitro Settings (in Details panel)
| Property | Value |
|----------|-------|
| Torque Boost Effect | `GE_NitroDrain` (or leave empty if using direct method) |
| Fuel Drain Effect | `GE_NitroDrain` |
| Torque Multiplier | `1.5` (default) |
| Fuel Cost Per Second | `20.0` (default) |
| Min Fuel To Activate | `5.0` (default) |

6. Save

---

## Step 7: Configure Vehicle Blueprints

### For BP_SportsCar_Pawn:

1. Open `Content/VehicleTemplate/Blueprints/SportsCar/BP_SportsCar_Pawn`
2. In Details panel, find **Input** section:
   - Set **Nitro Action** → `IA_Nitro`
3. Find **GAS** section:
   - **Default Abilities** → Click **+** → Select `BP_GA_NitroBoost`
   - **Default Effects** → Click **+** → Select `GE_NitroRegen`
4. Save

### For BP_OffroadCar_Pawn:

1. Open `Content/VehicleTemplate/Blueprints/OffroadCar/BP_OffroadCar_Pawn`
2. Repeat the same configuration as SportsCar
3. Save

---

## Step 8: Test

1. Play in Editor (PIE)
2. Drive the vehicle
3. Hold **Z** key to activate nitro
4. Observe:
   - Vehicle accelerates faster (1.5x torque)
   - Nitro fuel depletes (check with `GetNitroFuel()` in Blueprint or debug)
5. Release **Z**
6. Observe:
   - Vehicle returns to normal acceleration
   - Nitro fuel regenerates over time

---

## Optional: Add UI for Nitro Fuel

To display nitro fuel on HUD:

1. Open `Content/VehicleTemplate/Blueprints/UI/WBP_VehicleUI` (or create new widget)
2. Add a Progress Bar widget
3. In Graph, on Tick:
   - Get owning player pawn → Cast to `TestVehicleGamePawn`
   - Call `GetNitroFuel()` and `GetMaxNitroFuel()`
   - Set Progress Bar percent = NitroFuel / MaxNitroFuel

---

## Troubleshooting

### Nitro doesn't activate
- Check that `IA_Nitro` is assigned in vehicle Blueprint
- Check that `BP_GA_NitroBoost` is in `DefaultAbilities` array
- Verify Z key is mapped in `IMC_Vehicle_Default`

### No acceleration boost
- Verify `TorqueMultiplier` is set (default 1.5)
- Check console for errors related to GAS

### Fuel doesn't regenerate
- Verify `GE_NitroRegen` is in `DefaultEffects` array
- Check that the effect has correct Period and Modifier values

---

## File Structure After Setup

```
Content/VehicleTemplate/
├── Input/
│   ├── Actions/
│   │   ├── IA_Nitro.uasset          ← NEW
│   │   └── ... (existing actions)
│   └── IMC_Vehicle_Default.uasset   ← MODIFIED (added Z→IA_Nitro)
├── GAS/                              ← NEW FOLDER
│   ├── Abilities/
│   │   └── BP_GA_NitroBoost.uasset  ← NEW
│   └── Effects/
│       ├── GE_NitroDrain.uasset     ← NEW
│       └── GE_NitroRegen.uasset     ← NEW
└── Blueprints/
    ├── SportsCar/
    │   └── BP_SportsCar_Pawn.uasset ← MODIFIED
    └── OffroadCar/
        └── BP_OffroadCar_Pawn.uasset ← MODIFIED
```
