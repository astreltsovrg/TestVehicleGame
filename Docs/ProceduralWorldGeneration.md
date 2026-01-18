# Procedural World Generation: Valheim/Enshrouded Style

Документ описывает архитектуру процедурной генерации мира в стиле Valheim/Enshrouded с использованием UE5 + VoxelPlugin2 + PCG.

---

## 🎯 World Assumptions (ОБЯЗАТЕЛЬНО К ПРОЧТЕНИЮ)

**Этот раздел фиксирует ключевые архитектурные решения проекта. Все остальные разделы должны соответствовать этим выборам.**

### Выбор #1: Тип мира

| Вариант | Статус |
|---------|--------|
| Конечный мир (Valheim disk, ~10km) | ❌ НЕ ИСПОЛЬЗУЕМ |
| **Бесконечный мир с eventual consistency** | ✅ ВЫБРАН |

**Implications:**
- Global preprocessing невозможен — мир бесконечен
- Все системы должны работать инкрементально
- Допускается eventual consistency (река может слегка измениться при исследовании upstream)

### Выбор #2: Реки

| Вариант | Статус |
|---------|--------|
| Физически корректная гидрология (accumulated flow, drainage) | ❌ Phase 2 / Nice-to-have |
| **MVP: Эстетические реки (width ~ distance_from_source)** | ✅ ВЫБРАН для v1.0 |

**Implications:**
- Нет IncrementalRiverGraph в v1.0
- Нет перерасчёта upstream
- Фиксированный max_width = 30m
- Река выглядит правдоподобно, но не симулирует реальную гидрологию

### Выбор #3: Источник истины для высоты

| Вариант | Статус |
|---------|--------|
| Runtime noise (height вычисляется каждый раз) | ❌ НЕ ИСПОЛЬЗУЕМ |
| **Baked textures (noise → texture при стриминге региона)** | ✅ ВЫБРАН |

**Implications:**
- Height ВСЕГДА читается из R32_FLOAT texture
- Noise используется только при baking, не в runtime
- Modifiers (rivers, roads, settlements) тоже в texture
- Один источник истины = нет рассинхрона

### Выбор #4: NavMesh scope

| Вариант | Статус |
|---------|--------|
| NavMesh для всего мира (terrain + structures) | ❌ НЕ ИСПОЛЬЗУЕМ |
| **NavMesh только для structures/dungeons** | ✅ ВЫБРАН |

**Implications:**
- Open world terrain: steering behaviors + slope checks + raycasts
- Звери: flocking + simple obstacle avoidance
- NPC на дорогах: spline following + local avoidance
- NavMesh строится только внутри зданий, пещер, данжей

### Выбор #5: Terrain modification layers

| Слой | Mutable? | Описание |
|------|----------|----------|
| Procedural base | ❌ Immutable | Seed-based terrain, roads, rivers |
| Player delta | ✅ Mutable | Копание, строительство, терраформинг |

**Implications:**
- Road terrain mods = часть procedural layer, НЕ delta
- Player НИКОГДА не может изменить procedural слой напрямую
- Delta хранит только player actions
- При загрузке: procedural regenerates + delta applies on top

---

## Содержание

1. [Обзор архитектуры](#обзор-архитектуры)
2. [Воксельный террейн](#воксельный-террейн)
3. [Геологические слои и майнинг](#геологические-слои-и-майнинг)
4. [Детерминированная генерация](#детерминированная-генерация)
5. [Settlement Grid](#settlement-grid)
6. [Деревни](#деревни)
7. [Данжи](#данжи)
8. [Дороги](#дороги)
9. [Реки](#реки)
10. [Озёра и водопады](#озёра-и-водопады)
11. [Бесконечный мир и регионы](#бесконечный-мир-и-регионы)
12. [PCG для растительности](#pcg-для-растительности)

---

## Обзор архитектуры

### Стек технологий
- **UE5.7** - игровой движок
- **VoxelPlugin2** - воксельный террейн с SDF
- **PCG Framework** - процедурное размещение объектов
- **Chaos Physics** - физика разрушения

### Ключевые принципы
1. **Детерминированность** - один seed = один и тот же мир всегда
2. **Ленивая генерация** - генерируем только то, что нужно
3. **Иерархичность** - глобальные структуры → локальные детали
4. **Delta-saving** - храним только изменения игрока

### Порядок генерации

**Всё запекается в текстуры при стриминге региона (см. World Assumptions #3):**

```
World Seed
    │
    └─→ Region Streaming (async, ahead of player)
          │
          ├─ 1. BAKING PHASE (background thread):
          │     │
          │     ├─ ComputeBaseNoise() → R32_FLOAT texture
          │     │     └─ PerlinFBM + Domain Warping
          │     │
          │     ├─ ComputeRivers() → modify height texture
          │     │     └─ Gradient descent, valley carving
          │     │
          │     ├─ ComputeSettlements() → modify height texture
          │     │     └─ Plateau flattening
          │     │
          │     └─ ComputeRoads() → modify height texture
          │           └─ Road bed flattening
          │
          └─ 2. RUNTIME PHASE (при приближении игрока):
                │
                ├─ Height = SampleTexture(RegionHeightMap, UV)
                │     └─ ОДИН источник истины!
                │
                ├─ POIs - из Settlement Grid
                └─ PCG Vegetation - из seed + height texture

⚠️ ВАЖНО: В runtime НЕТ вычисления noise!
   Height ВСЕГДА читается из baked texture.
   Это гарантирует:
   - Нет рассинхрона между системами
   - Быстрый runtime (texture sample vs noise computation)
   - Простой debug (можно визуализировать текстуру)
```

---

## Воксельный террейн

### Структура данных вокселя
```cpp
struct FVoxelData
{
    float Distance;        // SDF - форма террейна
    uint8 SurfaceMaterial; // Визуальный материал (трава, камень, снег...)

    // НЕ храним (вычисляем на лету при майнинге):
    // - ResourceType
    // - ResourceAmount
    // - Hardness
    // - BiomeID
};
```

### Материалы террейна
Назначаются в Voxel Graph по:
- **Высоте** (Height) - снег на вершинах, песок у воды
- **Уклону** (Slope) - камень на крутых склонах
- **Биому** (Biome) - болотная грязь, пустынный песок
- **Noise** - вариации для естественности

```
Voxel Graph логика:

IF Height > 800:
    Material = Snow
ELSE IF Slope > 45°:
    Material = Rock
ELSE IF Height < 50:
    Material = Sand
ELSE:
    Material = Grass + Noise variation
```

### Triplanar Mapping
Для отсутствия швов на склонах используется triplanar projection в материале террейна.

---

## Геологические слои и майнинг

### Вертикальная стратификация

```
Поверхность ═══════════════════════════════════════════════
    │
    ▼ 0-0.3m   VEGETATION LAYER (трава, корни)
    │          └─ Resource: Grass → Plant Fiber, Seeds
    │
    ▼ 0.3-2m   TOPSOIL (плодородный слой)
    │          └─ Resource: Soil → Dirt, Worms
    │
    ▼ 2-5m     SUBSOIL (подпочва)
    │          └─ Resource: Clay → Clay, Dirt
    │
    ▼ 5-15m    REGOLITH (рыхлые породы)
    │          └─ Resource: Gravel → Stone, Flint
    │
    ▼ 15-50m   BEDROCK (коренная порода)
    │          └─ Resource: Stone + ORE VEINS
    │
    ▼ 50m+     DEEP ROCK
               └─ Resource: Granite, Obsidian, редкие руды
```

### Вариации по биомам

| Глубина | Meadows | Mountains | Swamp | Beach |
|---------|---------|-----------|-------|-------|
| 0-0.3m | Grass | Grass/Rock | Reeds | Sand |
| 0.3-2m | Soil | Thin Soil | Peat | Sand |
| 2-5m | Clay | Rock | Mud/Clay | Wet Sand |
| 5-15m | Gravel | Rock | Clay | Sandstone |
| 15m+ | Stone | Granite | Stone | Stone |

### Рудные жилы (3D Noise)

```cpp
// Руды генерируются 3D Cellular Noise
CopperOre:
  - Depth: 15-40m
  - Noise: CellularNoise3D(pos, seed+1000, scale=50)
  - Threshold: > 0.7
  - Biomes: Meadows, BlackForest

IronOre:
  - Depth: 20-60m
  - Noise: CellularNoise3D(pos, seed+2000, scale=40)
  - Threshold: > 0.75

SilverOre:
  - Depth: 40-100m
  - Noise: CellularNoise3D(pos, seed+3000, scale=30)
  - Threshold: > 0.8
  - Biomes: Mountains only
```

### Вычисление ресурса на лету

```cpp
FMiningResult GetResourceAt(FVector WorldPos, int32 WorldSeed)
{
    float SurfaceHeight = GetSurfaceHeight(WorldPos.XY(), WorldSeed);
    float Depth = SurfaceHeight - WorldPos.Z;
    EBiome Biome = GetBiome(WorldPos.XY(), WorldSeed);

    // Проверка руд (приоритет)
    if (Depth > 15.f)
    {
        float CopperNoise = CellularNoise3D(WorldPos, WorldSeed + 1000, 50);
        if (CopperNoise > 0.7f && Depth < 40.f)
            return { CopperOre, Hardness: 5 };

        // ... другие руды
    }

    // Обычные слои по глубине
    if (Depth < 0.3f) return { Grass, Hardness: 1 };
    if (Depth < 2.f)  return { Soil, Hardness: 2 };
    if (Depth < 5.f)  return { Clay, Hardness: 3 };
    if (Depth < 15.f) return { Gravel, Hardness: 4 };

    return { Stone, Hardness: 5 };
}
```

---

## Детерминированная генерация

> **📋 TL;DR Decision**
>
> | Вопрос | Решение |
> |--------|---------|
> | Seed scope | Один seed = весь мир (terrain, POI, roads, resources) |
> | Storage | Delta-only (только изменения игрока) |
> | Runtime | Regenerate procedural + apply delta |
> | Floating point | Avoid `fmod()`, use integer hash |

### Принцип: Seed = Мир

```
World Seed: 12345
     │
     ├─→ Terrain Shape    (всегда одинаковый)
     ├─→ Biome Layout     (всегда одинаковый)
     ├─→ Resource Layers  (всегда одинаковый)
     ├─→ Ore Veins        (всегда одинаковый)
     ├─→ POI Positions    (всегда одинаковые)
     └─→ Road Network     (всегда одинаковая)

Noise(seed, position) = ВСЕГДА один и тот же результат
```

### Что хранить НЕ нужно

- ❌ Форму террейна (пересчитываем из seed)
- ❌ Позиции деревьев (пересчитываем)
- ❌ Слои ресурсов (пересчитываем)
- ❌ Руды (пересчитываем)

### Что храним: Только дельту

```
World Save File:
├── WorldSeed: 12345 (4 bytes)
├── Modified Chunks:
│   ├── Chunk[12,5]: Removed[(3,2,1), (3,2,2)], Added[(10,5,3,Stone)]
│   └── Chunk[15,8]: Added[(0,3,0,Wood)...(5,8,5,Wood)]  // дом игрока
├── Destroyed Procedural Objects:
│   ├── Tree_12_5_7832
│   └── Rock_15_3_2841
└── Player-Placed Objects:
    ├── Chest @ (1250, 680, 45), Contents: [...]
    └── Workbench @ (1245, 675, 45)
```

### Экономия памяти

```
Без delta: 8000×8000×200 × 5 bytes = 64 GB
С delta:   ~1 MB (seed + изменения игрока)
```

---

## Settlement Grid

> **📋 TL;DR Decision**
>
> | Вопрос | Решение |
> |--------|---------|
> | Подход | Встроенные плато в terrain (не поиск плоских мест) |
> | Биомы | ❌ НЕ в горах (Mountains), ❌ НЕ в воде |
> | Спейсинг | ~2 км между потенциальными плато |
> | Height flattening | Pre-baked в height texture (не runtime) |

### Проблема
Классический подход (terrain → find flat spots → place village) ненадёжен - может не найти подходящие места.

### Решение: Встроенные плато

Генератор террейна создаёт плоские места каждые ~2 км - естественно, но гарантированно.

**⚠️ ВАЖНО: Biome Restrictions**

```cpp
bool IsValidSettlementBiome(FVector2D Pos, int32 Seed)
{
    EBiome Biome = GetBiome(Pos, Seed);

    // Settlement Grid НЕ работает в этих биомах:
    switch (Biome)
    {
        case EBiome::Mountains:    return false;  // Слишком крутые склоны
        case EBiome::DeepOcean:    return false;  // Под водой
        case EBiome::Glacier:      return false;  // Непригодно для жизни
        case EBiome::Volcano:      return false;  // Опасно
        default:                   return true;
    }
}
```

**⚠️ ВАЖНО: Height Flattening - BAKED, не runtime!**

```cpp
// ❌ НЕПРАВИЛЬНО: Runtime вычисление (дорого!)
float GetHeight_WRONG(FVector2D Pos, int32 Seed)
{
    if (SettlementInfluence > 0.f)
    {
        // GetLocalAverageHeight - O(N) samples, ДОРОГО!
        float LocalAverage = GetLocalAverageHeight(Pos, 50.f, Seed);
        return FMath::Lerp(BaseHeight, LocalAverage, SettlementInfluence);
    }
}

// ✅ ПРАВИЛЬНО: Pre-baked в texture при стриминге региона
void BakeSettlementPlateau(FRegionHeightTexture& HeightTex, FVector2D SettlementPos)
{
    const float PlateauRadius = 150.f;
    float TargetHeight = SampleHeight(HeightTex, SettlementPos);  // Центр плато

    // Bake в текстуру - один раз при загрузке региона
    ForEachTexelInRadius(SettlementPos, PlateauRadius, [&](int32 X, int32 Y)
    {
        float Dist = GetDistanceToCenter(X, Y, SettlementPos);
        float Influence = 1.f - FMath::Square(Dist / PlateauRadius);

        float CurrentHeight = HeightTex.Get(X, Y);
        HeightTex.Set(X, Y, FMath::Lerp(CurrentHeight, TargetHeight, Influence));
    });
}

float GetSettlementGridInfluence(FVector2D Pos, int32 Seed)
{
    const float GridSpacing = 2000.f;    // Каждые 2 км
    const float PlateauRadius = 150.f;   // Радиус плоской зоны
    const float Jitter = 500.f;          // Случайное смещение

    // Найти ближайший узел сетки с jitter
    FVector2D GridCell = Floor(Pos / GridSpacing);

    for (каждый из 9 соседних узлов)
    {
        FVector2D NodePos = GetJitteredNodePosition(Cell, Seed);

        if (!IsValidSettlementNode(NodePos, Seed))
            continue;  // Не в воде, не слишком высоко

        float Dist = Distance(Pos, NodePos);
        if (Dist < PlateauRadius)
        {
            return 1.f - Pow(Dist / PlateauRadius, 2);
        }
    }

    return 0.f;
}
```

### Визуализация

```
Settlement Grid (2 км ячейки):

┌─────┬─────┬─────┬─────┐
│  ○  │  ●  │  ○  │  ○  │   ○ = потенциальное плато
├─────┼─────┼─────┼─────┤   ● = занято деревней
│  ●  │  ○  │  ╳  │  ○  │   ╳ = непригодно (вода/горы)
├─────┼─────┼─────┼─────┤
│  ○  │  ╳  │  ○  │  ●  │   Jitter смещает от центра ячейки
└─────┴─────┴─────┴─────┘
```

---

## Деревни

### Размещение

```cpp
TArray<FVillageData> PlaceVillages(int32 Seed)
{
    // 1. Собрать все пригодные плато из Settlement Grid
    TArray<FSettlementSpot> Spots = CollectSettlementSpots(Seed);

    // 2. Отсортировать по качеству
    Spots.Sort(по Quality: близость к воде, биом, размер плато);

    // 3. Выбрать лучшие для деревень
    for (Spot : лучшие spots)
    {
        if (Distance до других деревень > 1000м)
        {
            Villages.Add(CreateVillage(Spot));
        }
    }

    return Villages;
}
```

### Типы деревень по биомам

| Биом | Типы деревень |
|------|---------------|
| Meadows | Farming (70%), Trading (30%) |
| BlackForest | Logging (60%), Hunting (40%) |
| Swamp | Fishing (на сваях) |
| Plains | Fortified (укреплённые) |
| Coast | Harbor (порт) |

### Layout деревни

**Radial Pattern:**
```
           🌾 Farm
             │
  House ○    │    ○ House
        ╲    │   ╱
         ╲   │  ╱
   Stable ○──⛪──○ Blacksmith
         ╱   │  ╲
        ╱    │   ╲
  House ○    │    ○ House
             │
          🍺 Tavern
```

**Linear Pattern (вдоль дороги):**
```
══════════════════════════════
   🏠  🏠  ⛪  🍺  🏠  🏠
══════════════════════════════
```

**Fortified (с стенами):**
```
╔═══🗼═══════🚪═══════🗼═══╗
║   🏠──🏠──🏠──🏠        ║
🗼  🏠──⛪──🍺──🏠       🗼
║   🏠──🔨──🏠──🏠        ║
╚═══🗼═══════════════🗼═══╝
```

### Типы зданий

```cpp
enum class EBuildingType : uint8
{
    // Жилые
    House_Small, House_Medium, House_Large,

    // Общественные
    TownHall, Temple, Tavern, Market, Well,

    // Производство
    Blacksmith, Sawmill, Farm, Stable, Windmill,

    // Военные
    Barracks, WatchTower, Wall_Section, Gate,

    // Специальные
    Harbor_Dock, FishingHut, LumberCamp, MiningCamp,
};
```

---

## Данжи

### Размещение по прогрессии

```cpp
TArray<FDungeonSpawnRule> Rules = {
    // Начальные (близко к центру)
    { BurialChamber, Meadows, 300-2000m, 5 штук },
    { TrollCave, BlackForest, 1500-4000m, 4 штуки },

    // Средние
    { SunkenCrypt, Swamp, 3000-6000m, 6 штук },
    { FrostCave, Mountain, 4000-7000m, 4 штуки },

    // Сложные
    { FulingVillage, Plains, 5000-8000m, 3 штуки },
    { InfernalPit, Ashlands, 7000-10000m, 2 штуки },
};
```

### Алгоритмы генерации layout

**BSP (Binary Space Partitioning):**
```
╔═══════════╦═══════════════════╗
║ ENTRANCE  ║    CORRIDOR       ║
╠═══════════╣═══════╦═══════════╣
║  CRYPT 1  ║ TRAP  ║  TREASURE ║
╠═══════════╣═══════╬═══════════╣
║  CRYPT 2  ║ HALL  ║   BOSS    ║
╚═══════════╩═══════╩═══════════╝
```

**Cellular Automata (пещеры):**
```
      ╭──────────╮
      │ ENTRANCE │
      ╰────┬─────╯
    ╭──────┴───────────╮
   ╱   ~~~~  POOL ~~~~  ╲
    ╲        │        ╱
      ╲  ╭───┴───╮  ╱
        │  BOSS  │
        ╰───────╯
```

**Prefab (модульный):**
- Готовые комнаты соединяются через "двери"
- Гарантированная проходимость

### Специальные комнаты

```cpp
void AssignSpecialRooms(FDungeonLayout& Layout)
{
    // Entrance - ближайшая к (0,0)
    // Boss - самая дальняя от entrance
    // Treasure - рядом с боссом или тупик
    // Trap - на пути к боссу
    // Secret - изолированная, низкий шанс
}
```

---

## Дороги

> **📋 TL;DR Decision**
>
> | Вопрос | Решение |
> |--------|---------|
> | Генерация | A* между POI с slope/water/biome penalties |
> | Terrain modification | Baked в height texture (immutable) |
> | Player interaction | Только декорации, НЕ terrain |
> | Gateway resolution | 3-level: exact → sliding → terrain mod |

**⚠️ КРИТИЧНО: Procedural vs Player Delta (см. World Assumptions #5)**

```
┌─────────────────────────────────────────────────────────────────┐
│              TERRAIN MODIFICATION LAYERS                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Layer 1: PROCEDURAL (immutable, regenerates from seed)         │
│    ├── Base terrain noise                                       │
│    ├── Settlement plateaus                                      │
│    ├── River valleys                                            │
│    └── Road beds ← СЮДА ВХОДЯТ ДОРОГИ!                         │
│                                                                 │
│  Layer 2: PLAYER DELTA (mutable, saved to disk)                 │
│    ├── Excavation (копание)                                     │
│    ├── Construction foundations                                 │
│    └── Terraforming                                             │
│                                                                 │
│  ⚠️ Player НИКОГДА не модифицирует Layer 1!                    │
│     Road terrain mods = часть seed-based generation             │
│     При reload: Layer 1 regenerates → Layer 2 applies on top    │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Граф дорог

```cpp
// 1. Delaunay triangulation всех POI
// 2. Minimum Spanning Tree (гарантированная связность)
// 3. + дополнительные альтернативные пути

TArray<FRoadEdge> BuildRoadGraph(POIs, Seed)
{
    AllEdges = DelaunayTriangulate(POIs);
    MST = PrimMST(AllEdges);

    // Добавить альтернативы (shortcuts)
    Alternatives = SelectBestAlternatives(AllEdges - MST);

    return MST + Alternatives;
}
```

### A* Cost Function

```cpp
float GetRoadPathCost(FVector From, FVector To, int32 Seed)
{
    float Cost = Distance(From, To);

    // Slope penalty (крутизна)
    float Slope = abs(Height(To) - Height(From)) / Distance;
    Cost += Pow(Slope * 10, 2);

    // Water penalty (мосты дорогие)
    if (IsUnderwater(To)) Cost *= 5.f;

    // Biome penalty
    switch (GetBiome(To))
    {
        case Swamp:   Cost *= 2.f; break;
        case Mountain: Cost *= 1.5f; break;
        case Meadows: Cost *= 0.8f; break;
    }

    return Cost;
}
```

### Типы дорог

| Тип | Ширина | Материал | Между |
|-----|--------|----------|-------|
| MainRoad | 6м | Cobblestone | Деревня ↔ Деревня |
| SecondaryRoad | 4м | DirtRoad | Деревня ↔ Данж |
| Path | 2м | GrassPath | Мелкие POI |
| MountainPass | 3м | Rocky | Через горы |
| SwampPath | 3м | WoodPlanks | Гать через болото |

### Альтернативные пути

```
Варианты A → C:

1. "King's Road" (через B)
   📏 2200m  ⚔ Safe  🛡 Patrolled

2. "Mountain Shortcut"
   📏 1800m  ⚔ Dangerous  ❄ Cold

3. "River Road"
   📏 2500m  ⚔ Safe  🎣 Fishing spots
```

### Модификация террейна под дорогу

```cpp
void ApplyRoadToTerrain(const TArray<FVector>& Spline)
{
    for (Point : Spline)
    {
        for (Offset = -RoadWidth to +RoadWidth)
        {
            VoxelPos = Point + Perpendicular * Offset;
            TargetHeight = Lerp(OriginalHeight, RoadHeight, FlattenStrength);

            if (OriginalHeight > TargetHeight)
                CutTerrain(VoxelPos, TargetHeight);
            else
                FillTerrain(VoxelPos, TargetHeight);

            SetSurfaceMaterial(VoxelPos, RoadMaterial);
        }
    }
}
```

---

## Реки

> **📋 TL;DR Decision**
>
> | Вопрос | Решение |
> |--------|---------|
> | Модель рек | MVP: Эстетические (width ~ distance_from_source) |
> | Accumulated flow | ❌ НЕ в v1.0 (Phase 2) |
> | Max river width | 30m (жёсткий clamp) |
> | Preprocessing | Bake в texture при загрузке региона |
> | Гидрологическая точность | Не требуется для gameplay |
>
> **Ключевой вопрос:** *Заметит ли игрок разницу между физически корректной гидрологией и эвристикой `width ~ distance`?*
> **Ответ:** Нет. Для Valheim-style gameplay важна эстетика, не симуляция.

---

### MVP: Эстетические реки (v1.0)

**Принцип:** Река выглядит правдоподобно, но НЕ симулирует реальную гидрологию.

```cpp
// ПРОСТАЯ МОДЕЛЬ: ширина зависит от расстояния от истока
float GetRiverWidth(float DistanceFromSource)
{
    const float MinWidth = 3.f;     // Исток - ручей
    const float MaxWidth = 30.f;    // ЖЁСТКИЙ CLAMP!
    const float GrowthRate = 0.01f; // 1м ширины на каждые 100м длины

    return FMath::Clamp(
        MinWidth + DistanceFromSource * GrowthRate,
        MinWidth,
        MaxWidth
    );
}

// НЕ НУЖНО:
// - Accumulated flow
// - Drainage basin calculation
// - Incremental graph
// - Upstream dependencies
```

**Почему это работает для gameplay:**

| Физическая реальность | MVP эвристика | Разница для игрока |
|----------------------|---------------|-------------------|
| Река расширяется от притоков | Река расширяется от расстояния | ❌ Не заметит |
| Объём воды = сумма upstream | Визуальная ширина | ❌ Не заметит |
| Притоки увеличивают flow | Притоки просто пересекаются | ⚠️ Минимально |

**Детерминизм сохраняется:**
- `RiverSource(cell, seed)` → детерминированный исток
- `RiverPath(source, seed)` → детерминированная трассировка вниз по градиенту
- `RiverWidth(distance)` → детерминированная ширина

---

### River Grid (реализация MVP)

```
Сетка истоков рек (каждые 4 км):

┌─────────┬─────────┬─────────┐
│    ⬇    │   ╳    │    ⬇    │   ⬇ = исток реки (с jitter)
├─────────┼─────────┼─────────┤   ╳ = нет истока (noise < threshold)
│   ╳     │    ⬇    │   ╳     │
├─────────┼─────────┼─────────┤   Река течёт вниз по градиенту
│    ⬇    │   ╳     │    ⬇    │   до моря или локального минимума
└─────────┴─────────┴─────────┘
```

### Вычисление влияния реки на высоту

```cpp
float GetRiverValleyInfluence(FVector2D Pos, int32 Seed)
{
    const float RiverGridSize = 4000.f;  // Исток каждые 4 км
    const float SearchRadius = 3;         // Проверяем соседние ячейки

    FIntPoint GridCell = FIntPoint(FMath::FloorToInt(Pos.X / RiverGridSize),
                                   FMath::FloorToInt(Pos.Y / RiverGridSize));

    float TotalInfluence = 0.f;

    // Проверяем ячейки в радиусе (река может течь далеко от истока)
    for (int32 dx = -SearchRadius; dx <= SearchRadius; dx++)
    {
        for (int32 dy = -SearchRadius; dy <= SearchRadius; dy++)
        {
            FIntPoint Cell = GridCell + FIntPoint(dx, dy);

            // Детерминированный seed для этой ячейки
            int32 CellSeed = HashCombine(HashCombine(Seed, Cell.X), Cell.Y);
            CellSeed = HashCombine(CellSeed, 7777);  // River magic number

            // Не в каждой ячейке есть исток
            if (!HasRiverSource(Cell, CellSeed))
                continue;

            // Вычислить сплайн реки детерминированно
            FRiverSpline River = ComputeRiverSpline(Cell, CellSeed, Seed);

            // Влияние этой реки на точку Pos
            float Influence = ComputeRiverInfluenceAtPoint(Pos, River);
            TotalInfluence = FMath::Max(TotalInfluence, Influence);
        }
    }

    return TotalInfluence;
}
```

### Детерминированный сплайн реки

> ⚠️ **Примечание**: Эта функция вызывается при **PREPROCESSING** региона, ДО baking текстуры.
> Поэтому здесь допустимо вызывать PerlinFBM напрямую — текстура ещё не существует!

```cpp
// PREPROCESSING ONLY — вызывается при стриминге региона
FRiverSpline ComputeRiverSpline(FIntPoint Cell, int32 CellSeed, int32 WorldSeed)
{
    FRiverSpline Result;

    // 1. Исток - позиция в ячейке с jitter
    FVector2D SourcePos = GetJitteredPosition(Cell, CellSeed, RiverGridSize, 1000.f);
    Result.Points.Add(SourcePos);

    // 2. Трассировка вниз по градиенту (БЕЗ влияния рек!)
    FVector2D Current = SourcePos;
    const float StepSize = 50.f;
    const int32 MaxSteps = 500;

    for (int32 i = 0; i < MaxSteps; i++)
    {
        // Градиент от BASE noise (не от финальной высоты!)
        FVector2D Gradient = GetBaseHeightGradient(Current, WorldSeed);

        if (Gradient.IsNearlyZero())
            break;  // Локальный минимум → озеро

        FVector2D Next = Current - Gradient.GetSafeNormal() * StepSize;

        float NextHeight = GetBaseHeight(Next, WorldSeed);
        if (NextHeight < SeaLevel)
            break;  // Достигли моря

        Result.Points.Add(Next);
        Current = Next;
    }

    return Result;
}
```

### Ключевой момент: Избегаем рекурсии

```cpp
// НЕПРАВИЛЬНО - бесконечная рекурсия!
float GetHeight(FVector2D Pos, int32 Seed)
{
    float RiverInfluence = GetRiverValleyInfluence(Pos, Seed);
    // GetRiverValleyInfluence → ComputeRiverSpline → GetHeight(следующая точка)
    // → GetRiverValleyInfluence → ... БЕСКОНЕЧНОСТЬ!
}

// ПРАВИЛЬНО - два слоя (используется при BAKING текстуры, НЕ в runtime!)
// См. World Assumptions #3: runtime читает из baked texture
float GetBaseHeight_ForBaking(FVector2D Pos, int32 Seed)
{
    // Только noise, без рек и поселений
    return PerlinFBM(Pos, Seed);  // ← Вызывается ТОЛЬКО при baking региона!
}

float GetFinalHeight_ForBaking(FVector2D Pos, int32 Seed)
{
    float Base = GetBaseHeight_ForBaking(Pos, Seed);
    float RiverValley = GetRiverValleyInfluence(Pos, Seed);  // Использует GetBaseHeight
    float Settlement = GetSettlementGridInfluence(Pos, Seed);

    return Base - RiverValley + Settlement;
}
// Результат записывается в R32_FLOAT texture → runtime читает ТОЛЬКО из неё
```

---

### ⚠️ АРХИТЕКТУРНОЕ РЕШЕНИЕ: Preprocessing vs Lazy Compute

**Проблема с lazy compute:**
```
Игрок подлетает к устью реки
  → GetHeight() требует river influence
  → GetRiverValleyInfluence() находит ячейку River Grid
  → Исток реки за 10 км отсюда
  → ComputeRiverSpline() трассирует 500+ шагов
  → 💥 HITCH: 200-500ms фриз
```

Даже с кэшированием сплайнов, **первое обращение** к любому чанку с рекой требует вычисления ВСЕЙ реки от истока.

**РЕКОМЕНДУЕМОЕ РЕШЕНИЕ: Region Preprocessing**

```cpp
// Реки считаются НЕ внутри Voxel Graph, а при загрузке региона
class FWorldRegion
{
public:
    // Вызывается асинхронно при стриминге региона
    void PrecomputeRivers(int32 WorldSeed)
    {
        // 1. Найти все истоки River Grid в этом регионе + соседних
        TArray<FIntPoint> SourceCells = FindRiverSourcesInRange(RegionBounds, WorldSeed);

        // 2. Трассировать каждую реку (в background thread!)
        for (FIntPoint Cell : SourceCells)
        {
            FRiverSpline Spline = ComputeRiverSpline(Cell, WorldSeed);

            // 3. Сохранить только сегменты, проходящие через наш регион
            TArray<FVector2D> ClippedPoints = ClipSplineToRegion(Spline, RegionBounds);
            if (ClippedPoints.Num() > 0)
            {
                RiverSplines.Add(ClippedPoints);
            }
        }

        // 4. Построить spatial index для быстрого distance query
        RiverSpatialIndex.Build(RiverSplines);
    }

    // Вызывается из Voxel Graph - ДЕШЕВО!
    float GetRiverInfluence(FVector2D Pos) const
    {
        // Просто distance to nearest segment - O(1) с spatial index
        float Distance = RiverSpatialIndex.GetDistanceToNearest(Pos);
        return ComputeValleyProfile(Distance);
    }

private:
    TArray<TArray<FVector2D>> RiverSplines;
    FRiverSpatialIndex RiverSpatialIndex;
};
```

**⚠️ АРХИТЕКТУРНЫЙ ЭЛЕМЕНТ: RiverSplineCache (избежание O(N²))**

```
Проблема без кэша:

Region A загружается:
  → FindRiverSources() находит River#1, River#2
  → ComputeRiverSpline(River#1) — 500ms
  → ComputeRiverSpline(River#2) — 300ms

Region B загружается (соседний):
  → FindRiverSources() находит River#1, River#3
  → ComputeRiverSpline(River#1) — 500ms ← ДУБЛИКАТ!
  → ComputeRiverSpline(River#3) — 400ms

Без кэша: одна река пересчитывается для каждого региона, через который течёт.
River#1 течёт через 5 регионов = 5 × 500ms = 2.5 секунды wasted work.
```

```cpp
// АРХИТЕКТУРНЫЙ ЭЛЕМЕНТ (не просто оптимизация!)
class FRiverSplineCache
{
    // Key = River source cell (детерминированный из seed)
    TMap<FIntPoint, FRiverSpline> CachedSplines;
    FCriticalSection CacheLock;  // Thread-safe для async loading

public:
    // Вызывается из FWorldRegion::PrecomputeRivers()
    FRiverSpline GetOrCompute(FIntPoint SourceCell, int32 WorldSeed)
    {
        FScopeLock Lock(&CacheLock);

        // Уже посчитали для другого региона?
        if (FRiverSpline* Cached = CachedSplines.Find(SourceCell))
        {
            return *Cached;  // O(1) - бесплатно!
        }

        // Первый раз — считаем и кэшируем
        int32 CellSeed = HashCombine(HashCombine(WorldSeed, SourceCell.X), SourceCell.Y);
        FRiverSpline Spline = ComputeRiverSpline(SourceCell, CellSeed, WorldSeed);

        CachedSplines.Add(SourceCell, Spline);
        return Spline;
    }

    // При unload далёких регионов — можно чистить
    void PurgeUnusedSplines(const TSet<FIntPoint>& ActiveRegions)
    {
        // Оставляем только реки, проходящие через активные регионы
        // (опционально, для экономии памяти)
    }
};

// Глобальный кэш (или per-world)
static FRiverSplineCache GRiverSplineCache;
```

**Важно:**
```
Cache is performance-only, does NOT affect determinism.

Один и тот же seed + source cell = один и тот же spline.
Кэш только избегает повторного вычисления.
При очистке кэша — результат генерации не меняется.
```

**Сложность с кэшем vs без:**

| Сценарий | Без кэша | С кэшем |
|----------|----------|---------|
| 5 регионов, 1 река через все | 5 × 500ms = 2.5s | 500ms |
| 25 регионов, 10 рек | ~50 × 400ms = 20s | ~10 × 400ms = 4s |
| Экономия | — | **5x faster** |

**Почему preprocessing лучше:**

| Критерий | Lazy Compute | Region Preprocessing |
|----------|--------------|---------------------|
| Hitches при первом доступе | ❌ 200-500ms | ✅ Нет (async) |
| Async loading | ⚠️ Сложно | ✅ Естественно |
| Память | ✅ Минимум | ⚠️ ~10KB на регион |
| Детерминизм | ✅ | ✅ (тот же seed → те же реки) |
| Код в Voxel Graph | ⚠️ Сложный | ✅ Просто distance query |

**Детерминизм сохраняется!**
```cpp
// Это тоже чистая функция, просто вызывается в другой момент:
TArray<FRiverSpline> ComputeRegionRivers(FIntPoint RegionCoord, int32 WorldSeed);

// Результат тот же независимо от:
// - Порядка загрузки регионов
// - Пути игрока по миру
// - Времени вызова
```

**Интеграция с Voxel Graph:**

```cpp
// Voxel Graph получает реки как внешние данные
USTRUCT()
struct FVoxelNode_RiverInfluence : public FVoxelComputeNode
{
    // Ссылка на pre-computed данные региона
    VOXEL_INPUT_PIN(FVoxelRiverData, RiverData, nullptr);
    VOXEL_INPUT_PIN(FVoxelVector2DBuffer, Position, nullptr);
    VOXEL_OUTPUT_PIN(FVoxelFloatBuffer, Influence);

    virtual FString GenerateCode(FCode& Code) const override
    {
        // Просто читает distance, не вычисляет сплайн!
        return "{Influence} = GetRiverInfluenceFromData({RiverData}, {Position})";
    }
};
```

**⚠️ ВАЖНО: Сложность кастомных нод в Voxel Plugin 2**

VP2 (в отличие от VP1) сильно завязан на ISPC кодогенерацию. Написание кастомных C++ нод со сложными структурами (TArray сплайнов) — сложная задача:

```
VP1 (старая версия):
  C++ код → прямой вызов → работает

VP2 (новая версия):
  C++ код → ISPC codegen → vectorized execution

  Проблемы:
  - Передача TArray в ISPC-контекст — узкое место
  - Сложные структуры данных плохо ложатся на SIMD
  - Debug и профилирование затруднены
```

**РЕКОМЕНДУЕМАЯ АЛЬТЕРНАТИВА: Voxel Parameter Textures**

Запекаем pre-computed данные в 2D текстуру — это на порядок проще и быстрее:

```cpp
// При preprocessing региона — генерируем текстуру
class FRegionDataTexture
{
    UTexture2D* DataTexture;  // 512×512 или 1024×1024
    FVector2D RegionOrigin;
    float RegionSize;
    float TexelSize;          // RegionSize / TextureResolution

public:
    void BakeRegionData(const FWorldRegion& Region, int32 Resolution = 512)
    {
        DataTexture = UTexture2D::CreateTransient(Resolution, Resolution, PF_R16G16B16A16F);

        FFloat16Color* MipData = static_cast<FFloat16Color*>(
            DataTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));

        TexelSize = Region.Size / Resolution;

        for (int32 Y = 0; Y < Resolution; Y++)
        {
            for (int32 X = 0; X < Resolution; X++)
            {
                FVector2D WorldPos = Region.Origin + FVector2D(X, Y) * TexelSize;
                int32 Idx = Y * Resolution + X;

                // Запекаем все нужные данные в каналы текстуры:
                MipData[Idx].R = Region.GetRiverInfluence(WorldPos);    // River valley depth
                MipData[Idx].G = Region.GetRiverWidth(WorldPos);        // River width
                MipData[Idx].B = Region.GetRoadInfluence(WorldPos);     // Road flatten
                MipData[Idx].A = Region.GetSettlementInfluence(WorldPos); // Settlement plateau
            }
        }

        DataTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
        DataTexture->UpdateResource();
    }
};
```

**Использование в Voxel Graph (простой texture sample):**

```
Voxel Graph (нодовый редактор VP2):

┌─────────────┐     ┌─────────────────┐     ┌──────────────┐
│ World Pos XY│────→│ Transform to UV │────→│ Sample Tex2D │
└─────────────┘     │ (normalize)     │     │              │
                    └─────────────────┘     │ R = River    │
                                            │ G = Width    │
┌─────────────┐                             │ B = Road     │
│ Region      │────────────────────────────→│ A = Settle   │
│ DataTexture │                             └──────┬───────┘
└─────────────┘                                    │
                                                   ▼
                                            ┌──────────────┐
                                            │ Final Height │
                                            │ = Base - R   │
                                            └──────────────┘
```

**Сравнение подходов:**

| Аспект | Custom C++ Node | Parameter Texture |
|--------|-----------------|-------------------|
| Сложность реализации | ⚠️ Высокая (ISPC) | ✅ Низкая |
| Производительность | ⚠️ Зависит от структуры | ✅ GPU-оптимизирован |
| Передача данных | ⚠️ TArray → узкое место | ✅ Один bind текстуры |
| Debug | ⚠️ Сложно | ✅ Можно визуализировать |
| Память | ✅ Минимум | ⚠️ 512² × 8 bytes = 2MB |
| Точность | ✅ Float64 | ⚠️ Float16 (см. ниже!) |
| Интерполяция | ❌ Нужно руками | ✅ Bilinear бесплатно |

**⚠️ КРИТИЧНО: Проблема точности Float16 (Precision Issues)**

Half Float (PF_R16G16B16A16F) имеет только ~3-4 значащих цифры:

```
Float16 precision:
  - Диапазон: ±65504
  - Точность: ~0.1% от значения
  - Для высоты 2000м: точность ≈ 2м (!)

Проблема: "Ступеньки" (terracing artifacts) на пологих склонах
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   Float32 (идеально):     Float16 (артефакты):              │
│                                                             │
│   ╱                       ┌─┐                               │
│  ╱                        │ └─┐                             │
│ ╱                         │   └─┐ ← "ступеньки"            │
│╱                          │     └─┐                         │
│                           │       └─                        │
│                                                             │
│   Пологий склон           Тот же склон,                    │
│                           но с потерей точности             │
└─────────────────────────────────────────────────────────────┘
```

**Решение: Разделение по точности**

```cpp
class FRegionDataTextures
{
    // Текстура 1: Высота (КРИТИЧНА точность!)
    // Вариант A: R32_FLOAT (4 bytes per pixel, высокая точность)
    UTexture2D* HeightTexture;  // PF_R32_FLOAT

    // Вариант B: Два канала для height (если нужно сэкономить память)
    // R16 = целая часть (метры), G16 = дробная часть (сантиметры)

    // Текстура 2: Модификаторы (Float16 достаточно!)
    // R = River influence (0..1)
    // G = Road influence (0..1)
    // B = Settlement influence (0..1)
    // A = Reserved
    UTexture2D* ModifiersTexture;  // PF_R16G16B16A16F

public:
    void BakeRegionData(const FWorldRegion& Region, int32 Resolution = 512)
    {
        // === ВЫСОТА: Float32 для точности ===
        HeightTexture = UTexture2D::CreateTransient(Resolution, Resolution, PF_R32_FLOAT);

        float* HeightData = static_cast<float*>(
            HeightTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));

        // === МОДИФИКАТОРЫ: Float16 достаточно ===
        ModifiersTexture = UTexture2D::CreateTransient(Resolution, Resolution, PF_R16G16B16A16F);

        FFloat16Color* ModData = static_cast<FFloat16Color*>(
            ModifiersTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));

        for (int32 Y = 0; Y < Resolution; Y++)
        {
            for (int32 X = 0; X < Resolution; X++)
            {
                FVector2D WorldPos = Region.Origin + FVector2D(X, Y) * TexelSize;
                int32 Idx = Y * Resolution + X;

                // Высота - Float32 (полная точность)
                HeightData[Idx] = Region.GetPrecomputedHeight(WorldPos);

                // Модификаторы - Float16 (достаточно для 0..1)
                ModData[Idx].R = Region.GetRiverInfluence(WorldPos);
                ModData[Idx].G = Region.GetRoadInfluence(WorldPos);
                ModData[Idx].B = Region.GetSettlementInfluence(WorldPos);
                ModData[Idx].A = 0.f;  // Reserved
            }
        }

        HeightTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
        ModifiersTexture->GetPlatformData()->Mips[0].BulkData.Unlock();

        HeightTexture->UpdateResource();
        ModifiersTexture->UpdateResource();
    }
};
```

**Альтернатива: Кодирование высоты в два Float16 канала**

```cpp
// Если нужна одна текстура (экономия bind slots)
void EncodeHeightToTwoChannels(float Height, FFloat16& OutHigh, FFloat16& OutLow)
{
    // High = целые метры, Low = сантиметры
    const float Scale = 100.f;  // 1 единица Low = 1см

    float Meters = FMath::FloorToFloat(Height);
    float Centimeters = (Height - Meters) * Scale;

    OutHigh = FFloat16(Meters);
    OutLow = FFloat16(Centimeters);
}

float DecodeHeightFromTwoChannels(float High, float Low)
{
    return High + Low / 100.f;
}

// В Voxel Graph:
// Height = TexR + TexG / 100.0
```

**Когда какой формат использовать:**

| Данные | Float16 OK? | Рекомендация |
|--------|-------------|--------------|
| Высота террейна (0-2000м) | ❌ Артефакты | PF_R32_FLOAT или 2-channel encoding |
| River influence (0..1) | ✅ | PF_R16 |
| Road influence (0..1) | ✅ | PF_R16 |
| Settlement influence (0..1) | ✅ | PF_R16 |
| Biome ID (0..255) | ✅ | PF_R8 достаточно |
| River width (0..100м) | ⚠️ Зависит | Encode as width/100 → PF_R16 |

**Память:**

```
512×512 регион:
  - PF_R32_FLOAT (height):     512² × 4 = 1 MB
  - PF_R16G16B16A16F (mods):   512² × 8 = 2 MB
  - Total: 3 MB per region

vs

  - Одна PF_R16G16B16A16F:     512² × 8 = 2 MB
  - Но с артефактами высоты!
```

**3 MB на регион (4×4 км) — приемлемо для качества.**

**Texture Memory Budget (VRAM):**

```
Дальность прорисовки 5×5 регионов (25 активных регионов):

512×512 текстуры (текущий вариант):
  - 25 регионов × 3 MB = 75 MB VRAM
  - ✅ ОТЛИЧНО — это очень мало для terrain data

1024×1024 текстуры (если нужна детализация рек):
  - Height R32:    1024² × 4 = 4 MB
  - Modifiers:     1024² × 8 = 8 MB
  - Total:         12 MB per region
  - 25 регионов × 12 MB = 300 MB VRAM
  - ✅ Всё ещё приемлемо для mid-range GPU

Для сравнения:
  - Типичная AAA текстура terrain: 2-4 GB
  - Наш подход: 75-300 MB (в 10-50 раз меньше!)
```

**Когда увеличивать до 1024×1024:**
- Реки требуют точных береговых линий < 4м
- Дороги имеют сложные серпантины
- Settlement influence требует детализации отдельных зданий

**Рекомендация:** Начинать с 512×512, увеличивать только для регионов с реками/дорогами.

**⚠️ КРИТИЧНО: CPU Readback невозможен/дорог!**

R32_FLOAT texture живёт на GPU. Вызов `ReadPixels()` для CPU-доступа:
- Требует GPU→CPU transfer (stall)
- Latency 1-2 frames
- Для реалтайм queries — неприемлемо

```cpp
// ❌ НЕПРАВИЛЬНО: ReadPixels каждый кадр
float GetHeight_WRONG(FVector2D Pos)
{
    // GPU stall! 1-5ms per call!
    TArray<FFloat16Color> Pixels;
    HeightTexture->Source.GetMipData(Pixels, 0);
    return SampleFromPixels(Pixels, Pos);  // 💥 LAG
}
```

**Где это аукнется:**
- `GetSurfaceHeight()` для gameplay (placement, grounding)
- AI height queries (pathfinding, slope checks)
- Physics queries (collision with terrain)
- PCG placement logic вне Voxel Graph

**Решение: Параллельный CPU-side кэш**

```cpp
// Архитектура: GPU texture + CPU mirror
class FRegionHeightData
{
    // GPU-side: для Voxel Graph rendering
    UTexture2D* HeightTexture_GPU;  // PF_R32_FLOAT

    // CPU-side: для gameplay queries
    TArray<float> HeightCache_CPU;  // Копия данных
    int32 CacheResolution;          // Может быть ниже чем GPU (256 vs 512)

public:
    // При baking — заполняем ОБА
    void BakeHeight(...)
    {
        // 1. Bake в GPU texture (для рендера)
        BakeToGPUTexture(HeightTexture_GPU, ...);

        // 2. Параллельно храним в CPU array (для queries)
        HeightCache_CPU.SetNum(CacheResolution * CacheResolution);
        for (int32 Y = 0; Y < CacheResolution; Y++)
        for (int32 X = 0; X < CacheResolution; X++)
        {
            HeightCache_CPU[Y * CacheResolution + X] = ComputeHeight(X, Y);
        }
    }

    // ✅ ПРАВИЛЬНО: CPU query из кэша
    float GetHeight_Gameplay(FVector2D WorldPos) const
    {
        FVector2D UV = WorldToRegionUV(WorldPos);
        return BilinearSample(HeightCache_CPU, UV, CacheResolution);
    }
};
```

**Сводка по height queries:**

| Контекст | Источник | Latency |
|----------|----------|---------|
| Voxel Graph (render) | GPU R32 texture | 0 (GPU-side) |
| Gameplay (AI, physics) | CPU HeightCache | ~0.01ms |
| Editor tools | CPU HeightCache | ~0.01ms |
| ReadPixels (избегать!) | GPU→CPU transfer | 1-5ms + stall |

**Память CPU кэша:**
```
512×512 region × 4 bytes = 1 MB per region
25 regions = 25 MB RAM (приемлемо)

Можно снизить до 256×256 для gameplay (0.25 MB per region)
— точности хватит для AI и physics
```

**Детали реализации Parameter Texture:**

```cpp
// Формат текстуры и что в каких каналах
struct FRegionDataFormat
{
    // Текстура 1: Terrain modifiers (R16G16B16A16F)
    // R = River valley influence (0..1, 0 = no river, 1 = center of river)
    // G = River width at this point (in meters, encoded as width/100)
    // B = Road flatten influence (0..1)
    // A = Settlement plateau influence (0..1)

    // Текстура 2 (опционально): Additional data
    // R = Biome ID (encoded)
    // G = Ore density hint
    // B = Reserved
    // A = Reserved
};

// Bind в Voxel Graph
void FWorldRegion::BindToVoxelGraph(UVoxelGraphInstance* Graph)
{
    // VP2 поддерживает texture parameters
    Graph->SetTextureParameter("RegionData", DataTexture);
    Graph->SetVectorParameter("RegionBounds",
        FVector4(Origin.X, Origin.Y, Size, 1.f / Size));
}
```

**UV Transform в Voxel Graph:**

```cpp
// Преобразование World Position → UV для сэмплирования
FVector2D WorldToRegionUV(FVector2D WorldPos, FVector4 RegionBounds)
{
    float RegionOriginX = RegionBounds.X;
    float RegionOriginY = RegionBounds.Y;
    float RegionSize = RegionBounds.Z;
    float InvRegionSize = RegionBounds.W;

    return FVector2D(
        (WorldPos.X - RegionOriginX) * InvRegionSize,
        (WorldPos.Y - RegionOriginY) * InvRegionSize
    );
}

// В Voxel Graph это буквально 2 ноды:
// 1. Subtract (WorldPos - RegionOrigin)
// 2. Multiply (Result * InvRegionSize)
// 3. Sample Texture2D
```

**Обработка границ регионов:**

```cpp
// На границах регионов нужно сэмплировать обе текстуры
float SampleWithBorderBlend(FVector2D WorldPos, const TArray<FRegionDataTexture>& Regions)
{
    // Найти до 4 регионов, которые могут влиять на точку
    TArray<FRegionSample> Samples;

    for (const FRegionDataTexture& Region : Regions)
    {
        if (Region.ContainsOrNear(WorldPos, BlendDistance))
        {
            FVector2D UV = Region.WorldToUV(WorldPos);
            float Value = Region.Sample(UV);
            float Weight = Region.GetBlendWeight(WorldPos, BlendDistance);

            Samples.Add({ Value, Weight });
        }
    }

    // Weighted average
    float Sum = 0.f, WeightSum = 0.f;
    for (const FRegionSample& S : Samples)
    {
        Sum += S.Value * S.Weight;
        WeightSum += S.Weight;
    }

    return Sum / WeightSum;
}
```

**Визуализация Parameter Texture workflow:**

```
┌─────────────────────────────────────────────────────────────────┐
│                    PARAMETER TEXTURE WORKFLOW                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. Region Preprocessing (async, ahead of player)                │
│     └─→ PrecomputeRivers()                                       │
│     └─→ PrecomputeRoads()                                        │
│     └─→ BakeToTexture() ──────┐                                 │
│                                │                                 │
│                                ▼                                 │
│     ┌──────────────────────────────────────────┐                │
│     │  Region Data Texture (512×512)           │                │
│     │  ┌────┬────┬────┬────┐                   │                │
│     │  │ R  │ G  │ B  │ A  │                   │                │
│     │  │Rivr│Wdth│Road│Sttl│                   │                │
│     │  └────┴────┴────┴────┘                   │                │
│     │  ~2 MB per region (4km × 4km)            │                │
│     └──────────────────────────────────────────┘                │
│                                │                                 │
│                                ▼                                 │
│  2. Voxel Graph (runtime, per-voxel)                            │
│     ┌────────────────────────────────────────┐                  │
│     │  WorldPos → UV → Sample → Influence    │                  │
│     │  (3 nodes, GPU-optimized)              │                  │
│     └────────────────────────────────────────┘                  │
│                                                                  │
│  Результат: Простота + Скорость + Совместимость с VP2           │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**Рекомендация:**

| Сценарий | Подход |
|----------|--------|
| Prototype / быстрый старт | Parameter Textures |
| Production с VP2 | Parameter Textures |
| Нужна высокая точность | Custom C++ + fallback to textures |
| Сложная процедурная логика | Hybrid (textures + minimal C++) |

### Визуализация архитектуры (обновлённая)

```
┌─────────────────────────────────────────────────────────────┐
│                    RIVER ARCHITECTURE                        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. Region Streaming (async, ahead of player)               │
│     └─→ PrecomputeRivers() в background thread              │
│           ├─ ComputeRiverSpline() - тяжёлая операция       │
│           ├─ ClipToRegion() - только нужные сегменты       │
│           └─ BuildSpatialIndex() - для быстрых queries     │
│                                                             │
│  2. Voxel Graph (runtime, per-voxel)                        │
│     └─→ GetRiverInfluence(pos) - ДЕШЕВО!                   │
│           └─ SpatialIndex.GetDistance() - O(1)             │
│                                                             │
│  Результат: Детерминизм + Плавность + Простота             │
│                                                             │
└─────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────┐
│  BAKING PHASE (при стриминге региона):                       │
│  GetFinalHeight_ForBaking(pos, seed)                         │
│       │                                                      │
│       ├── PerlinFBM(pos, seed) + Domain Warping              │
│       ├── + GetSettlementInfluence(pos, seed)                │
│       ├── - Region.GetRiverInfluence(pos) [pre-computed]     │
│       └── → R32_FLOAT Texture (GPU + CPU mirror)             │
│                                                              │
│  RUNTIME (chunk generation):                                 │
│  GetHeight_Gameplay(pos)                                     │
│       └── Region.HeightCache_CPU[UV]  ← O(1) texture read   │
└──────────────────────────────────────────────────────────────┘

Ключевой инсайт (см. World Assumptions #3):
- PerlinFBM вызывается ТОЛЬКО при baking региона
- Runtime читает ТОЛЬКО из baked texture (O(1) lookup)
- Rivers - глобальные (10+ км), preprocessing обязателен
```

---

## 🔮 Phase 2: Физически корректные реки (Nice-to-have)

> ⚠️ **ВНИМАНИЕ: Этот раздел описывает сложную систему, НЕ НУЖНУЮ для v1.0!**
>
> MVP (выше) достаточен для gameplay. Этот раздел - референс на случай, если понадобится более реалистичная гидрология в будущем.
>
> **Не реализовывать без явного решения команды!**

### Слияние рек и накопление потока

**Проблема с локальным вычислением ширины:**

```
     A (источник, flow=1)
      ╲
       ╲
    B ══╬══ C (слияние, flow=3) ─── D ─── E (устье, flow=3)
       ╱
      ╱
     F (источник, flow=1)

Простой подход "ширина = f(расстояние от истока)" НЕВЕРЕН:
- Игнорирует притоки
- Река с 5 притоками = река без притоков?

Правильно: ширина = f(accumulated_flow)
Но accumulated_flow требует знания ВСЕГО графа выше по течению!
```

**Решение: Flow Graph в preprocessing**

```cpp
struct FRiverNetwork
{
    // Граф: вершина = точка сплайна, ребро = сегмент
    TArray<FRiverNode> Nodes;
    TArray<FRiverEdge> Edges;

    // Вычисляется при preprocessing
    void ComputeAccumulatedFlow()
    {
        // 1. Найти все источники (in-degree = 0)
        TArray<int32> Sources = FindSources();

        // 2. BFS от источников вниз по течению
        for (int32 SourceIdx : Sources)
        {
            Nodes[SourceIdx].AccumulatedFlow = 1.f;
        }

        // 3. Topological sort + propagate flow
        TArray<int32> TopoOrder = TopologicalSort();
        for (int32 NodeIdx : TopoOrder)
        {
            for (int32 DownstreamIdx : Nodes[NodeIdx].Downstream)
            {
                Nodes[DownstreamIdx].AccumulatedFlow += Nodes[NodeIdx].AccumulatedFlow;
            }
        }
    }
};

// Теперь ширина - просто lookup
float GetRiverWidth(const FRiverNode& Node)
{
    // Ширина пропорциональна корню из потока (физически корректно)
    return BaseWidth * FMath::Sqrt(Node.AccumulatedFlow);
}
```

**Детекция слияний:**

```cpp
void FWorldRegion::BuildRiverNetwork(int32 WorldSeed)
{
    // 1. Сначала трассируем ВСЕ реки независимо
    TArray<FRiverSpline> AllSplines;
    for (FIntPoint SourceCell : RiverSourceCells)
    {
        AllSplines.Add(ComputeRiverSpline(SourceCell, WorldSeed));
    }

    // 2. Находим точки слияния (где реки сближаются)
    for (int32 i = 0; i < AllSplines.Num(); i++)
    {
        for (int32 j = i + 1; j < AllSplines.Num(); j++)
        {
            if (FMergePoint Merge = FindMergePoint(AllSplines[i], AllSplines[j]))
            {
                // Меньшая река становится притоком большей
                MergeRivers(AllSplines[i], AllSplines[j], Merge);
            }
        }
    }

    // 3. Строим граф и считаем flow
    RiverNetwork.BuildFromSplines(AllSplines);
    RiverNetwork.ComputeAccumulatedFlow();

    // 4. Spatial index для queries
    SpatialIndex.Build(RiverNetwork);
}
```

**Визуализация preprocessing:**

```
┌─────────────────────────────────────────────────────────────┐
│                 RIVER PREPROCESSING                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. Trace all rivers independently                          │
│     A ────────────┐                                         │
│     B ──────────╮ │                                         │
│     F ────────╮ ╰─┴───────── (raw splines)                 │
│                                                             │
│  2. Detect merge points                                     │
│     A ────────┬──╮                                          │
│     B ────────┤  ├───────── (merged at confluence)         │
│     F ────────┴──╯                                          │
│                                                             │
│  3. Build flow graph + compute accumulated flow             │
│     A[1] ─────┬──╮                                          │
│     B[1] ─────┤  ├─[3]───── (flow propagated)              │
│     F[1] ─────┴──╯                                          │
│                                                             │
│  4. Width = f(accumulated_flow)                             │
│     Narrow ───┬──╮                                          │
│     Narrow ───┤  ├─ WIDE ── (width from flow)              │
│     Narrow ───┴──╯                                          │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**Ширина реки (физически корректная модель):**

```cpp
// Закон Леопольда: ширина ~ Q^0.5, глубина ~ Q^0.4
// где Q = расход воды (наш AccumulatedFlow)

float GetRiverWidth(float AccumulatedFlow)
{
    const float BaseWidth = 5.f;      // Минимальная ширина ручья
    const float Exponent = 0.5f;       // Закон Леопольда

    return BaseWidth * FMath::Pow(AccumulatedFlow, Exponent);
}

// Примеры:
// 1 источник:  5 * 1^0.5   = 5м
// 2 источника: 5 * 2^0.5   = 7м
// 4 источника: 5 * 4^0.5   = 10м
// 9 источников: 5 * 9^0.5  = 15м
```

**Межрегиональные реки:**

```cpp
// Река может начаться в регионе A и течь через B, C, D...
// Нужна координация между регионами

void FWorldRegion::PrecomputeRivers(int32 WorldSeed)
{
    // 1. Сначала ищем ВСЕ источники, которые могут влиять на наш регион
    //    (даже если они за несколько регионов upstream)
    FBox2D SearchBounds = RegionBounds.ExpandBy(MaxRiverLength);
    TArray<FIntPoint> AllPotentialSources = FindRiverSourcesInBounds(SearchBounds, WorldSeed);

    // 2. Трассируем все реки, оставляем только те, что проходят через нас
    for (FIntPoint Source : AllPotentialSources)
    {
        FRiverSpline Spline = ComputeRiverSpline(Source, WorldSeed);
        if (Spline.IntersectsBounds(RegionBounds))
        {
            LocalRivers.Add(Spline);
        }
    }

    // 3. Строим граф с учётом всех притоков
    BuildRiverNetwork(LocalRivers);
}
```

### Проблема "Потока из ниоткуда" (Cross-Region River Flow)

**Сценарий:**
```
Регион A (выгружен)     Регион B (игрок телепортировался сюда)
┌──────────────────┐    ┌──────────────────┐
│  ○ Source        │    │                  │
│  │               │    │   ???            │
│  │ flow=1        │    │   Какая ширина?  │
│  ○───────────────┼────┼─→ ═══════════    │
│  │ flow=2        │    │                  │
│  │               │    │                  │
└──────────────────┘    └──────────────────┘
```

Регион B не знает AccumulatedFlow на входе, потому что регион A не загружен.

**Решение 1: Global Low-Res River Graph (рекомендуется)**

Глобальный граф хранится всегда в памяти. Содержит только ключевые точки:

```cpp
// Глобальный граф - всегда в памяти (~1-10 MB для большого мира)
struct FGlobalRiverGraph
{
    // Узлы: источники + точки слияния + пересечения границ регионов
    struct FRiverNode
    {
        FVector2D Position;
        float AccumulatedFlow;      // Уже вычислено при создании мира
        int32 DownstreamNodeIdx;    // -1 если уходит за край мира
    };

    TArray<FRiverNode> Nodes;

    // Индекс для быстрого поиска: RegionID → узлы на границе
    TMap<FIntPoint, TArray<int32>> BorderNodes;
};

// Загрузка региона
void FWorldRegion::OnLoad(const FGlobalRiverGraph& GlobalGraph)
{
    // 1. Получаем входящий поток из глобального графа (O(1) lookup)
    TArray<int32> EntryNodes = GlobalGraph.BorderNodes.FindRef(RegionID);

    for (int32 NodeIdx : EntryNodes)
    {
        float IncomingFlow = GlobalGraph.Nodes[NodeIdx].AccumulatedFlow;
        FVector2D EntryPoint = GlobalGraph.Nodes[NodeIdx].Position;

        // 2. Локально трассируем реку внутри региона
        //    с ИЗВЕСТНЫМ начальным потоком
        TraceRiverFromEntry(EntryPoint, IncomingFlow);
    }
}
```

**Создание глобального графа (один раз при генерации мира):**

```cpp
void CreateGlobalRiverGraph(int32 WorldSeed, FGlobalRiverGraph& OutGraph)
{
    // 1. Находим ВСЕ источники в мире (детерминированно из seed)
    TArray<FVector2D> AllSources = FindAllRiverSources(WorldSeed);

    // 2. Трассируем ВСЕ реки до конца (край мира или слияние)
    TArray<FRiverSpline> AllRivers;
    for (FVector2D Source : AllSources)
    {
        AllRivers.Add(TraceRiver(Source, WorldSeed));
    }

    // 3. Строим граф слияний
    FRiverNetwork FullNetwork;
    FullNetwork.BuildFromSplines(AllRivers);
    FullNetwork.ComputeAccumulatedFlow();  // Топологическая сортировка

    // 4. Упрощаем: оставляем только key points
    OutGraph = SimplifyToKeyPoints(FullNetwork);
    //   - Sources
    //   - Merge points
    //   - Region border crossings
    //   - ~100-1000 узлов на весь мир
}

// Сохраняем в файл мира (вместе с WorldSeed)
WorldSaveFile:
├── WorldSeed: 12345
├── GlobalRiverGraph: [serialized]   // <-- Новое!
└── PlayerModifications: [...]
```

**Решение 2: Drainage Basin Estimation (fallback)**

Если глобальный граф недоступен, оцениваем поток по площади водосбора:

```cpp
float EstimateFlowFromDrainageArea(FVector2D RiverPoint, int32 WorldSeed)
{
    // Площадь водосбора ≈ сколько регионов upstream от этой точки
    // Это можно вычислить локально через анализ градиента высоты

    // 1. Идём против градиента (вверх по течению) пока не дойдём до водораздела
    float DrainageArea = 0.f;
    TSet<FIntPoint> VisitedCells;

    FloodFillUpstream(RiverPoint, WorldSeed, [&](FVector2D Cell) {
        DrainageArea += CellArea;
        VisitedCells.Add(ToGridCell(Cell));
    });

    // 2. Площадь → поток (эмпирическая формула)
    // В реальной гидрологии: Q ≈ k * A^0.8
    const float k = 0.1f;
    return k * FMath::Pow(DrainageArea, 0.8f);
}
```

**Минусы:** менее точно, может не совпадать с upstream вычислениями.

**Решение 3: Incremental River Graph (для бесконечного мира - рекомендуется)**

Граф растёт по мере исследования мира:

```cpp
struct FIncrementalRiverGraph
{
    // Граф "известной" части мира - растёт со временем
    TMap<FIntPoint, FRegionRiverData> KnownRegions;

    struct FRegionRiverData
    {
        // Входящие реки на границу региона
        TArray<FBorderRiverEntry> IncomingRivers;
        // Исходящие реки с границы региона
        TArray<FBorderRiverEntry> OutgoingRivers;
        // Полностью ли вычислен upstream для всех входящих рек
        bool bUpstreamComplete = false;
    };

    struct FBorderRiverEntry
    {
        FVector2D Position;
        float AccumulatedFlow;
        FIntPoint NeighborRegion;  // Откуда/куда течёт
    };
};

// При загрузке нового региона
void FWorldRegion::OnLoad(FIncrementalRiverGraph& Graph)
{
    // 1. Проверяем, есть ли данные о входящих реках
    if (FRegionRiverData* Known = Graph.KnownRegions.Find(RegionID))
    {
        if (Known->bUpstreamComplete)
        {
            // Идеально - используем точные данные
            for (auto& Entry : Known->IncomingRivers)
                TraceRiverFromEntry(Entry.Position, Entry.AccumulatedFlow);
            return;
        }
    }

    // 2. Upstream не полный - используем estimation + помечаем для пересчёта
    TArray<FVector2D> RiverEntries = FindRiverEntriesOnBorder(RegionID);
    for (FVector2D Entry : RiverEntries)
    {
        float EstimatedFlow = EstimateFlowFromDrainageArea(Entry, WorldSeed);
        TraceRiverFromEntry(Entry, EstimatedFlow);

        // Помечаем для уточнения когда upstream загрузится
        PendingFlowRecalculation.Add(Entry);
    }
}

// Когда игрок исследует upstream регион - пересчитываем downstream
void OnUpstreamRegionExplored(FIntPoint UpstreamRegion, FIncrementalRiverGraph& Graph)
{
    // 1. Вычисляем точный flow для исходящих рек
    FRegionRiverData& Data = Graph.KnownRegions.FindOrAdd(UpstreamRegion);
    for (auto& Outgoing : Data.OutgoingRivers)
    {
        Outgoing.AccumulatedFlow = ComputeExactFlow(Outgoing.Position);
    }

    // 2. Propagate downstream (если эти регионы загружены)
    for (auto& Outgoing : Data.OutgoingRivers)
    {
        FIntPoint DownstreamRegion = Outgoing.NeighborRegion;
        if (IsRegionLoaded(DownstreamRegion))
        {
            // Пересчитать ширину рек в downstream регионе
            RecalculateRiverWidths(DownstreamRegion, Outgoing);
        }
        else
        {
            // Сохранить для будущей загрузки
            Graph.KnownRegions.FindOrAdd(DownstreamRegion)
                .IncomingRivers.Add(Outgoing);
        }
    }
}
```

**Визуализация Incremental подхода:**

```
Время T=0: Игрок в регионе D
┌─────┬─────┬─────┐
│  ?  │  ?  │  ?  │   ? = неисследовано
├─────┼─────┼─────┤
│  ?  │ [D] │  ?  │   [D] = игрок здесь
├─────┼─────┼─────┤       Река входит слева → estimation flow=~3
│  ?  │  ?  │  ?  │
└─────┴─────┴─────┘

Время T=1: Игрок исследовал B и C
┌─────┬─────┬─────┐
│  ?  │ [B] │  ?  │   ○ = источник (flow=1)
├─────┼─────┼─────┤
│  ?  │  C  │  D  │   B имеет 2 источника → C получает flow=2
├─────┼─────┼─────┤   C добавляет 1 → D получает flow=3 (ТОЧНО!)
│  ?  │  ?  │  ?  │
└─────┴─────┴─────┘   → Пересчитываем ширину реки в D

Время T=2: Игрок нашёл ещё источник в A
┌─────┬─────┬─────┐
│ [A] │  B  │  ?  │   A имеет источник → B получает +1
├─────┼─────┼─────┤   → Propagate: B=3, C=4, D=5
│  ?  │  C  │  D  │   → Река в D стала шире!
├─────┼─────┼─────┤
│  ?  │  ?  │  ?  │
└─────┴─────┴─────┘
```

**Ключевой принцип: Eventual Consistency**

Река может временно иметь "неправильную" ширину, но:
1. Estimation даёт разумное приближение (~80% точности)
2. По мере исследования upstream - ширина уточняется
3. Визуально незаметно (плавная интерполяция при пересчёте)

```cpp
// Плавное обновление ширины при пересчёте
void UpdateRiverWidth(FRiverSegment& Segment, float NewFlow)
{
    float OldWidth = Segment.CurrentWidth;
    float NewWidth = GetRiverWidth(NewFlow);

    // Интерполируем за 2-3 секунды чтобы не было резкого скачка
    Segment.TargetWidth = NewWidth;
    Segment.WidthLerpAlpha = 0.f;
}

void TickRiverVisuals(float DeltaTime)
{
    for (FRiverSegment& Seg : RiverSegments)
    {
        if (Seg.CurrentWidth != Seg.TargetWidth)
        {
            Seg.WidthLerpAlpha += DeltaTime / 2.f;  // 2 секунды transition
            Seg.CurrentWidth = FMath::Lerp(Seg.CurrentWidth, Seg.TargetWidth,
                                           FMath::Clamp(Seg.WidthLerpAlpha, 0.f, 1.f));
        }
    }
}
```

**Рекомендация для бесконечного мира:**

| Подход | Точность | Использование |
|--------|----------|---------------|
| Incremental Graph | 100% (eventual) | Основной - данные о исследованных регионах |
| Drainage Estimation | ~80% | Fallback для неисследованного upstream |
| Distance-to-Source | ~60% | Emergency fallback (если estimation слишком дорого) |

**Сохранение в SaveFile:**
```
WorldSaveFile:
├── WorldSeed: 12345
├── IncrementalRiverGraph:          // <-- Растёт со временем
│   ├── Region[0,0]: { incoming: [...], outgoing: [...], complete: true }
│   ├── Region[0,1]: { incoming: [...], outgoing: [...], complete: true }
│   └── Region[1,0]: { incoming: [...], outgoing: [...], complete: false }
└── PlayerModifications: [...]
```

### ⚠️ Фундаментальное противоречие: Global vs Infinite

**Критический выбор архитектуры:**

```
┌─────────────────────────────────────────────────────────────────┐
│                    WORLD TYPE DECISION                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  КОНЕЧНЫЙ МИР (Valheim-style диск)?                             │
│       │                                                          │
│       ├── ДА → Global River Graph                               │
│       │        • Генерируем ВСЕ реки один раз при создании      │
│       │        • 100% надёжность, нет surprises                 │
│       │        • ~1-10 MB на весь мир                           │
│       │        • Рекомендуется!                                  │
│       │                                                          │
│       └── НЕТ (бесконечный) → Incremental Graph                 │
│                • Невозможно найти "все источники"               │
│                • Граф растёт по мере исследования               │
│                • Риск "эффекта бабочки" (см. ниже)              │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**Риск Incremental подхода: "Эффект бабочки"**

```
День 1: Игрок строит базу у реки
┌─────────────────────────────────────────────────────────────────┐
│  [Неисследовано]     │     [Неисследовано]    │    [База]      │
│                      │                        │                 │
│       ???            │        ???             │   🏠 ← база    │
│                      │                        │   ═══ река     │
│  Estimated flow: 3   │   Estimated flow: 3    │   width: 10м   │
│                      │                        │                 │
└─────────────────────────────────────────────────────────────────┘

День 100: Игрок исследует далёкий регион, находит ОГРОМНЫЙ бассейн
┌─────────────────────────────────────────────────────────────────┐
│  [ИССЛЕДОВАН]        │     [ИССЛЕДОВАН]       │    [База]      │
│                      │                        │                 │
│  ○ ○ ○ ○ ○ ← 20      │        ○ ○ ○           │   🏠 💀        │
│  источников!         │                        │   ══════════   │
│                      │                        │   width: 40м!  │
│  Actual flow: 25!    │   Propagate → 30       │   ЗАТОПЛЕНО!   │
│                      │                        │                 │
└─────────────────────────────────────────────────────────────────┘

Подвал игрока ушёл под воду 😱
```

**ОБЯЗАТЕЛЬНЫЕ защитные меры для Incremental подхода:**

```cpp
// 1. CLAMP максимальной ширины реки
float GetRiverWidth(float AccumulatedFlow)
{
    const float BaseWidth = 5.f;
    const float MaxWidth = 30.f;  // ← КРИТИЧНО! Ограничиваем максимум

    float Width = BaseWidth * FMath::Sqrt(AccumulatedFlow);
    return FMath::Min(Width, MaxWidth);  // Clamp!
}

// 2. ЗАЩИТА построек игрока от изменения террейна
class FPlayerBuildingProtection
{
    TSet<FIntVector> ProtectedVoxels;  // Воксели под постройками игрока

public:
    void OnPlayerBuildingPlaced(AActor* Building)
    {
        // Запоминаем все воксели под фундаментом
        FBox Bounds = Building->GetComponentsBoundingBox();
        ForEachVoxelInBounds(Bounds, [this](FIntVector Voxel)
        {
            ProtectedVoxels.Add(Voxel);
        });
    }

    bool CanModifyTerrain(FIntVector VoxelCoord) const
    {
        // Запрещаем модификацию под постройками
        return !ProtectedVoxels.Contains(VoxelCoord);
    }
};

// 3. FREEZE ширины реки в зоне игрока
class FRiverWidthFreeze
{
    // Зоны где ширина реки "заморожена" (не меняется от upstream updates)
    TMap<FIntPoint, float> FrozenRiverWidths;  // ChunkCoord → frozen width

public:
    void OnPlayerBaseEstablished(FVector BaseLocation)
    {
        // Замораживаем ширину рек в радиусе 500м от базы
        const float FreezeRadius = 500.f;

        ForEachChunkInRadius(BaseLocation, FreezeRadius, [this](FIntPoint ChunkCoord)
        {
            float CurrentWidth = GetCurrentRiverWidth(ChunkCoord);
            if (CurrentWidth > 0.f)
            {
                FrozenRiverWidths.Add(ChunkCoord, CurrentWidth);
            }
        });
    }

    float GetEffectiveRiverWidth(FIntPoint ChunkCoord, float CalculatedWidth)
    {
        if (float* Frozen = FrozenRiverWidths.Find(ChunkCoord))
        {
            // Река может только СУЖАТЬСЯ, не расширяться
            return FMath::Min(*Frozen, CalculatedWidth);
        }
        return CalculatedWidth;
    }
};

// 4. GRADUAL CHANGE - изменения применяются постепенно
void UpdateRiverWidthSafe(FRiverSegment& Segment, float NewFlow)
{
    float NewWidth = GetRiverWidth(NewFlow);
    float OldWidth = Segment.CurrentWidth;

    // Ограничиваем СКОРОСТЬ изменения (не более 1м в день игрового времени)
    const float MaxChangePerDay = 1.f;
    float MaxChange = MaxChangePerDay * DeltaGameDays;

    if (NewWidth > OldWidth)
    {
        // Расширение - медленно и с уведомлением
        Segment.TargetWidth = FMath::Min(NewWidth, OldWidth + MaxChange);

        if (NewWidth - OldWidth > 5.f)
        {
            // Предупредить игрока!
            NotifyPlayer("Река расширяется! Возможно затопление через N дней.");
        }
    }
    else
    {
        // Сужение - можно быстрее
        Segment.TargetWidth = NewWidth;
    }
}
```

**Визуализация защитных мер:**

```
С защитой:
┌─────────────────────────────────────────────────────────────────┐
│                                                                  │
│  Далёкий регион           База игрока                           │
│  ○ ○ ○ ○ ○ (20 источ.)    🏠                                    │
│       │                    │                                     │
│       ▼                    ▼                                     │
│  flow = 25            FROZEN ZONE                                │
│  width = 40м?         width = MAX(10м, clamp(30м))              │
│       │                    │                                     │
│       ▼                    ▼                                     │
│  CLAMPED!             Река осталась 10м (frozen)                │
│  width = 30м (max)    База в безопасности! ✓                    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**Рекомендуемая конфигурация для бесконечного мира:**

```cpp
struct FInfiniteWorldRiverConfig
{
    // Лимиты ширины
    float MinRiverWidth = 3.f;      // Минимум - ручей
    float MaxRiverWidth = 30.f;     // Максимум - большая река (КРИТИЧНО!)
    float MaxEstimatedFlow = 100.f; // Clamp estimated flow

    // Защита игрока
    float PlayerBaseFreezeRadius = 500.f;  // Радиус заморозки вокруг базы
    float MaxWidthChangePerDay = 1.f;      // Скорость изменения

    // Уведомления
    float WarningThreshold = 5.f;   // Предупреждать если река расширится > 5м
    int32 WarningDaysAhead = 7;     // За сколько дней предупреждать
};
```

**Итоговая рекомендация:**

| Тип мира | Решение | Риски |
|----------|---------|-------|
| Конечный (диск) | **Global Graph** | Нет |
| Бесконечный | Incremental + ALL защиты | Минимальные |
| Бесконечный без защит | ❌ НЕ ДЕЛАТЬ | Сломанный gameplay |

---

### Профиль русла реки

```cpp
float ComputeRiverInfluenceAtPoint(FVector2D Pos, const FRiverNetwork& Network)
{
    // 1. Найти ближайший сегмент (через spatial index - O(1))
    FRiverQueryResult Query = Network.SpatialIndex.FindNearest(Pos);
    if (!Query.IsValid())
        return 0.f;

    float Distance = Query.Distance;
    const FRiverNode& Node = Network.Nodes[Query.NodeIndex];

    // 2. Ширина реки из accumulated flow (уже вычислено при preprocessing)
    float RiverWidth = GetRiverWidth(Node.AccumulatedFlow);

    if (Distance > RiverWidth * 2.f)
        return 0.f;  // Слишком далеко

    // 3. Глубина русла (U-образный профиль)
    float NormDist = Distance / RiverWidth;
    float ValleyDepth = RiverWidth * 0.4f;  // Глубина пропорциональна ширине

    if (NormDist < 1.f)
    {
        // Внутри русла - параболический профиль
        return ValleyDepth * (1.f - NormDist * NormDist);
    }
    else
    {
        // Берега - плавный переход к окружающему terrain
        float FalloffDist = (NormDist - 1.f);
        return ValleyDepth * FMath::Max(0.f, 1.f - FalloffDist);
    }
}
```

**Профиль русла в разрезе:**

```
                    Ширина (зависит от AccumulatedFlow)
                    ◄─────────────────────────►

Terrain level ════╗                             ╔════ Terrain level
                  ║  ╲    Берег (falloff)   ╱  ║
                  ║    ╲                  ╱    ║
                  ║      ╲______________╱      ║
                  ║         Русло реки         ║
                  ║      (параболический)       ║
                  ╚════════════════════════════╝

       │←──────── RiverWidth * 2 ──────────►│
               (зона влияния)
```

---

### ⚠️ КРИТИЧНО: Оптимизация производительности

**Проблема:** Наивная реализация `GetRiverValleyInfluence()` - это CPU-бомба:
- 49 ячеек (7×7) × до 500 точек сплайна × O(n) поиск сегмента
- Вызывается на КАЖДЫЙ height query
- Collision, NavMesh, PCG - тысячи запросов в секунду

**Симптомы:**
- FPS падает при генерации ландшафта
- NavMesh rebuild занимает минуты
- Производительность деградирует нелинейно с радиусом обзора

#### Решение 1: Low-Resolution River Map (рекомендуется)

```cpp
// River influence считаем на грубой сетке, интерполируем
class FRiverInfluenceCache
{
    static constexpr float CellSize = 8.f;  // 8 метров на ячейку
    TMap<FIntPoint, float> Cache;

public:
    float GetInfluence(FVector2D Pos, int32 Seed)
    {
        // Bilinear interpolation из 4 соседних ячеек
        FIntPoint Cell00 = WorldToCell(Pos);
        FIntPoint Cell10 = Cell00 + FIntPoint(1, 0);
        FIntPoint Cell01 = Cell00 + FIntPoint(0, 1);
        FIntPoint Cell11 = Cell00 + FIntPoint(1, 1);

        float V00 = GetOrCompute(Cell00, Seed);
        float V10 = GetOrCompute(Cell10, Seed);
        float V01 = GetOrCompute(Cell01, Seed);
        float V11 = GetOrCompute(Cell11, Seed);

        FVector2D Frac = GetFractional(Pos);
        return FMath::BiLerp(V00, V10, V01, V11, Frac.X, Frac.Y);
    }

private:
    float GetOrCompute(FIntPoint Cell, int32 Seed)
    {
        if (float* Cached = Cache.Find(Cell))
            return *Cached;

        // Только здесь вызываем дорогую функцию
        FVector2D WorldPos = CellToWorld(Cell);
        float Value = ComputeRiverInfluence_Expensive(WorldPos, Seed);
        Cache.Add(Cell, Value);
        return Value;
    }
};
```

**Результат:** ~64× меньше вычислений (8м сетка вместо 1м queries)

#### Решение 2: Tile-Based Precompute

```cpp
// Генерируем river influence для целого тайла сразу
struct FTerrainTile
{
    static constexpr int32 Size = 64;      // 64×64 точек
    static constexpr float Resolution = 2.f; // 2м между точками

    TArray<float> RiverInfluence;  // 64×64 = 4096 float

    void Precompute(FIntPoint TileCoord, int32 Seed)
    {
        RiverInfluence.SetNum(Size * Size);

        for (int32 y = 0; y < Size; y++)
        {
            for (int32 x = 0; x < Size; x++)
            {
                FVector2D WorldPos = TileToWorld(TileCoord, x, y);
                RiverInfluence[y * Size + x] = ComputeRiverInfluence_Expensive(WorldPos, Seed);
            }
        }
    }

    float Sample(FVector2D LocalPos) const
    {
        // Bilinear sample из precomputed данных
        return BilinearSample(RiverInfluence, LocalPos / Resolution);
    }
};
```

#### Решение 3: Двухуровневый Height

**Семантика функций (важно!):**

```cpp
// ✅ ПРАВИЛЬНЫЕ НАЗВАНИЯ (согласно World Assumptions #3):
// Height_Gameplay = terrain solid surface (для AI, physics, placement)
// Height_Render   = visual mesh surface (с river carving)

// ─── BAKING PHASE (при стриминге региона) ───
// Эти функции вызываются ТОЛЬКО при создании текстуры региона:

float BakeHeight_Gameplay(FVector2D Pos, int32 Seed)
{
    // Запекается в HeightTexture_Gameplay
    return PerlinFBM(Pos, Seed) + GetSettlementInfluence(Pos, Seed);
}

float BakeHeight_Render(FVector2D Pos, int32 Seed)
{
    // Запекается в HeightTexture_Render (или отдельный RiverValley channel)
    return BakeHeight_Gameplay(Pos, Seed) - GetRiverValleyInfluence_Cached(Pos, Seed);
}

// ─── RUNTIME (chunk generation, AI queries) ───
// Runtime ВСЕГДА читает из baked texture, НЕ вычисляет noise!

float GetHeight_Gameplay(FVector2D Pos)  // ← Нет Seed! Читаем из текстуры
{
    FVector2D UV = WorldToRegionUV(Pos);
    return CurrentRegion->HeightCache_CPU.Sample(UV);  // O(1) lookup
}

float GetHeight_Render(FVector2D Pos)
{
    return GetHeight_Gameplay(Pos) - CurrentRegion->RiverValleyCache_CPU.Sample(UV);
}
```

**⚠️ ВАЖНО: River water и collision — ОТДЕЛЬНЫЕ системы!**

```
Gameplay height игнорирует river carving, НО:

1. River WATER SURFACE — отдельная плоскость (WaterBodyRiver actor)
   → Игрок может плавать
   → Collision = WaterBody volume, НЕ terrain height

2. River COLLISION — отдельный volume для AI/pathfinding
   → AI учитывает реки как барьер через Spatial Query
   → НЕ через GetHeight_Gameplay()

3. River BRIDGES — строятся НАД river valley
   → Мост имеет свой collision mesh
   → Мост НЕ модифицирует terrain height

Схема:
┌─────────────────────────────────────────────────────────┐
│                                                         │
│   GetHeight_Gameplay() ────────────────── (terrain)     │
│                        ╲                                │
│   GetHeight_Render() ─────────────── (visual with valley)│
│                            ╲                            │
│   WaterSurface ─────────────────────── (water plane)    │
│   RiverCollision ───────────────────── (swimming zone)  │
│                                                         │
│   Мост строится на GetHeight_Gameplay(), НЕ Render!     │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

**Использование:**
```cpp
// - Physics ground check  → GetHeight_Gameplay()
// - AI slope calculation  → GetHeight_Gameplay()
// - PCG tree placement    → GetHeight_Gameplay()
// - Building foundation   → GetHeight_Gameplay()
// - Terrain mesh baking   → GetHeight_Render() (однократно)
// - Can player swim here? → WaterBody::IsInWater(Pos)
// - AI avoids river?      → RiverSpatialQuery::IsBlocked(From, To)
```

#### Решение 4: Spatial Index для сплайнов

```cpp
// Вместо O(n) поиска ближайшего сегмента
class FRiverSplineIndex
{
    static constexpr float GridSize = 100.f;
    TMap<FIntPoint, TArray<int32>> Grid;  // Cell → список индексов сегментов

public:
    void Build(const FRiverSpline& Spline)
    {
        for (int32 i = 0; i < Spline.Points.Num() - 1; i++)
        {
            // Добавить сегмент во все ячейки, которые он пересекает
            FBox2D SegmentBounds = GetSegmentBounds(Spline.Points[i], Spline.Points[i+1]);
            for (FIntPoint Cell : GetCellsInBounds(SegmentBounds))
            {
                Grid.FindOrAdd(Cell).Add(i);
            }
        }
    }

    int32 FindClosestSegment(FVector2D Pos, const FRiverSpline& Spline)
    {
        FIntPoint Cell = WorldToCell(Pos);

        // Проверяем только сегменты в текущей и соседних ячейках
        float MinDist = MAX_FLT;
        int32 ClosestSegment = -1;

        for (int32 dx = -1; dx <= 1; dx++)
        {
            for (int32 dy = -1; dy <= 1; dy++)
            {
                if (TArray<int32>* Segments = Grid.Find(Cell + FIntPoint(dx, dy)))
                {
                    for (int32 Idx : *Segments)
                    {
                        float Dist = PointToSegmentDistance(Pos,
                            Spline.Points[Idx], Spline.Points[Idx+1]);
                        if (Dist < MinDist)
                        {
                            MinDist = Dist;
                            ClosestSegment = Idx;
                        }
                    }
                }
            }
        }

        return ClosestSegment;
    }
};
```

#### Комбинированная стратегия (рекомендация)

```
┌─────────────────────────────────────────────────────────────┐
│                    HEIGHT QUERIES                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Частые (collision, AI, PCG):                               │
│    GetHeight_Gameplay() - terrain surface (БЕЗ river carving)│
│    ~0.1 μs per query                                        │
│                                                             │
│  Редкие (terrain mesh baking):                              │
│    GetHeight_Render() с River Tile Cache                    │
│    ~1 μs per query (после прогрева кэша)                    │
│                                                             │
│  River/Water interactions:                                  │
│    WaterBody::IsInWater() - плавание                       │
│    RiverSpatialQuery::IsBlocked() - AI барьеры             │
│                                                             │
│  River Tile Cache:                                          │
│    - 64×64 тайлы, 2м resolution                            │
│    - Precompute при первом обращении к тайлу               │
│    - LRU eviction для памяти                               │
│    - ~16 KB на тайл (4096 floats)                          │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**Benchmarks (примерные):**

| Подход | Время на query | Queries/sec |
|--------|----------------|-------------|
| Наивный (49 cells × 500 segments) | ~500 μs | ~2,000 |
| С кэшем сплайнов | ~50 μs | ~20,000 |
| + Spatial index | ~10 μs | ~100,000 |
| + Tile cache (8м) | ~0.5 μs | ~2,000,000 |
| GetHeight_Gameplay (без river carving) | ~0.1 μs | ~10,000,000 |

### Ширина реки (downstream = шире)

```cpp
void CalculateRiverWidth(FRiverData& River)
{
    float AccumulatedFlow = 1.f;

    for (i = 0; i < River.Path.Num(); i++)
    {
        float BaseWidth = 5.f + Sqrt(AccumulatedFlow) * 3.f;
        River.Widths[i] = BaseWidth * NoiseVariation;

        AccumulatedFlow += 0.5f;  // Притоки добавляют воду
    }
}
```

### Слияние рек

Когда две реки сближаются:
- Меньшая становится притоком большей
- Ширина главной реки увеличивается на 40%

### Carving русла

```cpp
float GetRiverDepth(FVector2D Pos, int32 Seed)
{
    float DistToRiver = GetDistanceToNearestRiver(Pos, Seed);
    float RiverWidth = GetRiverWidth(...);

    if (DistToRiver > RiverWidth * 2)
        return 0.f;

    float NormDist = DistToRiver / RiverWidth;

    if (NormDist < 1.f)
    {
        // Внутри русла - U-образный профиль
        float ChannelDepth = RiverWidth * 0.3f;
        return ChannelDepth * (1.f - NormDist * NormDist);
    }
    else
    {
        // Берега - плавный подъём
        return SmoothFalloff(...);
    }
}
```

---

> **📋 Конец Phase 2 секции**
>
> Выше описана сложная система гидрологии с accumulated flow, drainage estimation и защитой от "butterfly effect".
> Для v1.0 используйте MVP подход из начала раздела "Реки".

---

## Озёра и водопады

### Типы озёр

**Эндорейное (нет выхода):**
```
       река
         ↓
  ═══════╗
         ║
  ≋≋≋≋≋≋≋╨≋≋≋≋≋≋≋
  ≋     LAKE     ≋
  ≋≋≋≋≋≋≋≋≋≋≋≋≋≋≋
```

**Проточное:**
```
       река
         ↓
  ═══════╗
  ≋≋≋≋≋≋≋╨≋≋≋≋≋≋≋
  ≋     LAKE     ≋
  ≋≋≋≋≋≋≋╥≋≋≋≋≋≋≋
         ╚═══════
         ↓
       река
```

**Горное:**
```
     ╱╲     ╱╲
    ╱ ≋≋≋≋≋≋≋ ╲
   ╱  ≋ LAKE ≋  ╲
        ║
        ╚═══════
```

### Генерация озёр

```cpp
TArray<FLakeData> GenerateLakes(Rivers, Seed)
{
    // 1. Озёра в локальных минимумах (конец реки)
    for (River : Rivers)
    {
        if (River.EndHeight > SeaLevel)
        {
            Lakes.Add(CreateLakeAtMinimum(River.EndPoint));
        }
    }

    // 2. Дополнительные из noise (basin lakes)
    // 3. Горные озёра (высоко в горах)

    return Lakes;
}
```

### Береговая линия

```cpp
TArray<FVector2D> TraceShoreline(FVector2D Center, float WaterLevel, int32 Seed)
{
    // Contour tracing: найти линию где Height == WaterLevel
    // Marching squares algorithm
}
```

### Водопады

**Детекция:**
```cpp
for (i = 0; i < River.Path.Num() - 1; i++)
{
    float HeightDrop = Height(Current) - Height(Next);
    float HorizontalDist = Distance(Current, Next);

    if (HeightDrop > 10m && HorizontalDist < 50m)
    {
        // Водопад!
        Waterfalls.Add({ Current, Next, HeightDrop });
    }
}
```

**Типы водопадов:**
| Тип | Условие |
|-----|---------|
| Plunge | Steepness > 2, Drop > 30m |
| Horsetail | Steepness > 0.5, Drop > 15m |
| Wide | Width > 30m |
| Cascade | Ступенчатый |

**Plunge Pool (бассейн под водопадом):**
```cpp
PoolRadius = RiverWidth * 2 + DropHeight * 0.5;
PoolDepth = DropHeight * 0.3;
```

---

## Бесконечный мир и регионы

> **📋 TL;DR Decision**
>
> | Вопрос | Решение |
> |--------|---------|
> | World type | Бесконечный (см. World Assumptions #1) |
> | Streaming unit | Region ~4x4 km |
> | Inter-region roads | Gateways на границах (deterministic) |
> | Pre-generation | Async при стриминге (height bake + rivers) |
> | LOD | По расстоянию от игрока |

### Проблема
Нельзя сгенерировать бесконечный граф дорог заранее.

### Решение: Иерархические регионы

```
Мир делится на РЕГИОНЫ (~4x4 км)
Каждый регион - самодостаточная "страна"

┌─────────┬─────────┬─────────┐
│ Region  │ Region  │ Region  │
│  0,1    │  1,1    │  2,1    │
│  ⛪══⛪  │ ⛪══════⛪│  ⛪═══⛪ │
├────╫────┼───╫─────┼────╫────┤
│ Region ║│ Region ║│ Region ║│
│  0,0   ║│  1,0   ║│  2,0   ║│
│  ⛪══⛪═╬═⛪══════╬═══⛪═══⛪ │
└─────────┴─────────┴─────────┘
```

### Структура региона

```cpp
struct FWorldRegion
{
    FIntPoint RegionCoord;
    TArray<FVillageData> Villages;
    TArray<FDungeonData> Dungeons;
    TArray<FRoadSegment> InternalRoads;
    TArray<FBorderGateway> BorderGateways;

    static FWorldRegion Generate(FIntPoint Coord, int32 WorldSeed);
};
```

### Border Gateways

**Проблема стыковки дорог:**
```
Region A                    │  Region B
                           │
    Village_A              │              Village_B
        │                  │                  │
        └── A* finds P1 ───┼───? ───── A* finds P2 ──┘
                           │
                    P1 ≠ P2 = дороги НЕ соединяются!

Если позиции gateways определяются ПОСЛЕ A*,
регионы не согласуются друг с другом.
```

**Решение: Gateway Grid (детерминированный)**

Позиции gateways определяются ДО поиска пути, по seed границы:

```cpp
// Gateway Grid - аналог Settlement Grid, но для границ регионов
TArray<FBorderGateway> ComputeBorderGateways(
    FIntPoint RegionA, FIntPoint RegionB, int32 WorldSeed)
{
    // Seed границы - одинаковый для обоих регионов!
    // Важно: не зависит от порядка регионов
    int32 BorderSeed = HashCombine(WorldSeed,
        FMath::Min(RegionA.X, RegionB.X),
        FMath::Max(RegionA.X, RegionB.X),
        FMath::Min(RegionA.Y, RegionB.Y),
        FMath::Max(RegionA.Y, RegionB.Y));

    TArray<FBorderGateway> Gateways;

    // Граница делится на сегменты (~500м), в каждом может быть gateway
    const float SegmentSize = 500.f;
    FVector2D BorderStart, BorderEnd;
    GetSharedBorder(RegionA, RegionB, BorderStart, BorderEnd);

    int32 NumSegments = FMath::CeilToInt((BorderEnd - BorderStart).Size() / SegmentSize);

    for (int32 i = 0; i < NumSegments; i++)
    {
        int32 SegmentSeed = HashCombine(BorderSeed, i);
        FRandomStream Rng(SegmentSeed);

        // Не каждый сегмент имеет gateway (density ~30%)
        if (Rng.FRand() > 0.3f)
            continue;

        // Позиция с jitter внутри сегмента
        float T = (i + Rng.FRand()) / NumSegments;
        FVector2D GatewayPos = FMath::Lerp(BorderStart, BorderEnd, T);

        // Проверка пригодности (не в воде, не на скале)
        if (IsValidGatewayLocation(GatewayPos, WorldSeed))
        {
            Gateways.Add({ GatewayPos, GetRoadType(SegmentSeed) });
        }
    }

    return Gateways;
}
```

**Проблема "Неудачных шлюзов" (Gateway Failures)**

Сценарий: граница проходит по горному хребту или посередине озера — все точки непригодны.

```
Регион A                    │ Регион B
                            │
  ⛪ Village                │      ⛪ Village
     ╲                     │      ╱
      ╲   ← дорога нужна   │     ╱
       ?═══════════════════?═══?
       ▲                   ▲
       │    Горный хребет / Озеро
       │    IsValid = false для ВСЕХ точек!
       │
       └── Дорога обрывается
```

**Решение: Трёхуровневый Gateway Resolution**

```cpp
FBorderGateway ResolveGateway(FVector2D IdealPos, int32 WorldSeed, ERoadImportance Importance)
{
    // === Уровень 1: Точная позиция ===
    if (IsValidGatewayLocation(IdealPos, WorldSeed))
    {
        return { IdealPos, EGatewayType::Normal };
    }

    // === Уровень 2: Gateway Sliding ===
    // Ищем лучшую точку в радиусе вдоль границы
    const float SearchRadius = 200.f;  // ±200м вдоль границы
    const int32 SearchSteps = 20;

    float BestScore = -FLT_MAX;
    FVector2D BestPos = IdealPos;

    for (int32 i = -SearchSteps; i <= SearchSteps; i++)
    {
        float Offset = (float(i) / SearchSteps) * SearchRadius;
        FVector2D TestPos = IdealPos + BorderTangent * Offset;

        float Score = EvaluateGatewayLocation(TestPos, WorldSeed);
        // Score учитывает: высоту, уклон, расстояние от воды, близость к ideal

        if (Score > BestScore && IsValidGatewayLocation(TestPos, WorldSeed))
        {
            BestScore = Score;
            BestPos = TestPos;
        }
    }

    if (BestScore > 0.f)
    {
        return { BestPos, EGatewayType::Shifted };
    }

    // === Уровень 3: Terrain Modification (только для важных дорог) ===
    if (Importance >= ERoadImportance::MainRoad)
    {
        // Анализируем почему gateway не проходит
        ETerrainBlocker Blocker = AnalyzeBlocker(IdealPos, WorldSeed);

        switch (Blocker)
        {
        case ETerrainBlocker::Mountain:
            // Вариант A: Перевал (flatten terrain)
            if (CanCreatePass(IdealPos, WorldSeed))
            {
                TerrainModifications.Add(FTerrainMod::CreatePass(IdealPos, PassWidth));
                return { IdealPos, EGatewayType::MountainPass };
            }
            // Вариант B: Туннель
            return { IdealPos, EGatewayType::Tunnel };

        case ETerrainBlocker::Water:
            // Мост
            return { IdealPos, EGatewayType::Bridge };

        case ETerrainBlocker::Cliff:
            // Серпантин или лифт
            return { IdealPos, EGatewayType::Switchback };
        }
    }

    // === Fallback: Дорога не будет построена ===
    // Для Minor Roads это допустимо
    return { IdealPos, EGatewayType::Failed };
}
```

**Типы шлюзов и их обработка:**

```cpp
enum class EGatewayType : uint8
{
    Normal,       // Обычный проход - просто дорога
    Shifted,      // Сдвинут от идеальной позиции
    MountainPass, // Требует flatten terrain в Voxel
    Tunnel,       // Спавн префаба туннеля
    Bridge,       // Спавн префаба моста
    Switchback,   // Серпантин на крутом склоне
    Failed        // Дорога не будет построена
};

// При генерации чанка - обработка специальных gateway
void ProcessGateway(const FBorderGateway& Gateway)
{
    switch (Gateway.Type)
    {
    case EGatewayType::MountainPass:
        // Модификация вокселей: flatten в радиусе
        VoxelWorld->ApplyFlatten(Gateway.Position, PassRadius, PassHeight);
        break;

    case EGatewayType::Tunnel:
        // Карвинг туннеля через гору
        VoxelWorld->ApplyTunnel(Gateway.Position, TunnelDirection, TunnelLength);
        // Спавн декора (входы, освещение)
        SpawnTunnelPrefab(Gateway.Position, TunnelDirection);
        break;

    case EGatewayType::Bridge:
        // Спавн моста над водой/ущельем
        FBridgeParams Params = CalculateBridgeParams(Gateway.Position);
        SpawnBridgePrefab(Gateway.Position, Params);
        break;

    case EGatewayType::Switchback:
        // Генерация серпантина
        TArray<FVector2D> SwitchbackPath = GenerateSwitchback(Gateway.Position);
        Roads.Add(SwitchbackPath);
        break;
    }
}
```

**Визуализация решений:**

```
ПРОБЛЕМА: Горный хребет на границе
┌────────────────────┬────────────────────┐
│  Region A          │          Region B  │
│                   ╱│╲                   │
│    ⛪            ╱ │ ╲            ⛪    │
│      ╲         ╱  │  ╲          ╱      │
│       ╲  ▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲  ╱       │  ▲ = гора
│        ╲    BLOCKED!!!      ╱         │
└─────────╲──────────┴───────╱──────────┘

РЕШЕНИЕ 1: Gateway Sliding (сдвиг к перевалу)
┌────────────────────┬────────────────────┐
│  Region A          │          Region B  │
│                   ╱│╲                   │
│    ⛪            ╱ │ ╲            ⛪    │
│      ╲         ╱  │  ╲          ╱      │
│       ╲  ▲▲▲▲   G ═══ G   ▲▲▲▲╱       │  G = shifted gateway
│        ╲     ╲   ╱│╲   ╱     ╱         │
└─────────╲─────╲─╱─┴─╲─╱─────╱──────────┘
              естественный перевал

РЕШЕНИЕ 2: Tunnel (для Main Road)
┌────────────────────┬────────────────────┐
│  Region A          │          Region B  │
│                   ╱│╲                   │
│    ⛪            ╱ │ ╲            ⛪    │
│      ╲         ╱  │  ╲          ╱      │
│       ╲  ▲▲▲▲[====G====]▲▲▲▲ ╱       │  [===] = туннель
│        ╲       voxel carve  ╱         │
└─────────╲──────────┴───────╱──────────┘

РЕШЕНИЕ 3: Bridge (над водой)
┌────────────────────┬────────────────────┐
│  Region A          │          Region B  │
│                    │                    │
│    ⛪              │              ⛪    │
│      ╲   ≈≈≈≈≈≈≈≈≈│≈≈≈≈≈≈≈≈≈   ╱      │  ≈ = вода
│       ╲  ≈≈≈[═══G═══]≈≈≈     ╱       │  [===] = мост
│        ╲   ≈≈≈≈≈≈≈│≈≈≈≈≈≈   ╱         │
└─────────╲─────────┴────────╱──────────┘
```

**Приоритеты по важности дороги:**

| Road Importance | Gateway Sliding | Terrain Mod | Fallback |
|-----------------|-----------------|-------------|----------|
| MainRoad | ✅ | ✅ Tunnel/Bridge | Должна быть построена |
| SecondaryRoad | ✅ | ✅ Bridge only | Может быть Failed |
| MinorRoad | ✅ | ❌ | Допустимо Failed |
| Trail | ❌ | ❌ | Пропускаем |

**Детерминизм при Terrain Modification:**

```cpp
// Terrain mods тоже детерминированы!
FTerrainMod CreateTunnelMod(FVector2D GatewayPos, int32 WorldSeed)
{
    // Направление туннеля определяется градиентом высоты
    FVector2D GradientDir = GetHeightGradient(GatewayPos, WorldSeed);

    // Длина туннеля = пока не выйдем из горы
    float TunnelLength = FindTunnelExitDistance(GatewayPos, GradientDir, WorldSeed);

    // Высота туннеля = детерминирована
    float TunnelHeight = GetHeight(GatewayPos, WorldSeed) - TunnelDepth;

    return FTerrainMod{
        .Type = ETerrain ModType::Tunnel,
        .Position = GatewayPos,
        .Direction = GradientDir,
        .Length = TunnelLength,
        .Height = TunnelHeight
    };
}
```

**Порядок генерации дорог в регионе:**

```cpp
void FWorldRegion::GenerateRoads(int32 WorldSeed)
{
    // 1. СНАЧАЛА: Получить ВСЕ gateways на границах (детерминировано!)
    TArray<FBorderGateway> AllGateways;
    for (EBorderSide Side : {North, South, East, West})
    {
        FIntPoint NeighborCoord = GetNeighborCoord(RegionCoord, Side);
        TArray<FBorderGateway> BorderGateways =
            ComputeBorderGateways(RegionCoord, NeighborCoord, WorldSeed);

        AllGateways.Append(BorderGateways);
    }

    // 2. ПОТОМ: A* от каждого POI к ближайшим gateways
    for (FVillageData& Village : Villages)
    {
        // Найти ближайшие gateways для этой деревни
        TArray<FBorderGateway*> NearestGateways =
            FindNearestGateways(Village.Position, AllGateways, MaxCount=3);

        for (FBorderGateway* Gateway : NearestGateways)
        {
            // A* теперь ищет путь к ФИКСИРОВАННОЙ точке
            TArray<FVector2D> Path = AStarPath(Village.Position, Gateway->Position);
            InternalRoads.Add(Path);

            Gateway->ConnectedPOI = &Village;
        }
    }

    // 3. MST между внутренними POI (как раньше)
    BuildInternalRoadNetwork();
}
```

**Визуализация:**

```
┌─────────────────┬─────────────────┐
│  Region (0,1)   │  Region (1,1)   │
│                 │                 │
│    ⛪ Village   G2      G1 ⛪      │
│      ╲         ╱│╲      │         │
│       ╲       ╱ │ ╲     │         │
│        ╲     ╱  │  ╲    │         │
├─────────G3─────G4───────G5────────┤
│        ╱       │╲       ╲         │
│       ╱        │ ╲       ╲        │
│      ╱         │  ╲       ╲       │
│    ⛪          │   G6      ⛪      │
│  Region (0,0)  │  Region (1,0)    │
└─────────────────┴─────────────────┘

G1-G6 = Border Gateways (позиции фиксированы по seed границы)
Дороги внутри региона идут К этим фиксированным точкам
```

**Гарантии:**

| Свойство | Как обеспечивается |
|----------|-------------------|
| Одинаковые gateways для обоих регионов | BorderSeed не зависит от порядка регионов |
| Детерминизм | Gateway Grid = f(border_coords, world_seed) |
| Соединяемость | A* внутри региона ведёт К gateway, не ОТ него |
| Параллельная генерация | Регионы независимы после определения gateways |

**Соединение регионов (теперь тривиально):**

```cpp
void ConnectAdjacentRegions(RegionA, RegionB)
{
    // Gateways УЖЕ одинаковые для обоих регионов!
    // Просто проверяем, что оба региона построили дороги к gateway

    TArray<FBorderGateway> SharedGateways =
        ComputeBorderGateways(RegionA.Coord, RegionB.Coord, WorldSeed);

    for (FBorderGateway& Gateway : SharedGateways)
    {
        // Оба региона уже подключены к этой точке
        // Дорога автоматически непрерывна
        if (Gateway.ConnectedFromA && Gateway.ConnectedFromB)
        {
            // Дорога через границу существует!
        }
    }
}
```

### Streaming регионов

```cpp
void UpdateAroundPlayer(FVector PlayerPos, int32 WorldSeed)
{
    FIntPoint PlayerRegion = WorldPosToRegion(PlayerPos);

    // Загрузить регионы в радиусе 2
    for (dx = -2 to 2, dy = -2 to 2)
    {
        FIntPoint Coord = PlayerRegion + (dx, dy);

        if (!LoadedRegions.Contains(Coord))
        {
            Region = FWorldRegion::Generate(Coord, WorldSeed);
            LoadedRegions.Add(Coord, Region);
            ConnectToNeighbors(Coord);
        }
    }

    // Выгрузить далёкие
    UnloadDistantRegions(PlayerRegion, Radius=3);
}
```

### Реки в бесконечном мире

**Реки не зависят от регионов** - они вычисляются как чистые функции:

```cpp
// River Grid работает для ЛЮБОЙ координаты в бесконечном мире
// Не нужно заранее знать границы мира или регионов

float GetRiverInfluenceAnywhere(FVector2D Pos, int32 WorldSeed)
{
    // Просто вызываем - работает для любых координат
    return GetRiverValleyInfluence(Pos, WorldSeed);
}
```

**Кэширование для производительности:**
```cpp
// Кэш вычисленных сплайнов (performance only, не влияет на детерминизм)
TMap<FIntPoint, FRiverSpline> RiverSplineCache;

FRiverSpline GetOrComputeRiverSpline(FIntPoint Cell, int32 CellSeed, int32 WorldSeed)
{
    if (FRiverSpline* Cached = RiverSplineCache.Find(Cell))
    {
        return *Cached;
    }

    FRiverSpline River = ComputeRiverSpline(Cell, CellSeed, WorldSeed);
    RiverSplineCache.Add(Cell, River);
    return River;
}

// Очистка кэша при удалении от игрока
void CleanupDistantRiverCache(FVector2D PlayerPos, float MaxDistance);
```

**Сравнение подходов:**
| Параметр | Предварительная генерация | River Grid (чистая функция) |
|----------|---------------------------|----------------------------|
| Бесконечный мир | ❌ Нужно определить границы | ✅ Работает для любых координат |
| Детерминизм | ⚠️ Зависит от порядка генерации | ✅ Гарантирован математически |
| Память | ⚠️ O(мир) - хранение всех рек | ✅ O(видимость) - только кэш |
| Streaming | ⚠️ Нужна синхронизация | ✅ Независимые вычисления |

---

## PCG для растительности

### Двухслойная система

```
Voxel Graph: Noise → Terrain GEOMETRY (форма)
PCG:         Terrain → Object PLACEMENT (деревья, трава)
```

---

### ⚠️ КРИТИЧНО: Синхронизация PCG и Voxel Terrain

**Проблема 1: Race Condition**

```
Timeline:
  T0: Chunk streaming начинается
  T1: PCG запрашивает surface для спавна деревьев
  T2: Voxel mesh ещё генерируется (нет коллизии!)
  T3: PCG получает невалидные данные или крашится

PCG Framework ожидает готовую геометрию/коллизию,
а Voxel Plugin генерирует её асинхронно.
```

**Решение: Chunk State Machine**

```cpp
enum class EChunkState : uint8
{
    Unloaded,
    VoxelGenerating,    // Воксели генерируются
    VoxelReady,         // Меш готов, коллизия есть
    PCGGenerating,      // PCG спавнит объекты
    Complete            // Всё готово
};

void FVoxelChunk::OnVoxelMeshReady()
{
    State = EChunkState::VoxelReady;

    // ТОЛЬКО ТЕПЕРЬ запускаем PCG
    PCGComponent->Generate();
}

void FVoxelChunk::OnPCGComplete()
{
    State = EChunkState::Complete;

    // Регистрируем все объекты для anchoring
    RegisterAnchoredObjects(PCGComponent->GetSpawnedActors());
}
```

---

**Проблема 2: Подкопанные объекты**

```
БЫЛО:
  Игрок копает землю под деревом
  → Воксели удаляются
  → Дерево (StaticMeshInstance) висит в воздухе
  → Выглядит нереалистично

НАДО (как в Valheim):
  Игрок копает землю под деревом
  → Воксели удаляются
  → Система обнаруживает потерю опоры
  → Дерево конвертируется в Physics Actor
  → Дерево падает под действием гравитации
```

> **📋 TL;DR Decision: Anchoring System**
>
> | Вопрос | Решение |
> |--------|---------|
> | Подход | Reverse Lookup + Spatial Hash |
> | Memory | O(N objects), НЕ O(V voxels) |
> | Check trigger | Dirty flag + batched (100ms interval) |
> | Support calculation | Probabilistic для деревьев, full для зданий |
> | Physics throttling | Max 5 collapses/frame, queue with "shake" |

**Решение: Anchoring System**

```cpp
// Каждый процедурный объект "привязан" к опорным вокселям
struct FAnchoredObject
{
    TWeakObjectPtr<AActor> Actor;
    TWeakObjectPtr<UInstancedStaticMeshComponent> ISMComponent;
    int32 InstanceIndex;  // Для ISM

    FVector WorldPosition;
    TArray<FIntVector> SupportVoxels;  // Воксели, на которых стоит объект
    float MinSupportRequired;          // Минимум % опоры для устойчивости

    EAnchorType Type;  // Tree, Rock, Building, etc.
};

class FAnchoringSystem
{
    // Карта: VoxelCoord → список объектов, которые на нём стоят
    TMap<FIntVector, TArray<FAnchoredObject*>> VoxelToObjects;

    // Карта: Object → его данные привязки
    TMap<uint32, FAnchoredObject> ObjectAnchors;

public:
    // Вызывается PCG после спавна
    void RegisterObject(AActor* Actor, FVector Position)
    {
        FAnchoredObject Anchor;
        Anchor.Actor = Actor;
        Anchor.WorldPosition = Position;

        // Найти воксели под объектом (footprint)
        Anchor.SupportVoxels = FindSupportVoxels(Position, GetFootprintRadius(Actor));

        // Зарегистрировать в обратном маппинге
        for (FIntVector Voxel : Anchor.SupportVoxels)
        {
            VoxelToObjects.FindOrAdd(Voxel).Add(&Anchor);
        }

        ObjectAnchors.Add(GetObjectId(Actor), Anchor);
    }

    // Вызывается при удалении вокселя (копание, взрыв)
    void OnVoxelRemoved(FIntVector VoxelCoord)
    {
        if (TArray<FAnchoredObject*>* Objects = VoxelToObjects.Find(VoxelCoord))
        {
            for (FAnchoredObject* Anchor : *Objects)
            {
                // Убрать этот воксель из опоры
                Anchor->SupportVoxels.Remove(VoxelCoord);

                // Проверить, достаточно ли осталось опоры
                float SupportRatio = float(Anchor->SupportVoxels.Num()) /
                                     float(Anchor->OriginalSupportCount);

                if (SupportRatio < Anchor->MinSupportRequired)
                {
                    // Объект теряет опору!
                    TriggerPhysicsCollapse(Anchor);
                }
            }
        }
    }
};
```

**⚠️ Проблема производительности и памяти**

Наивный подход `TMap<FIntVector, ...>` на каждый воксель — тяжёлый:
- Дом 10×10м при шаге 0.5м = ~400 вокселей опоры
- 400 записей в Map на один дом
- 1000 домов = 400,000 записей в Map

**Оптимизация 1: Block-Based Anchoring**

Храним анкоры поблочно (например, 4×4×4 вокселя = 1 блок):

```cpp
class FAnchoringSystem_Optimized
{
    // Блок = 4×4×4 вокселя (64 вокселя в одном entry)
    static constexpr int32 BlockSize = 4;

    // Карта: BlockCoord → объекты, имеющие опору в этом блоке
    TMap<FIntVector, TArray<FAnchoredObject*>> BlockToObjects;

    FIntVector ToBlockCoord(FIntVector VoxelCoord) const
    {
        return FIntVector(
            FMath::FloorToInt(VoxelCoord.X / (float)BlockSize),
            FMath::FloorToInt(VoxelCoord.Y / (float)BlockSize),
            FMath::FloorToInt(VoxelCoord.Z / (float)BlockSize)
        );
    }

    void OnVoxelRemoved(FIntVector VoxelCoord)
    {
        FIntVector BlockCoord = ToBlockCoord(VoxelCoord);

        if (TArray<FAnchoredObject*>* Objects = BlockToObjects.Find(BlockCoord))
        {
            for (FAnchoredObject* Anchor : *Objects)
            {
                // Проверяем, затронут ли объект (точная проверка)
                if (Anchor->SupportVoxels.Contains(VoxelCoord))
                {
                    Anchor->SupportVoxels.Remove(VoxelCoord);
                    CheckSupport(Anchor);
                }
            }
        }
    }
};

// Экономия памяти:
// Дом 10×10м = 400 вокселей → ~25 блоков (вместо 400 записей)
// 1000 домов = 25,000 записей (вместо 400,000)
// Экономия: ~16× меньше памяти
```

**Оптимизация 2: Reverse Lookup (рекомендуется)**

Объект хранит Bounds опоры. При копании — SphereOverlap вместо Map lookup:

```cpp
struct FAnchoredObject_Optimized
{
    TWeakObjectPtr<AActor> Actor;
    FBox SupportBounds;           // AABB опорной зоны (вместо TArray<FIntVector>)
    int32 OriginalSupportVolume;  // Объём в вокселях при регистрации
    float MinSupportRequired;
    EAnchorType Type;
};

class FAnchoringSystem_ReverseLookup
{
    // Spatial hash для быстрого поиска объектов по позиции
    TSpatialHash<FAnchoredObject_Optimized*> ObjectSpatialHash;

    // Все анкорированные объекты (flat array, компактно)
    TArray<FAnchoredObject_Optimized> AllAnchors;

public:
    void RegisterObject(AActor* Actor, FVector Position)
    {
        FAnchoredObject_Optimized Anchor;
        Anchor.Actor = Actor;
        Anchor.SupportBounds = CalculateSupportBounds(Actor, Position);
        Anchor.OriginalSupportVolume = CalculateSupportVolume(Anchor.SupportBounds);

        AllAnchors.Add(Anchor);
        ObjectSpatialHash.Insert(&AllAnchors.Last(), Anchor.SupportBounds);
    }

    void OnVoxelRemoved(FIntVector VoxelCoord)
    {
        FVector WorldPos = VoxelToWorld(VoxelCoord);

        // Находим все объекты, чьи Bounds содержат этот воксель
        TArray<FAnchoredObject_Optimized*> AffectedObjects;
        ObjectSpatialHash.Query(FBox(WorldPos, WorldPos + FVector(VoxelSize)),
                                AffectedObjects);

        for (FAnchoredObject_Optimized* Anchor : AffectedObjects)
        {
            // Пересчитываем текущий объём опоры
            int32 CurrentVolume = CalculateCurrentSupportVolume(Anchor->SupportBounds);
            float SupportRatio = float(CurrentVolume) / float(Anchor->OriginalSupportVolume);

            if (SupportRatio < Anchor->MinSupportRequired)
            {
                TriggerPhysicsCollapse(Anchor);
            }
        }
    }

private:
    // Считаем сколько вокселей в Bounds ещё solid
    int32 CalculateCurrentSupportVolume(const FBox& Bounds)
    {
        int32 Volume = 0;
        FIntVector Min = WorldToVoxel(Bounds.Min);
        FIntVector Max = WorldToVoxel(Bounds.Max);

        for (int32 Z = Min.Z; Z <= Max.Z; Z++)
        for (int32 Y = Min.Y; Y <= Max.Y; Y++)
        for (int32 X = Min.X; X <= Max.X; X++)
        {
            if (IsVoxelSolid(FIntVector(X, Y, Z)))
                Volume++;
        }
        return Volume;
    }
};
```

**Сравнение подходов:**

```
Наивный (TMap повоксельно):
┌─────────────────────────────────────────────────────────┐
│ Memory: O(V) где V = общее число опорных вокселей       │
│ OnVoxelRemoved: O(1) lookup + O(K) объектов             │
│ Проблема: 400K+ записей в Map для 1000 домов            │
└─────────────────────────────────────────────────────────┘

Block-Based:
┌─────────────────────────────────────────────────────────┐
│ Memory: O(V/64) — в 64 раза меньше записей              │
│ OnVoxelRemoved: O(1) lookup + O(K) объектов + filter    │
│ Компромисс: экономия памяти, немного больше CPU         │
└─────────────────────────────────────────────────────────┘

Reverse Lookup (рекомендуется):
┌─────────────────────────────────────────────────────────┐
│ Memory: O(N) где N = число объектов (не вокселей!)      │
│ OnVoxelRemoved: O(log N) spatial query + O(K) × volume  │
│ Компромисс: минимум памяти, CPU при копании             │
└─────────────────────────────────────────────────────────┘
```

**Выбор подхода:**

| Сценарий | Рекомендация |
|----------|--------------|
| Много объектов, редкое копание | Reverse Lookup |
| Мало объектов, частое копание | Block-Based |
| Realtime mining (Minecraft-style) | Block-Based + кэш |
| Эпизодическое копание (Valheim) | Reverse Lookup |

**Дополнительная оптимизация: Dirty Flag + Batching**

```cpp
class FAnchoringSystem_Batched
{
    TSet<FAnchoredObject_Optimized*> DirtyObjects;
    float LastCheckTime = 0.f;
    const float CheckInterval = 0.1f;  // Проверка раз в 100мс

public:
    void OnVoxelRemoved(FIntVector VoxelCoord)
    {
        // Не проверяем сразу — помечаем как dirty
        TArray<FAnchoredObject_Optimized*> AffectedObjects;
        ObjectSpatialHash.Query(VoxelCoord, AffectedObjects);

        for (auto* Obj : AffectedObjects)
            DirtyObjects.Add(Obj);
    }

    void Tick(float CurrentTime)
    {
        if (CurrentTime - LastCheckTime < CheckInterval)
            return;

        // Batch-проверка всех dirty объектов
        for (FAnchoredObject_Optimized* Anchor : DirtyObjects)
        {
            int32 CurrentVolume = CalculateCurrentSupportVolume(Anchor->SupportBounds);
            float SupportRatio = float(CurrentVolume) / float(Anchor->OriginalSupportVolume);

            if (SupportRatio < Anchor->MinSupportRequired)
                TriggerPhysicsCollapse(Anchor);
        }

        DirtyObjects.Empty();
        LastCheckTime = CurrentTime;
    }
};

// Преимущество:
// Игрок копает 50 вокселей подряд → 1 проверка вместо 50
// Особенно важно для взрывов (много вокселей за кадр)
```

**⚠️ Дополнительные оптимизации (для explosions/mass digging):**

```cpp
// Оптимизация 1: Кэш support volume
struct FAnchoredObject_Cached
{
    // ... базовые поля ...

    // Кэш для избежания пересчёта
    int32 CachedSupportVolume = -1;  // -1 = dirty
    float LastValidCheckTime = 0.f;
    bool bPendingRecheck = false;

    void InvalidateCache() { CachedSupportVolume = -1; bPendingRecheck = true; }

    int32 GetSupportVolume(float CurrentTime)
    {
        // Если кэш валиден и проверяли недавно - используем кэш
        if (CachedSupportVolume >= 0 && (CurrentTime - LastValidCheckTime) < 0.5f)
            return CachedSupportVolume;

        // Иначе пересчитываем
        CachedSupportVolume = CalculateCurrentSupportVolume(SupportBounds);
        LastValidCheckTime = CurrentTime;
        return CachedSupportVolume;
    }
};

// Оптимизация 2: Probabilistic check (для деревьев/простых объектов)
// Деревьям достаточно 2-3 тестовых точек, не нужен full volume scan
int32 CalculateSupportVolume_Probabilistic(const FAnchoredObject& Obj)
{
    if (Obj.Type == EAnchorType::Tree || Obj.Type == EAnchorType::Rock)
    {
        // Для деревьев: проверяем только корневую зону (2-3 точки)
        const int32 NumSamples = 3;
        int32 SolidCount = 0;

        FVector Base = Obj.WorldPosition - FVector(0, 0, 10);  // Чуть ниже основания
        for (int32 i = 0; i < NumSamples; i++)
        {
            // Случайные точки в радиусе 50см от центра
            FVector SamplePos = Base + FVector(
                FMath::RandRange(-50.f, 50.f),
                FMath::RandRange(-50.f, 50.f),
                0
            );

            if (IsVoxelSolid(WorldToVoxel(SamplePos)))
                SolidCount++;
        }

        // Если 0 из 3 solid - падает. 1+ из 3 - стоит.
        return (SolidCount > 0) ? Obj.OriginalSupportVolume : 0;
    }

    // Для зданий - full volume scan (точность важнее)
    return CalculateCurrentSupportVolume(Obj.SupportBounds);
}

// Оптимизация 3: Incremental volume update (для частых изменений)
void OnVoxelRemoved_Incremental(FIntVector VoxelCoord)
{
    for (FAnchoredObject_Cached* Obj : GetObjectsInBounds(VoxelCoord))
    {
        // Вместо полного пересчёта - декрементим кэшированный объём
        if (Obj->CachedSupportVolume > 0)
        {
            Obj->CachedSupportVolume--;

            // Быстрая проверка без полного rescan
            float QuickRatio = float(Obj->CachedSupportVolume) / float(Obj->OriginalSupportVolume);
            if (QuickRatio < Obj->MinSupportRequired)
            {
                // Только тогда делаем full recheck для подтверждения
                Obj->InvalidateCache();
            }
        }
    }
}
```

**Выбор стратегии по типу объекта:**

| Тип | CalculateSupportVolume | Почему |
|-----|------------------------|--------|
| Tree | Probabilistic (3 samples) | Достаточно проверить корень |
| Rock | Probabilistic (3 samples) | Простая форма |
| Bush | Probabilistic (1 sample) | Мелкий объект |
| Small Building | Full volume scan | Сложная форма, критично |
| **Large Building** | **Segmented** | Footprint > threshold |
| **Bridge** | **Segmented** | Длинный объект |
| **Wall** | **Segmented** | Линейный объект |

**⚠️ EDGE CASE: Очень большие объекты**

```
Проблема:
  Мост 100м длиной, footprint = 100×10×2 = 2000 вокселей
  При взрыве под мостом → CalculateCurrentSupportVolume(2000 voxels) → SPIKE!

  Большое здание 50×50м, footprint = 50×50×3 = 7500 вокселей
  Несколько зданий + взрыв = массовый lag
```

```cpp
// Правило: Для объектов с footprint > MAX_FULL_SCAN_VOXELS
const int32 MAX_FULL_SCAN_VOXELS = 500;  // ~8×8×8 метров

int32 CalculateSupportVolume_Smart(const FAnchoredObject& Obj)
{
    int32 FootprintVoxels = GetFootprintVoxelCount(Obj.SupportBounds);

    // Маленькие объекты — full scan (точно)
    if (FootprintVoxels <= MAX_FULL_SCAN_VOXELS)
    {
        return CalculateCurrentSupportVolume(Obj.SupportBounds);
    }

    // Большие объекты — сегментированный подход
    return CalculateSupportVolume_Segmented(Obj);
}

// Вариант 1: Sampling для больших объектов
int32 CalculateSupportVolume_Sampled(const FAnchoredObject& Obj)
{
    const int32 NumSamples = 20;  // Фиксированное число сэмплов
    int32 SolidCount = 0;

    for (int32 i = 0; i < NumSamples; i++)
    {
        // Равномерно распределённые точки по footprint
        FVector SamplePos = GetStratifiedSamplePoint(Obj.SupportBounds, i, NumSamples);

        if (IsVoxelSolid(WorldToVoxel(SamplePos)))
            SolidCount++;
    }

    // Экстраполируем на весь объём
    float SampleRatio = float(SolidCount) / float(NumSamples);
    return FMath::RoundToInt(SampleRatio * Obj.OriginalSupportVolume);
}

// Вариант 2: Logical Segments для мостов/стен
int32 CalculateSupportVolume_Segmented(const FAnchoredObject& Obj)
{
    // Разбиваем на логические сегменты (каждый < MAX_FULL_SCAN_VOXELS)
    TArray<FBox> Segments = SplitIntoSegments(Obj.SupportBounds, MAX_FULL_SCAN_VOXELS);

    int32 TotalSupport = 0;
    int32 FailedSegments = 0;

    for (const FBox& Segment : Segments)
    {
        int32 SegmentVolume = CalculateCurrentSupportVolume(Segment);
        int32 SegmentOriginal = GetOriginalVolumeForSegment(Segment);

        float SegmentRatio = float(SegmentVolume) / float(SegmentOriginal);

        if (SegmentRatio < 0.3f)  // Сегмент потерял опору
            FailedSegments++;

        TotalSupport += SegmentVolume;
    }

    // Мост рушится если ≥2 смежных сегмента без опоры
    // (центральная секция может висеть на краях)
    if (Obj.Type == EAnchorType::Bridge && FailedSegments >= 2)
    {
        // Проверить смежность failed сегментов
        if (HasAdjacentFailedSegments(Segments, FailedSegments))
            return 0;  // Рушится
    }

    return TotalSupport;
}
```

**Визуализация сегментного подхода для моста:**

```
Мост 100м, разбит на 10 сегментов по 10м:

[Seg0][Seg1][Seg2][Seg3][Seg4][Seg5][Seg6][Seg7][Seg8][Seg9]
  ██    ██    ██    ░░    ░░    ░░    ██    ██    ██    ██
                    ↑↑    ↑↑    ↑↑
                  Взрыв убрал опору

██ = опора есть (support > 30%)
░░ = опора потеряна (support < 30%)

Правило для моста:
  - 3 смежных сегмента без опоры → центр рушится
  - Края могут оставаться (cantilever эффект ограничен)
```

**Сводка по размеру объекта:**

| Footprint | Стратегия | Сложность | Точность |
|-----------|-----------|-----------|----------|
| < 500 voxels | Full scan | O(V) | 100% |
| 500-5000 voxels | Sampled (20 points) | O(20) | ~95% |
| > 5000 voxels | Segmented | O(V/segment × N) | 100% per segment |

---

**Проблема 3: Instance → Physics Actor**

PCG обычно использует `UInstancedStaticMeshComponent` (ISM) для производительности.
Но ISM не поддерживает физику per-instance.

**Решение: Конвертация при потере опоры**

```cpp
void FAnchoringSystem::TriggerPhysicsCollapse(FAnchoredObject* Anchor)
{
    if (Anchor->ISMComponent.IsValid())
    {
        // 1. Получить transform инстанса
        FTransform InstanceTransform;
        Anchor->ISMComponent->GetInstanceTransform(Anchor->InstanceIndex, InstanceTransform);

        // 2. Удалить из ISM
        Anchor->ISMComponent->RemoveInstance(Anchor->InstanceIndex);

        // 3. Спавнить физический актор
        AActor* PhysicsActor = SpawnPhysicsVersion(
            Anchor->ISMComponent->GetStaticMesh(),
            InstanceTransform
        );

        // 4. Применить начальный импульс (небольшой толчок)
        if (UPrimitiveComponent* Prim = PhysicsActor->FindComponentByClass<UPrimitiveComponent>())
        {
            Prim->SetSimulatePhysics(true);
            Prim->AddImpulse(FVector(FMath::RandRange(-100, 100),
                                      FMath::RandRange(-100, 100),
                                      -50));
        }

        // 5. Обновить anchor
        Anchor->Actor = PhysicsActor;
        Anchor->ISMComponent = nullptr;
    }
    else if (Anchor->Actor.IsValid())
    {
        // Уже отдельный актор - просто включить физику
        if (UPrimitiveComponent* Prim = Anchor->Actor->FindComponentByClass<UPrimitiveComponent>())
        {
            Prim->SetSimulatePhysics(true);
        }
    }

    // 6. Разрегистрировать из anchoring (объект теперь свободен)
    UnregisterObject(Anchor);
}
```

**⚠️ КРИТИЧНО: Безопасность физики падающих объектов**

Проблемы без защиты:
```
1. Падающее дерево может ВЫТОЛКНУТЬ игрока под текстуры
2. Мгновенная коллизия = мгновенная смерть (если есть урон от физики)
3. Камера может войти внутрь падающего объекта
4. 100 деревьев падают одновременно = LAG SPIKE (Chaos Physics тяжёлый)
```

**Решение 1: Collision Safety (защита от выталкивания)**

```cpp
AActor* SpawnPhysicsVersion(UStaticMesh* Mesh, const FTransform& Transform)
{
    AActor* PhysicsActor = GetWorld()->SpawnActor<AStaticMeshActor>(
        AStaticMeshActor::StaticClass(), Transform);

    UStaticMeshComponent* MeshComp = PhysicsActor->GetStaticMeshComponent();
    MeshComp->SetStaticMesh(Mesh);
    MeshComp->SetSimulatePhysics(true);

    // === КРИТИЧНО: Безопасные collision channels ===

    // Временно отключаем коллизию с игроком и камерой
    MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

    // Через 2 секунды - восстанавливаем коллизию
    FTimerHandle TimerHandle;
    GetWorld()->GetTimerManager().SetTimer(TimerHandle, [MeshComp]()
    {
        if (IsValid(MeshComp))
        {
            MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
            MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
        }
    }, 2.0f, false);

    // Также: ограничиваем скорость падающего объекта
    MeshComp->SetPhysicsMaxAngularVelocityInDegrees(360.f);  // Не крутится как бешеный
    MeshComp->SetPhysicsLinearVelocityDamping(0.5f);        // Замедление

    return PhysicsActor;
}
```

**Решение 2: Throttling (защита от лагов)**

```cpp
class FPhysicsCollapseThrottler
{
    // Очередь объектов, ожидающих падения
    TQueue<FAnchoredObject*> PendingCollapses;

    // Максимум объектов, которые могут начать падать за кадр
    const int32 MaxCollapsesPerFrame = 5;

    // Объекты "в ожидании" визуально "дрожат", но не падают
    TSet<FAnchoredObject*> SuspendedObjects;

public:
    void RequestCollapse(FAnchoredObject* Anchor)
    {
        // Не спавним физику сразу - добавляем в очередь
        PendingCollapses.Enqueue(Anchor);
        SuspendedObjects.Add(Anchor);

        // Начинаем визуальное "дрожание" (объект вот-вот упадёт)
        StartShakeAnimation(Anchor);
    }

    void Tick(float DeltaTime)
    {
        int32 ProcessedThisFrame = 0;

        while (!PendingCollapses.IsEmpty() && ProcessedThisFrame < MaxCollapsesPerFrame)
        {
            FAnchoredObject* Anchor;
            if (PendingCollapses.Dequeue(Anchor))
            {
                // Теперь реально спавним физику
                ActuallySpawnPhysics(Anchor);
                SuspendedObjects.Remove(Anchor);
                ProcessedThisFrame++;
            }
        }
    }

private:
    void StartShakeAnimation(FAnchoredObject* Anchor)
    {
        // Мелкая вибрация ISM instance - визуальный feedback
        // Игрок видит, что дерево "вот-вот упадёт"
        if (Anchor->ISMComponent.IsValid())
        {
            FTransform Original;
            Anchor->ISMComponent->GetInstanceTransform(Anchor->InstanceIndex, Original);

            // Добавляем в tick для анимации
            ShakingObjects.Add({ Anchor, Original, GetWorld()->GetTimeSeconds() });
        }
    }

    void TickShakeAnimation(float DeltaTime)
    {
        for (auto& Shake : ShakingObjects)
        {
            float Time = GetWorld()->GetTimeSeconds() - Shake.StartTime;
            float ShakeAmount = FMath::Sin(Time * 30.f) * 2.f;  // 2cm вибрация

            FTransform Shaken = Shake.OriginalTransform;
            Shaken.AddToTranslation(FVector(ShakeAmount, ShakeAmount * 0.5f, 0.f));

            Shake.Anchor->ISMComponent->UpdateInstanceTransform(
                Shake.Anchor->InstanceIndex, Shaken, true);
        }
    }
};
```

**Визуализация throttling:**

```
Взрыв! 100 деревьев теряют опору одновременно
┌─────────────────────────────────────────────────────────────────┐
│                                                                  │
│  Frame 0:                                                        │
│    🌲🌲🌲🌲🌲 ← 5 деревьев ПАДАЮТ (physics spawn)               │
│    🌲🌲🌲🌲🌲 ← 95 деревьев ДРОЖАТ (в очереди)                 │
│                    ↑ визуальный feedback: "вот-вот упадёт"       │
│                                                                  │
│  Frame 1:                                                        │
│    💨💨💨💨💨 ← первые 5 уже летят                             │
│    🌲🌲🌲🌲🌲 ← следующие 5 ПАДАЮТ                             │
│    🌲🌲🌲🌲🌲 ← 90 всё ещё ДРОЖАТ                              │
│                                                                  │
│  ...через 20 frames (~0.3 сек) все упали...                      │
│                                                                  │
│  Результат:                                                      │
│    ❌ Без throttling: 100 physics spawns за 1 кадр = 500ms spike │
│    ✅ С throttling:   5 physics spawns × 20 кадров = плавно!    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**Дополнительные оптимизации Chaos Physics:**

```cpp
struct FCollapsePhysicsSettings
{
    // Лимиты одновременно симулируемых объектов
    int32 MaxSimultaneousPhysicsObjects = 30;

    // Время жизни физического объекта (потом исчезает или становится static)
    float PhysicsLifetime = 10.f;

    // Порог скорости для "засыпания" (объект остановился)
    float SleepThreshold = 5.f;

    // После засыпания - конвертировать обратно в static или удалить
    bool bConvertToStaticOnSleep = true;

    // Упрощённая коллизия для падающих объектов
    bool bUseSimplifiedCollision = true;
};

void ConfigureChaosForCollapse(UPrimitiveComponent* Comp, const FCollapsePhysicsSettings& Settings)
{
    // 1. Упрощённая коллизия (не full mesh)
    if (Settings.bUseSimplifiedCollision)
    {
        Comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        // Используем convex hull вместо complex collision
        Comp->GetBodyInstance()->SetCollisionProfileName("PhysicsActor");
    }

    // 2. Sleep threshold - быстрее "засыпает"
    Comp->GetBodyInstance()->SleepFamily = ESleepFamily::Custom;
    Comp->GetBodyInstance()->CustomSleepThresholdMultiplier = 2.f;

    // 3. Damping - быстрее останавливается
    Comp->SetLinearDamping(0.5f);
    Comp->SetAngularDamping(0.5f);

    // 4. Таймер на удаление/конвертацию
    FTimerHandle LifetimeTimer;
    GetWorld()->GetTimerManager().SetTimer(LifetimeTimer, [Comp, Settings]()
    {
        if (IsValid(Comp))
        {
            if (Settings.bConvertToStaticOnSleep)
            {
                // Остановить физику, оставить как static debris
                Comp->SetSimulatePhysics(false);
            }
            else
            {
                // Fade out и удалить
                FadeOutAndDestroy(Comp->GetOwner());
            }
        }
    }, Settings.PhysicsLifetime, false);
}
```

**Рекомендуемые лимиты:**

| Параметр | Значение | Почему |
|----------|----------|--------|
| MaxCollapsesPerFrame | 3-5 | Баланс визуала и производительности |
| MaxSimultaneousPhysics | 20-30 | Chaos budget |
| CollisionSafetyDelay | 1.5-2.0 сек | Время падения дерева |
| PhysicsLifetime | 10-30 сек | Потом статик или удаление |
| ShakeAnimationDuration | 0.3-0.5 сек | Время "ожидания" в очереди |

---

**Оптимизация: Tiered Object System**

```cpp
// Не все объекты нужно отслеживать одинаково
enum class EAnchorTier : uint8
{
    // Tier 0: Трава, мелкие камни - НЕ отслеживаем
    // Просто исчезают при удалении чанка
    None,

    // Tier 1: Кусты, средние камни - простое отслеживание
    // При подкопе просто удаляются (без физики)
    Simple,

    // Tier 2: Деревья - полное отслеживание
    // При подкопе падают с физикой
    Full,

    // Tier 3: Структуры (частично разрушаемые)
    // Поддержка Chaos Destruction
    Destructible
};

EAnchorTier GetAnchorTier(UStaticMesh* Mesh)
{
    if (Mesh->GetBounds().BoxExtent.Z < 50.f)
        return EAnchorTier::None;      // Трава

    if (Mesh->GetBounds().BoxExtent.Z < 150.f)
        return EAnchorTier::Simple;    // Кусты

    if (IsTreeMesh(Mesh))
        return EAnchorTier::Full;      // Деревья

    return EAnchorTier::Simple;
}
```

---

**Архитектура интеграции:**

```
┌─────────────────────────────────────────────────────────────┐
│                  CHUNK LIFECYCLE                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. Voxel Generation                                        │
│     └─→ Terrain mesh + collision ready                     │
│                    │                                        │
│                    ▼                                        │
│  2. PCG Execution (ПОСЛЕ готовности terrain)               │
│     └─→ Surface sampling                                   │
│     └─→ Spawn instances (ISM для производительности)       │
│                    │                                        │
│                    ▼                                        │
│  3. Anchoring Registration                                  │
│     └─→ Каждый объект Tier 1+ регистрируется               │
│     └─→ Строится VoxelToObjects mapping                    │
│                    │                                        │
│                    ▼                                        │
│  4. Runtime (игра)                                          │
│     └─→ Player digs voxel                                  │
│     └─→ AnchoringSystem.OnVoxelRemoved()                   │
│     └─→ Check support, trigger physics if needed           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**Valheim-style падение дерева:**

```cpp
void SpawnFallingTree(UStaticMesh* TreeMesh, FTransform Transform)
{
    // 1. Спавнить как отдельный актор с физикой
    AStaticMeshActor* TreeActor = World->SpawnActor<AStaticMeshActor>(Transform);
    TreeActor->GetStaticMeshComponent()->SetStaticMesh(TreeMesh);
    TreeActor->GetStaticMeshComponent()->SetSimulatePhysics(true);

    // 2. Настроить Chaos для красивого падения
    TreeActor->GetStaticMeshComponent()->SetLinearDamping(0.5f);
    TreeActor->GetStaticMeshComponent()->SetAngularDamping(0.3f);

    // 3. Добавить звук и эффекты
    PlayTreeCreakSound(Transform.GetLocation());

    // 4. Самоуничтожение через время (или при остановке)
    TreeActor->SetLifeSpan(30.f);  // Удалить через 30 сек

    // 5. (Опционально) Разбить на брёвна при падении
    TreeActor->OnActorHit.AddDynamic(this, &HandleTreeImpact);
}

void HandleTreeImpact(AActor* Tree, AActor* Other, FVector NormalImpulse, const FHitResult& Hit)
{
    if (NormalImpulse.Size() > TreeBreakThreshold)
    {
        // Заменить на кучу брёвен (loot)
        SpawnLogs(Tree->GetActorLocation(), GetLogCount(Tree));
        Tree->Destroy();
    }
}
```

### Meadows биом (пример из Valheim)

**Forest Factor:**
```cpp
// FBM noise определяет лес vs поляна
float ForestFactor = FBM_Noise(pos * 0.01 * 0.4, seed);
bool IsForest = ForestFactor > 1.15f;
```

**PCG Graph для деревьев:**
```
[Surface Sampler] → [Biome Filter: Meadows]
                  → [Slope Filter: < 30°]
                  → [Forest Factor Filter]
                  → [Density: 0.02]
                  → [Random Scale: 0.8-1.2]
                  → [Spawn: BP_Tree_Beech]
```

**PCG Graph для травы:**
```
[Surface Sampler] → [Biome Filter]
                  → [Altitude Filter: 10-200m]
                  → [NOT Forest Filter]
                  → [Density: 0.5]
                  → [Spawn: Grass, Flowers]
```

### Деревья (ассеты)

**Megascans (бесплатно для UE5):**
- European Beech
- European Black Alder
- European Hornbeam

Доступны на **Fab.com** (не старый Quixel Bridge).

Включают: LODs, сезонные варианты, Pivot Painter для ветра.

---

## Навигация (NavMesh)

> **📋 TL;DR Decision (см. World Assumptions #4)**
>
> | AI Type | Navigation Method | NavMesh? |
> |---------|-------------------|----------|
> | Звери в open world | Steering + slope checks + raycasts | ❌ НЕТ |
> | NPC на дорогах | Spline following + local avoidance | ❌ НЕТ |
> | Враги в open world | Simple raycasts + steering | ❌ НЕТ |
> | AI в зданиях/данжах | NavMesh | ✅ ДА |
> | AI в пещерах | NavMesh | ✅ ДА |
>
> **Принцип:** NavMesh = только enclosed spaces (структуры, данжи, пещеры).
> Open world terrain НИКОГДА не использует NavMesh.

**Почему НЕ NavMesh для open world:**

```
1. Terrain динамический (игрок копает) → постоянный rebuild
2. Бесконечный мир → TilePoolSize overflow
3. Звери НЕ нуждаются в точной навигации:
   - Олень бежит от игрока → достаточно steering away
   - Волк атакует → достаточно raycast к цели
   - Птица летает → NavMesh вообще не применим
4. NPC на дорогах → дороги уже есть как splines
```

**Что использовать вместо:**

```cpp
// Для зверей: Steering Behaviors
FVector GetFleeDirection(APawn* Animal, AActor* Threat)
{
    FVector ToThreat = Threat->GetActorLocation() - Animal->GetActorLocation();
    FVector FleeDir = -ToThreat.GetSafeNormal();

    // Проверка проходимости (slope check)
    FVector TargetPos = Animal->GetActorLocation() + FleeDir * 500.f;
    float Slope = GetTerrainSlope(TargetPos);

    if (Slope > 45.f)  // Слишком крутой склон
        FleeDir = FindAlternativeDirection(Animal, FleeDir, Slope);

    return FleeDir;
}

// Для NPC на дорогах: Spline Following
void FollowRoad(ANPC* NPC, URoadSpline* Road)
{
    float Progress = Road->FindClosestPoint(NPC->GetActorLocation());
    FVector NextPoint = Road->GetLocationAtProgress(Progress + 0.01f);
    NPC->MoveToLocation(NextPoint);
}

// Для врагов: Simple Raycast
bool CanReachTarget(APawn* Enemy, AActor* Target)
{
    FHitResult Hit;
    FVector Start = Enemy->GetActorLocation();
    FVector End = Target->GetActorLocation();

    // Проверяем прямую видимость + slope
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility))
    {
        float Slope = GetTerrainSlope(End);
        return Slope < 50.f;  // Враг может забраться
    }
    return false;
}
```

---

### Проблема NavMesh в воксельном мире (референс)

```
Традиционный подход:
  Статичный уровень → NavMesh baked в редакторе → Работает отлично

Воксельный мир:
  Динамический terrain → NavMesh нужен runtime → 💥 LAG SPIKES

  + Игрок копает → NavMesh инвалидируется
  + Чанки стримятся → NavMesh перестраивается
  + Подземелья → Отдельный NavMesh для каждого этажа?
```

**Почему NavMesh Invokers не решают проблему:**
```
NavMesh Invoker (радиус 50м вокруг игрока):
  - Rebuild при каждом движении игрока
  - Rebuild при изменении terrain (копание)
  - O(n²) от количества полигонов в радиусе
  - 10-50ms спайки каждые несколько секунд
```

---

### Решение: Гибридная навигация

**Архитектура:**
```
┌─────────────────────────────────────────────────────────────┐
│                   NAVIGATION LAYERS                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Layer 3: MACRO (Region Graph)                              │
│     └─→ Граф регионов + дороги                             │
│     └─→ "Как добраться из деревни A в деревню B?"          │
│     └─→ Дешёвый A* по pre-computed графу                   │
│                                                             │
│  Layer 2: MESO (Chunk NavMesh Tiles)                        │
│     └─→ NavMesh per chunk (256×256м)                       │
│     └─→ Pre-baked при генерации чанка                      │
│     └─→ Инвалидируется только при изменении чанка          │
│                                                             │
│  Layer 1: MICRO (Local Steering)                            │
│     └─→ Raycast/Sweep для обхода препятствий               │
│     └─→ RVO (Reciprocal Velocity Obstacles) для толпы      │
│     └─→ Не требует NavMesh вообще                          │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

### Layer 3: Region Graph (макро-навигация)

```cpp
// Граф уже есть из системы дорог!
struct FNavigationGraph
{
    // Узлы: деревни, данжи, перекрёстки дорог, gateways
    TArray<FNavNode> Nodes;

    // Рёбра: дороги, тропы (с весами по длине/опасности)
    TArray<FNavEdge> Edges;

    // A* на высоком уровне
    TArray<FNavNode*> FindPath(FNavNode* Start, FNavNode* End)
    {
        // Дешёвый A* по графу из ~100-1000 узлов
        return AStar(Start, End, [](FNavEdge* E) { return E->TravelCost; });
    }
};

// Использование:
// NPC в деревне A хочет дойти до деревни C
TArray<FNavNode*> MacroPath = NavGraph.FindPath(VillageA, VillageC);
// Result: [VillageA] → [Crossroad_12] → [Gateway_34] → [VillageC]

// Теперь NPC следует по дороге от waypoint к waypoint
// Используя Layer 2 для локальной навигации
```

---

### Layer 2: Chunk NavMesh Tiles

**Ключевая идея:** NavMesh строится per-chunk, не глобально.

```cpp
class FNavigationChunk
{
    FIntPoint ChunkCoord;
    dtNavMesh* NavMeshTile;      // Recast/Detour tile
    uint32 TerrainVersion;        // Для инвалидации

public:
    void BuildNavMesh(const FVoxelChunk& VoxelChunk)
    {
        // 1. Получить collision geometry из воксельного чанка
        TArray<FVector> Vertices;
        TArray<int32> Indices;
        VoxelChunk.GetCollisionMesh(Vertices, Indices);

        // 2. Построить NavMesh tile (Recast)
        rcConfig Config;
        Config.cs = 50.f;           // Cell size (крупнее = быстрее)
        Config.ch = 20.f;           // Cell height
        Config.walkableSlopeAngle = 45.f;
        Config.agentRadius = 50.f;
        Config.agentHeight = 180.f;

        NavMeshTile = BuildRecastTile(Vertices, Indices, Config);
        TerrainVersion = VoxelChunk.GetVersion();
    }

    // Проверка актуальности
    bool NeedsRebuild(const FVoxelChunk& VoxelChunk) const
    {
        return TerrainVersion != VoxelChunk.GetVersion();
    }
};
```

**⚠️ Проблема сшивания тайлов (NavMesh Tile Stitching)**

В воксельном мире Marching Cubes/Dual Contouring генерируют меши независимо:

```
Чанк A                          Чанк B
┌─────────────────┐ │ ┌─────────────────┐
│                 │ │ │                 │
│    NavMesh A    │ │ │    NavMesh B    │
│                 │ │ │                 │
│            ●────┤ │ ├──●              │  ● = edge vertices
│                 │ │ │                 │
│            ●────┤ │ ├────●            │  ← МИКРО-ЩЕЛЬ!
│                 │ │ │                 │
└─────────────────┘ │ └─────────────────┘
                    │
            Граница чанков

Проблема: Edge vertices не совпадают точно
         → Recast не может сшить тайлы
         → Агенты застревают на границах!
```

**Решение 1: Border Overlap (нахлёст геометрии)**

```cpp
void FNavigationChunk::BuildNavMesh(const FVoxelChunk& VoxelChunk)
{
    // КРИТИЧНО: Берём геометрию с overlap на соседние чанки
    const float BorderPadding = 100.f;  // 1 метр нахлёст

    TArray<FVector> Vertices;
    TArray<int32> Indices;

    // Получаем EXPANDED bounds (включая соседей)
    FBox ExpandedBounds = VoxelChunk.GetBounds();
    ExpandedBounds = ExpandedBounds.ExpandBy(BorderPadding);

    // Собираем геометрию из текущего чанка + overlap из соседей
    VoxelChunk.GetCollisionMesh(Vertices, Indices);
    AppendNeighborBorderGeometry(Vertices, Indices, BorderPadding);

    // Recast config с учётом border
    rcConfig Config;
    Config.cs = 50.f;
    Config.ch = 20.f;
    Config.borderSize = FMath::CeilToInt(BorderPadding / Config.cs);  // ← КЛЮЧЕВОЕ!
    Config.walkableSlopeAngle = 45.f;
    Config.agentRadius = 50.f;
    Config.agentHeight = 180.f;

    // TileSize должен быть кратен размеру чанка
    Config.tileSize = ChunkSize / Config.cs;

    NavMeshTile = BuildRecastTile(Vertices, Indices, Config);
}

// Получаем border geometry из соседних чанков
void AppendNeighborBorderGeometry(TArray<FVector>& Vertices, TArray<int32>& Indices,
                                   float BorderSize)
{
    for (EDirection Dir : {North, South, East, West})
    {
        FVoxelChunk* Neighbor = GetNeighborChunk(Dir);
        if (Neighbor && Neighbor->IsReady())
        {
            FBox BorderBox = GetBorderBox(Dir, BorderSize);
            Neighbor->GetCollisionMeshInBounds(BorderBox, Vertices, Indices);
        }
    }
}
```

**Решение 2: Shared Edge Vertices (синхронизация границ)**

```cpp
// Воксельный меш генерируется с фиксированными позициями на границах
class FVoxelMeshGenerator
{
    // При генерации меша на границе чанка:
    // Используем ОДИНАКОВЫЕ vertex positions для обоих чанков

    FVector SnapToBorder(FVector Vertex, const FBox& ChunkBounds) const
    {
        const float SnapThreshold = 1.f;  // 1 см

        // Snap к границе чанка если близко
        if (FMath::Abs(Vertex.X - ChunkBounds.Min.X) < SnapThreshold)
            Vertex.X = ChunkBounds.Min.X;
        if (FMath::Abs(Vertex.X - ChunkBounds.Max.X) < SnapThreshold)
            Vertex.X = ChunkBounds.Max.X;
        // ... аналогично для Y, Z

        return Vertex;
    }

    void GenerateMesh(const FVoxelChunk& Chunk)
    {
        // ... marching cubes ...

        // Snap border vertices
        for (FVector& V : Vertices)
        {
            if (IsOnChunkBorder(V, Chunk.GetBounds()))
            {
                V = SnapToBorder(V, Chunk.GetBounds());
            }
        }
    }
};
```

**Решение 3: UE5 NavMesh Tile Pool (рекомендуется)**

Использовать встроенную систему UE5:

```cpp
// Project Settings → Navigation System
void ConfigureNavMeshForVoxelWorld()
{
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
    ARecastNavMesh* RecastNavMesh = Cast<ARecastNavMesh>(NavSys->GetMainNavData());

    // КРИТИЧНЫЕ настройки для воксельного мира:

    // 1. Фиксированный пул тайлов (лучшая производительность)
    RecastNavMesh->bFixedTilePoolSize = true;
    RecastNavMesh->TilePoolSize = 1024;  // Достаточно для стриминга

    // 2. Размер тайла = размер чанка (ВАЖНО!)
    RecastNavMesh->TileSizeUU = ChunkSize;  // например 25600 (256м)

    // 3. Border padding для сшивания
    // В реальности это Agent Radius × 2
    // Recast автоматически добавит padding при компиляции

    // 4. Включить runtime generation
    RecastNavMesh->RuntimeGeneration = ERuntimeGenerationType::Dynamic;

    // 5. Cell size влияет на качество сшивания
    RecastNavMesh->CellSize = 19.f;       // Default
    RecastNavMesh->CellHeight = 10.f;     // Default

    // 6. Geometry Export (для корректного overlap)
    RecastNavMesh->bDoFullyAsyncNavDataGathering = true;
}
```

**Визуализация правильного сшивания:**

```
БЕЗ overlap:                      С overlap (BorderPadding):
┌──────────┐│┌──────────┐         ┌──────────┬┬──────────┐
│          │││          │         │          ││          │
│    A     │││    B     │         │    A   ██││██   B    │
│          │││          │         │        ██││██        │
│       ●──┤││┌──●      │         │       ●══╬╬══●       │
│          │││          │         │        ██││██        │
│       ●──┤││├────●    │         │       ●══╬╬════●     │
│          │││          │         │        ██││██        │
└──────────┘│└──────────┘         └──────────┴┴──────────┘
       ↑                                    ↑
  Микро-щели!                    ██ = overlap geometry
  Агенты застревают               ╬ = shared edge (сшито!)
```

**Debug: Проверка сшивания**

```cpp
// Визуализация проблем с NavMesh stitching
void DebugNavMeshStitching()
{
    ARecastNavMesh* NavMesh = GetRecastNavMesh();

    // Получить все edges тайлов
    TArray<FNavMeshTileEdge> TileEdges;
    NavMesh->GetAllTileEdges(TileEdges);

    for (const FNavMeshTileEdge& Edge : TileEdges)
    {
        if (!Edge.bIsConnected)
        {
            // Несшитый edge — потенциальная проблема!
            DrawDebugLine(GetWorld(), Edge.Start, Edge.End,
                         FColor::Red, false, 10.f, 0, 5.f);

            UE_LOG(LogNavMesh, Warning,
                TEXT("Unstitched edge at %s - %s (Tiles: %d, %d)"),
                *Edge.Start.ToString(), *Edge.End.ToString(),
                Edge.TileA, Edge.TileB);
        }
    }
}
```

**Рекомендуемые параметры Recast для воксельного мира:**

| Параметр | Значение | Почему |
|----------|----------|--------|
| `TileSizeUU` | = ChunkSize | Один тайл = один чанк |
| `CellSize` | 19-50 | Меньше = точнее сшивание, больше = быстрее |
| `borderSize` | ≥ AgentRadius / CellSize | Минимум для overlap |
| `bFixedTilePoolSize` | true | Предотвращает аллокации runtime |
| `bDoFullyAsyncNavDataGathering` | true | Async сбор геометрии |

**⚠️ ВАЖНО: TilePoolSize в бесконечном мире**

`bFixedTilePoolSize = true` — палка о двух концах:

```
Проблема: Пул заполнился
┌─────────────────────────────────────────────────────────────────┐
│                                                                  │
│  TilePoolSize = 256 тайлов                                      │
│                                                                  │
│  Загружено регионов: 300+ (игрок далеко ушёл)                   │
│                                                                  │
│  Что происходит:                                                │
│    1. Recast начинает удалять "старые" тайлы                    │
│    2. AI в далёких регионах теряет NavMesh                      │
│    3. NPC "тупят" или проваливаются сквозь пол                 │
│                                                                  │
│  Пример:                                                         │
│    [Игрок]  ← NavMesh есть                                      │
│       ↓                                                          │
│    [300м]   ← NavMesh есть                                      │
│       ↓                                                          │
│    [Далёкий регион с активным NPC]                              │
│       ↓                                                          │
│    💀 NavMesh удалён! NPC провалился!                           │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**Решение: NavMesh Streaming с Invokers**

```cpp
// НЕ держим NavMesh для всего загруженного мира!
// Только в радиусе от "навигационных точек интереса"

class FNavMeshStreamingManager
{
    // Invokers = точки, вокруг которых нужен NavMesh
    TArray<FNavMeshInvoker> Invokers;

    struct FNavMeshInvoker
    {
        TWeakObjectPtr<AActor> Actor;  // Игрок или активный NPC
        float Radius;                   // Радиус NavMesh вокруг
        int32 Priority;                 // Приоритет (игрок > NPC)
    };

    const float PlayerNavRadius = 300.f;     // NavMesh вокруг игрока
    const float ActiveNPCNavRadius = 100.f;  // NavMesh вокруг активных NPC
    const float InactiveNPCNavRadius = 0.f;  // Далёкие NPC используют Macro Graph

public:
    void RegisterInvoker(AActor* Actor, EInvokerType Type)
    {
        FNavMeshInvoker Invoker;
        Invoker.Actor = Actor;
        Invoker.Priority = (Type == EInvokerType::Player) ? 100 : 10;
        Invoker.Radius = (Type == EInvokerType::Player) ? PlayerNavRadius : ActiveNPCNavRadius;

        Invokers.Add(Invoker);
    }

    void Tick()
    {
        // 1. Собрать все чанки, которым НУЖЕН NavMesh
        TSet<FIntPoint> RequiredChunks;

        for (const FNavMeshInvoker& Inv : Invokers)
        {
            if (!Inv.Actor.IsValid())
                continue;

            FVector Pos = Inv.Actor->GetActorLocation();
            ForEachChunkInRadius(Pos, Inv.Radius, [&](FIntPoint Chunk)
            {
                RequiredChunks.Add(Chunk);
            });
        }

        // 2. Освободить NavMesh для чанков вне радиуса
        for (auto& Pair : LoadedNavMeshTiles)
        {
            if (!RequiredChunks.Contains(Pair.Key))
            {
                // NPC далеко → переключить на Macro Graph навигацию
                NotifyNPCsInChunk(Pair.Key, ENavMode::MacroGraphOnly);

                // Освободить тайл
                FreeNavMeshTile(Pair.Key);
            }
        }

        // 3. Загрузить NavMesh для новых чанков в радиусе
        for (FIntPoint Chunk : RequiredChunks)
        {
            if (!LoadedNavMeshTiles.Contains(Chunk))
            {
                QueueNavMeshBuild(Chunk);
            }
        }
    }
};
```

**Визуализация NavMesh стриминга:**

```
┌─────────────────────────────────────────────────────────────────┐
│                                                                  │
│       LOADED WORLD (много регионов)                             │
│                                                                  │
│   ┌───────┬───────┬───────┬───────┬───────┬───────┐            │
│   │       │       │ Macro │       │       │       │            │
│   │  ---  │  ---  │ only  │  ---  │  ---  │  ---  │            │
│   │       │       │  🐺   │       │       │       │            │
│   ├───────┼───────┼───────┼───────┼───────┼───────┤            │
│   │       │░░░░░░░│░░░░░░░│░░░░░░░│       │       │            │
│   │  ---  │░NavM░░│░NavM░░│░NavM░░│  ---  │  ---  │            │
│   │       │░░░░░░░│░░🧑░░░│░░░░░░░│       │       │            │
│   ├───────┼───────┼───────┼───────┼───────┼───────┤            │
│   │       │░░░░░░░│░░░░░░░│░░░░░░░│       │       │            │
│   │  ---  │░NavM░░│░NavM░░│░NavM░░│  ---  │  ---  │            │
│   │       │░░🐺░░░│░░░░░░░│░░░░░░░│       │       │            │
│   └───────┴───────┴───────┴───────┴───────┴───────┘            │
│                                                                  │
│   🧑 = Игрок (PlayerNavRadius = 300м)                          │
│   🐺 = NPC (ActiveNPCNavRadius = 100м если близко к игроку)    │
│   ░░ = NavMesh тайлы активны                                    │
│   -- = Macro Graph only (тайлы выгружены)                       │
│                                                                  │
│   Далёкий волк (🐺 вверху) использует ТОЛЬКО Layer 3            │
│   (Region Graph), не требует детального NavMesh                 │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**Расчёт TilePoolSize:**

```cpp
// Сколько тайлов реально нужно?

int32 CalculateRequiredTilePoolSize()
{
    const float PlayerNavRadius = 300.f;
    const float TileSize = 256.f;  // = ChunkSize

    // Игрок: круг радиусом 300м
    // Покрывает ~(300/256)² × π ≈ 4-5 тайлов

    // Активные NPC вокруг игрока (например, 10 NPC)
    // Каждый добавляет ~1-2 тайла

    // Buffer для async loading
    const int32 AsyncBuffer = 8;

    // Минимум для одного игрока:
    int32 PlayerTiles = FMath::CeilToInt(
        PI * FMath::Square(PlayerNavRadius / TileSize)
    );  // ≈ 5

    int32 NPCTiles = 10 * 2;  // 10 активных NPC × 2 тайла

    return (PlayerTiles + NPCTiles + AsyncBuffer) * 2;  // ×2 запас
    // ≈ 50-100 тайлов достаточно для STREAMING подхода
}
```

**Сравнение подходов:**

| Подход | TilePoolSize | Проблемы | Рекомендация |
|--------|--------------|----------|--------------|
| Весь загруженный мир | 1000+ | Память, rebuild time | ❌ Не масштабируется |
| Фиксированный радиус | 256 | AI далеко тупит | ⚠️ Workaround |
| **NavMesh Streaming** | 64-128 | Нужна логика стриминга | ✅ Рекомендуется |

**Что делать NPC вне NavMesh радиуса:**

```cpp
enum class ENPCNavigationMode
{
    FullNavMesh,      // Внутри радиуса - полная навигация
    MacroGraphOnly,   // Вне радиуса - только Layer 3 (дороги)
    Teleport,         // Очень далеко - телепортация при приближении игрока
    Suspended         // За пределами симуляции - заморожен
};

void ANPC::UpdateNavigationMode()
{
    float DistToPlayer = GetDistanceToPlayer();

    if (DistToPlayer < 300.f)
        NavigationMode = ENPCNavigationMode::FullNavMesh;
    else if (DistToPlayer < 1000.f)
        NavigationMode = ENPCNavigationMode::MacroGraphOnly;
    else if (DistToPlayer < 5000.f)
        NavigationMode = ENPCNavigationMode::Teleport;
    else
        NavigationMode = ENPCNavigationMode::Suspended;
}
```

**Chunk State Machine (расширенная):**

```cpp
enum class EChunkState : uint8
{
    Unloaded,
    VoxelGenerating,
    VoxelReady,
    NavMeshBuilding,    // ← Новое состояние
    NavMeshReady,       // ← Новое состояние
    PCGGenerating,
    Complete
};

void FVoxelChunk::OnVoxelMeshReady()
{
    State = EChunkState::VoxelReady;

    // Строим NavMesh асинхронно
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this]()
    {
        NavigationChunk.BuildNavMesh(*this);

        AsyncTask(ENamedThreads::GameThread, [this]()
        {
            State = EChunkState::NavMeshReady;
            OnNavMeshReady();
        });
    });
}
```

---

### Инвалидация при копании

```cpp
void FVoxelWorld::OnVoxelModified(FIntVector VoxelCoord)
{
    // 1. Найти затронутый чанк
    FIntPoint ChunkCoord = VoxelToChunk(VoxelCoord);
    FVoxelChunk* Chunk = GetChunk(ChunkCoord);

    // 2. Инкрементировать версию
    Chunk->IncrementVersion();

    // 3. Пометить NavMesh как dirty (НЕ перестраивать сразу!)
    Chunk->MarkNavMeshDirty();

    // 4. Добавить в очередь на rebuild (с debounce)
    NavMeshRebuildQueue.AddUnique(ChunkCoord);
}

void FVoxelWorld::ProcessNavMeshRebuildQueue()
{
    // Обрабатываем не более N чанков за кадр
    const int32 MaxRebuildsPerFrame = 1;

    for (int32 i = 0; i < MaxRebuildsPerFrame && NavMeshRebuildQueue.Num() > 0; i++)
    {
        FIntPoint ChunkCoord = NavMeshRebuildQueue.Pop();
        FVoxelChunk* Chunk = GetChunk(ChunkCoord);

        // Rebuild только если NavMesh действительно устарел
        if (Chunk && Chunk->IsNavMeshDirty())
        {
            // Async rebuild
            Chunk->RebuildNavMeshAsync();
        }
    }
}
```

**Debounce для множественных изменений:**

```cpp
// Игрок копает несколько вокселей подряд
// Не перестраиваем NavMesh после каждого!

class FNavMeshRebuildScheduler
{
    TMap<FIntPoint, float> DirtyChunks;  // ChunkCoord → LastModifyTime
    const float RebuildDelay = 0.5f;      // Ждём 0.5 сек после последнего изменения

public:
    void MarkDirty(FIntPoint ChunkCoord)
    {
        DirtyChunks.Add(ChunkCoord, GetWorld()->GetTimeSeconds());
    }

    void Tick(float DeltaTime)
    {
        float CurrentTime = GetWorld()->GetTimeSeconds();

        for (auto It = DirtyChunks.CreateIterator(); It; ++It)
        {
            if (CurrentTime - It.Value() > RebuildDelay)
            {
                // Прошло достаточно времени - rebuild
                ScheduleRebuild(It.Key());
                It.RemoveCurrent();
            }
        }
    }
};
```

---

### Layer 1: Local Steering (микро-навигация)

**Для простых NPC (животные, мобы) NavMesh не нужен вообще:**

```cpp
// Простой steering без NavMesh
class FLocalSteering
{
public:
    FVector ComputeMovement(AActor* Agent, FVector TargetLocation)
    {
        FVector ToTarget = TargetLocation - Agent->GetActorLocation();
        FVector DesiredVelocity = ToTarget.GetSafeNormal() * MaxSpeed;

        // Obstacle avoidance через raycasts
        FVector AvoidanceForce = ComputeAvoidance(Agent);

        // Финальное направление
        return (DesiredVelocity + AvoidanceForce).GetSafeNormal();
    }

private:
    FVector ComputeAvoidance(AActor* Agent)
    {
        FVector Avoidance = FVector::ZeroVector;

        // Веер raycast-ов вперёд
        const int32 NumRays = 5;
        const float SpreadAngle = 60.f;

        for (int32 i = 0; i < NumRays; i++)
        {
            float Angle = FMath::Lerp(-SpreadAngle, SpreadAngle, float(i) / (NumRays - 1));
            FVector RayDir = Agent->GetActorForwardVector().RotateAngleAxis(Angle, FVector::UpVector);

            FHitResult Hit;
            if (LineTrace(Agent->GetActorLocation(), RayDir * DetectionRange, Hit))
            {
                // Отталкивание от препятствия
                float Weight = 1.f - (Hit.Distance / DetectionRange);
                Avoidance -= Hit.Normal * Weight * AvoidanceStrength;
            }
        }

        return Avoidance;
    }
};
```

**RVO для толпы NPC:**

```cpp
// Reciprocal Velocity Obstacles - NPC избегают друг друга
class FRVOSimulator
{
    TArray<FRVOAgent> Agents;

public:
    void Tick(float DeltaTime)
    {
        // 1. Собрать желаемые velocity всех агентов
        for (FRVOAgent& Agent : Agents)
        {
            Agent.DesiredVelocity = ComputeDesiredVelocity(Agent);
        }

        // 2. ORCA (Optimal Reciprocal Collision Avoidance)
        for (FRVOAgent& Agent : Agents)
        {
            Agent.ActualVelocity = ORCA(Agent, Agents, DeltaTime);
        }

        // 3. Применить движение
        for (FRVOAgent& Agent : Agents)
        {
            Agent.Position += Agent.ActualVelocity * DeltaTime;
        }
    }
};
```

---

### Специальные случаи

**Подземелья (Dungeons):**

```cpp
// Данжи - закрытые пространства, можно pre-bake полностью

class ADungeonActor
{
    // NavMesh для всего данжа, baked при генерации
    UNavMeshComponent* DungeonNavMesh;

public:
    void GenerateDungeon(int32 Seed)
    {
        // 1. Генерация layout
        GenerateRooms(Seed);

        // 2. Bake NavMesh (один раз, не меняется)
        BakeNavMesh();

        // 3. Данж готов - NavMesh статичный
    }
};
```

**Многоэтажные структуры:**

```cpp
// NavMesh Links для лестниц, лифтов
class FMultiFloorNavigation
{
    // Каждый этаж - отдельный NavMesh tile
    TMap<int32, dtNavMesh*> FloorNavMeshes;

    // Links между этажами
    TArray<FNavMeshLink> FloorConnections;

public:
    void AddStaircase(FVector Bottom, FVector Top)
    {
        // NavMesh Link позволяет pathfinding между этажами
        FNavMeshLink Link;
        Link.Start = Bottom;
        Link.End = Top;
        Link.Type = ELinkType::Staircase;
        Link.Cost = (Top - Bottom).Size();

        FloorConnections.Add(Link);
    }
};
```

**Вода и плавание:**

```cpp
// Отдельный "NavMesh" для плавающих NPC
enum class ENavigationDomain : uint8
{
    Ground,     // Обычный NavMesh
    Water,      // Поверхность воды (2D pathfinding)
    Underwater, // Под водой (3D pathfinding)
    Flying      // Воздух (3D pathfinding)
};

// NPC с несколькими доменами (амфибии)
class FAmphibianAI
{
    bool CanSwim = true;
    bool CanWalk = true;

    FVector FindPath(FVector Target)
    {
        if (IsInWater(GetLocation()) && IsInWater(Target))
            return WaterPathfinding(Target);
        else if (!IsInWater(GetLocation()) && !IsInWater(Target))
            return GroundPathfinding(Target);
        else
            return MultiDomainPathfinding(Target);  // Переход вода↔суша
    }
};
```

---

### Performance Budget

```
┌─────────────────────────────────────────────────────────────┐
│                NAVIGATION PERFORMANCE                        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Target: 60 FPS, max 2ms на навигацию                       │
│                                                             │
│  Layer 3 (Region Graph):                                    │
│    - A* на 1000 узлов: ~0.01ms                             │
│    - Вызывается редко (раз в несколько секунд)             │
│                                                             │
│  Layer 2 (Chunk NavMesh):                                   │
│    - Pathfinding внутри чанка: ~0.1ms                       │
│    - Rebuild одного чанка: ~50ms (async!)                  │
│    - Max 1 rebuild per frame                                │
│                                                             │
│  Layer 1 (Local Steering):                                  │
│    - 5 raycasts per NPC: ~0.02ms                           │
│    - RVO для 100 NPC: ~0.5ms                               │
│                                                             │
│  TOTAL per frame (100 NPC, normal gameplay): ~1ms          │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

### Рекомендации по типам NPC

| Тип NPC | Навигация | Причина |
|---------|-----------|---------|
| Животные (олени, кролики) | Layer 1 only | Простое поведение, избегание |
| Мобы (гоблины) | Layer 2 + Layer 1 | Нужен pathfinding к игроку |
| Жители деревни | Layer 3 + Layer 2 | Ходят по дорогам между зданиями |
| Караваны | Layer 3 only | Следуют по дорогам |
| Боссы | Custom | Уникальное поведение |
| Летающие | 3D steering | NavMesh не нужен |

---

## Итоговая архитектура

> 📋 **Каноническая диаграмма порядка генерации**: см. [Обзор архитектуры → Порядок генерации](#порядок-генерации)

### Дополнительные системы (не в основной диаграмме)

```
┌─────────────────────────────────────────────────────────────┐
│  POIs, Дороги (вычисляются лениво, по запросу)             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  GetPOIsInArea(bounds, seed) - чистая функция              │
│       ├── Settlement Grid → деревни                        │
│       └── Dungeon placement → данжи                        │
│                                                             │
│  ComputeRoadBetween(A, B, seed) - чистая функция           │
│       └── A* по GetHeight_Gameplay (из текстуры!)          │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│                    PLAYER MODIFICATIONS                      │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Player modifies terrain:                                   │
│    → Store Delta (removed/added voxels)                    │
│    → Delta applies ON TOP of procedural layer              │
│                                                             │
│  Player places/destroys objects:                           │
│    → Store destroyed procedural objects (IDs)              │
│    → Store player buildings (position, type, rotation)     │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│                    SAVE FILE (~1 MB)                         │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  WorldSeed: 12345                    (4 bytes)             │
│  ModifiedChunks: RLE compressed      (~500 KB)             │
│  DestroyedObjects: list of IDs       (~24 KB)              │
│  PlayerObjects: position+type+rot    (~100 KB)             │
│                                                             │
│  ⚠️ Реки НЕ хранятся — preprocessing при загрузке региона! │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Гарантии детерминизма

```cpp
// Любой игрок с одним seed видит ОДИН И ТОТ ЖЕ мир:
// - Те же горы, долины, берега
// - Те же реки в тех же местах
// - Те же деревни и дороги
// - Те же руды на той же глубине

// Детерминизм НЕ зависит от КОГДА вычисляем:
// ComputeRegionRivers(coord, seed) - всегда один результат
// Неважно, загрузил регион первым или последним
```

### Классификация операций

| Операция | Фаза | Когда вычисляется | Сложность |
|----------|------|-------------------|-----------|
| **BAKING PHASE** (при стриминге региона): |||
| BaseHeight | Baking | Per-texel, один раз | O(noise) |
| Settlement influence | Baking | Per-texel, один раз | O(1) |
| Rivers | Preprocessing | До baking | O(L) один раз |
| River carving | Baking | Per-texel | O(1) spatial query |
| **RUNTIME** (чтение из текстур): |||
| GetHeight_Gameplay | Texture read | Per-voxel | O(1) |
| GetHeight_Render | Texture read | Per-voxel | O(1) |
| POIs | Чистая функция | При необходимости | O(grid cells) |
| Roads | Чистая функция | Лениво | O(A*) |

---

## Ссылки и ресурсы

- **VoxelPlugin2**: https://voxelplugin.com
- **Megascans Trees**: https://fab.com (поиск "Megascans trees")
- **Valheim World Gen**: Декомпилированный код в `D:\Work\assembly-valheim\`
- **PCG Documentation**: UE5 Procedural Content Generation Framework

---

*Документ создан на основе обсуждения архитектуры процедурной генерации мира.*
