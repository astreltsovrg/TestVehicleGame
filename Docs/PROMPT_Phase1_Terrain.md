# Prompt: Phase 1 — Procedural Terrain Implementation

Скопируй этот промпт в новый диалог Claude Code.

---

## Контекст

Проект: `TestVehicleGame` (UE5.7 Source Build)
Путь: `D:\Work\Unreal\UE5_7_Source\TestVehicleGame`

Документация:
- Основной документ: `Docs/ProceduralWorldGeneration.md`
- Phase 1 спецификация: `Docs/ProceduralWorld_Phase1_Terrain.md`

## Задача

Реализовать **Phase 1: Basic Procedural Terrain** — минимальный MVP процедурного террейна.

### Scope Phase 1

| Включено | Исключено |
|----------|-----------|
| ✅ Seed-based terrain | ❌ Биомы |
| ✅ Region streaming (4×4 км) | ❌ Реки |
| ✅ Height baking в R32 текстуру | ❌ Растительность |
| ✅ VoxelPlugin2 интеграция | ❌ Деревни/дороги |
| ✅ Debug материал | ❌ Данжи |

### Ожидаемый результат

1. Игрок спавнится в процедурном мире
2. Terrain генерируется вокруг игрока (холмы, горы из Simplex noise)
3. При движении — новые регионы подгружаются, старые выгружаются
4. Один seed = один и тот же terrain (детерминизм)

---

## Пошаговый план

### Шаг 0: Создание уровня

**Вопрос:** Какой тип уровня создать?

Рекомендация: **Empty Open World** (если нужен World Partition) или **Empty Level** (если стриминг полностью кастомный).

Для нашей архитектуры с кастомными регионами 4×4 км лучше **Empty Level** — мы сами управляем стримингом.

Создай уровень:
- File → New Level → Empty Level
- Сохрани как: `Content/ProceduralWorld/Maps/L_ProceduralWorld`

Добавь минимум для тестирования:
- DirectionalLight (Sun)
- SkyAtmosphere
- Player Start (временно в 0,0,1000 — над террейном)

### Шаг 1: C++ классы

Создай следующие классы в `Source/TestVehicleGame/ProceduralWorld/`:

```
ProceduralWorld/
├── RegionManager.h/.cpp         — UWorldSubsystem, управляет стримингом
├── RegionData.h/.cpp            — UObject, данные одного региона
├── HeightGenerator.h/.cpp       — Статический класс, генерация высоты
└── ProceduralWorldSettings.h    — UDeveloperSettings, настройки (seed, etc.)
```

### Шаг 2: Height Generation

Реализуй `FHeightGenerator::BakeRegionHeight()`:
- Input: RegionCoord, WorldSeed, Resolution (512)
- Output: TArray<float> — высоты в метрах
- Алгоритм: SimplexFBM (6 октав) + Domain Warping

### Шаг 3: Region Streaming

Реализуй `URegionManager`:
- `UpdateStreaming(PlayerPosition)` — вызывается каждый tick
- Загружает регионы в радиусе 2 от игрока
- Выгружает регионы за пределами радиуса 3
- Async baking в background thread

### Шаг 4: VoxelPlugin2 интеграция

**ВАЖНО:** Прочитай документацию VoxelPlugin2 в `Plugins/VoxelPlugin/`.

Создай Voxel Graph:
- Input: World Position
- Sample HeightTexture по UV региона
- Output: SDF = WorldZ - SampledHeight

### Шаг 5: Debug Material

Создай `M_DebugTerrain`:
- Простой gradient по высоте (синий внизу → белый вверху)
- Или просто серый цвет для начала

### Шаг 6: Game Mode

Создай или модифицируй GameMode для процедурного мира:
- Spawn player в позиции (0, 0, 500) — над террейном
- Инициализируй RegionManager с seed'ом

---

## Технические требования

### Build.cs

Добавь зависимости:
```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "Voxel"  // VoxelPlugin2
});
```

### Настройки проекта

Убедись что VoxelPlugin включён в `.uproject`:
```json
{
    "Plugins": [
        { "Name": "Voxel", "Enabled": true }
    ]
}
```

---

## Метрики успеха

| Метрика | Target |
|---------|--------|
| Region bake time | < 500ms |
| HeightTexture memory | 1 MB per region |
| Max loaded regions | ~25 (5×5) |
| Hitches при стриминге | Нет |
| Determinism | seed → identical terrain |

---

## Начни с

1. Прочитай `Docs/ProceduralWorld_Phase1_Terrain.md` полностью
2. Создай Empty Level и сохрани
3. Создай C++ классы (начни с RegionData и HeightGenerator)
4. Протестируй генерацию высоты в изоляции (без VoxelPlugin)
5. Интегрируй с VoxelPlugin2

**Первый вопрос ко мне:** После создания уровня и базовых C++ классов — покажи структуру файлов и спроси если что-то неясно.

---

## Ссылки

- VoxelPlugin2 docs: `Plugins/VoxelPlugin/`
- Phase 1 spec: `Docs/ProceduralWorld_Phase1_Terrain.md`
- Main architecture: `Docs/ProceduralWorldGeneration.md`
