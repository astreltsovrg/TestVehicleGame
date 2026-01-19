# Phase 1: Basic Procedural Terrain

Минимальная реализация процедурного террейна. Цель — валидировать core архитектуру до добавления сложных систем.

---

## Scope Phase 1

| Включено | Исключено (Phase 2+) |
|----------|---------------------|
| ✅ Seed-based terrain | ❌ Реки |
| ✅ Region streaming (4×4 км) | ❌ Деревни/settlements |
| ✅ Height baking в текстуру | ❌ Дороги |
| ✅ VoxelPlugin2 интеграция | ❌ Данжи |
| ✅ Простейший материал (один цвет/текстура) | ❌ PCG растительность |
| | ❌ Биомы |
| | ❌ Геологические слои/майнинг |

---

## Архитектура

### Ключевые принципы

1. **Один seed** → один и тот же мир у всех игроков
2. **Baked textures** → height вычисляется при стриминге, runtime читает из текстуры
3. **Регионы 4×4 км** → единица стриминга
4. **Детерминизм** → `f(position, seed) = same result` всегда

### Порядок генерации

```
World Seed (int32)
    │
    └─→ Player approaches region
          │
          └─→ BAKING (async, background thread)
                │
                ├─ 1. Generate HeightTexture (512×512, R32_FLOAT)
                │     └─ SimplexFBM(pos, seed) + Domain Warping
                │
                └─ 2. Copy to CPU cache (для gameplay queries)
                      └─ HeightCache_CPU[]

          └─→ RUNTIME (когда игрок в регионе)
                │
                ├─ VoxelPlugin2 читает HeightTexture
                │     └─ SDF = WorldZ - SampledHeight
                │
                └─ Material — простейший (один цвет или debug визуализация)
```

---

## Реализация

### 1. Region Manager

```cpp
UCLASS()
class URegionManager : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // Текущие загруженные регионы
    TMap<FIntPoint, URegionData*> LoadedRegions;

    // Настройки
    static constexpr float RegionSize = 4096.f;  // 4 км
    static constexpr int32 StreamingRadius = 2;   // регионов вокруг игрока

    // Основной tick
    void UpdateStreaming(FVector PlayerPosition);

private:
    FIntPoint WorldToRegionCoord(FVector WorldPos);
    void LoadRegion(FIntPoint Coord);
    void UnloadRegion(FIntPoint Coord);
};
```

```cpp
void URegionManager::UpdateStreaming(FVector PlayerPos)
{
    FIntPoint PlayerRegion = WorldToRegionCoord(PlayerPos);

    // Определяем нужные регионы
    TSet<FIntPoint> NeededRegions;
    for (int32 X = -StreamingRadius; X <= StreamingRadius; X++)
    {
        for (int32 Y = -StreamingRadius; Y <= StreamingRadius; Y++)
        {
            NeededRegions.Add(PlayerRegion + FIntPoint(X, Y));
        }
    }

    // Выгружаем ненужные
    TArray<FIntPoint> ToUnload;
    for (auto& Pair : LoadedRegions)
    {
        if (!NeededRegions.Contains(Pair.Key))
            ToUnload.Add(Pair.Key);
    }
    for (FIntPoint Coord : ToUnload)
        UnloadRegion(Coord);

    // Загружаем новые
    for (FIntPoint Coord : NeededRegions)
    {
        if (!LoadedRegions.Contains(Coord))
            LoadRegion(Coord);
    }
}
```

### 2. Region Data

```cpp
UCLASS()
class URegionData : public UObject
{
    GENERATED_BODY()

public:
    FIntPoint Coord;
    bool bIsReady = false;

    // GPU texture (для VoxelPlugin)
    UPROPERTY()
    UTexture2D* HeightTexture;  // R32_FLOAT, 512×512

    // CPU cache (для gameplay queries)
    TArray<float> HeightCache;   // 512×512 floats

    // Быстрый доступ к высоте для gameplay
    float GetHeight(FVector2D WorldPos) const;

private:
    FVector2D WorldToUV(FVector2D WorldPos) const;
};
```

```cpp
float URegionData::GetHeight(FVector2D WorldPos) const
{
    FVector2D UV = WorldToUV(WorldPos);

    // Bilinear sample from CPU cache
    int32 X0 = FMath::FloorToInt(UV.X * 511);
    int32 Y0 = FMath::FloorToInt(UV.Y * 511);
    int32 X1 = FMath::Min(X0 + 1, 511);
    int32 Y1 = FMath::Min(Y0 + 1, 511);

    float FracX = UV.X * 511 - X0;
    float FracY = UV.Y * 511 - Y0;

    float H00 = HeightCache[Y0 * 512 + X0];
    float H10 = HeightCache[Y0 * 512 + X1];
    float H01 = HeightCache[Y1 * 512 + X0];
    float H11 = HeightCache[Y1 * 512 + X1];

    return FMath::BiLerp(H00, H10, H01, H11, FracX, FracY);
}
```

### 3. Height Generation (Baking)

```cpp
class FHeightGenerator
{
public:
    static void BakeRegionHeight(
        FIntPoint RegionCoord,
        int32 WorldSeed,
        TArray<float>& OutHeightData,
        int32 Resolution = 512)
    {
        OutHeightData.SetNum(Resolution * Resolution);

        const float RegionWorldSize = 4096.f;
        const FVector2D RegionOrigin = FVector2D(RegionCoord) * RegionWorldSize;
        const float TexelSize = RegionWorldSize / Resolution;

        for (int32 Y = 0; Y < Resolution; Y++)
        {
            for (int32 X = 0; X < Resolution; X++)
            {
                FVector2D WorldPos = RegionOrigin + FVector2D(X, Y) * TexelSize;

                float Height = ComputeHeight(WorldPos, WorldSeed);

                OutHeightData[Y * Resolution + X] = Height;
            }
        }
    }

private:
    static float ComputeHeight(FVector2D Pos, int32 Seed)
    {
        // Domain warping для более органичных форм
        FVector2D WarpedPos = ApplyDomainWarping(Pos, Seed);

        // Multi-octave Simplex noise (FBM)
        // Примечание: UE5 FMath::PerlinNoise2D — можно заменить на Simplex реализацию
        // для лучшего качества (меньше направленных артефактов)
        float Height = 0.f;
        float Amplitude = 1.f;
        float Frequency = 0.0005f;  // ~2км wavelength для основных гор

        for (int32 Octave = 0; Octave < 6; Octave++)
        {
            float NoiseValue = SimplexNoise2D(
                WarpedPos.X * Frequency + Seed * 0.1f,
                WarpedPos.Y * Frequency + Seed * 0.2f);

            Height += NoiseValue * Amplitude;

            Amplitude *= 0.5f;    // Persistence
            Frequency *= 2.0f;    // Lacunarity
        }

        // Нормализация в метры (например, -100 до 500м)
        return Height * 300.f + 100.f;
    }

    static FVector2D ApplyDomainWarping(FVector2D Pos, int32 Seed)
    {
        float WarpStrength = 200.f;
        float WarpFreq = 0.0003f;

        float WarpX = SimplexNoise2D(Pos.X * WarpFreq + Seed, Pos.Y * WarpFreq);
        float WarpY = SimplexNoise2D(Pos.X * WarpFreq, Pos.Y * WarpFreq + Seed);

        return Pos + FVector2D(WarpX, WarpY) * WarpStrength;
    }
};
```

### 4. VoxelPlugin2 Integration

```cpp
// Voxel Graph node: Sample Region Height
UCLASS()
class UVoxelNode_SampleRegionHeight : public UVoxelNode
{
    // В Voxel Graph:
    // 1. Получаем world position вокселя
    // 2. Определяем регион
    // 3. Сэмплируем HeightTexture
    // 4. Возвращаем SDF = WorldZ - SampledHeight
};
```

**Voxel Graph (псевдокод):**
```
WorldPosition → GetRegionUV → SampleTexture(HeightTexture) → TerrainHeight
SDF = WorldPosition.Z - TerrainHeight
Output: SDF (negative = inside terrain, positive = air)
```

### 5. Terrain Material (debug)

Для Phase 1 используем простейший материал — визуализация высоты:

```hlsl
// M_DebugTerrain — просто показывает высоту цветом

float Height = WorldPosition.Z;

// Gradient: синий (низко) → зелёный (средне) → белый (высоко)
float NormalizedHeight = saturate(Height / 500.0);  // 0-500м → 0-1

float3 Albedo = lerp(
    float3(0.2, 0.3, 0.8),   // синий (низины)
    float3(0.9, 0.9, 0.9),   // белый (вершины)
    NormalizedHeight
);

// Или ещё проще — один цвет:
// float3 Albedo = float3(0.5, 0.5, 0.5);  // серый
```

**Цель:** видеть форму terrain, не тратить время на красивые материалы.

---

## Async Baking

```cpp
void URegionManager::LoadRegion(FIntPoint Coord)
{
    // Создаём пустой RegionData
    URegionData* Region = NewObject<URegionData>(this);
    Region->Coord = Coord;
    LoadedRegions.Add(Coord, Region);

    // Запускаем baking в background thread
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, Region, Coord]()
    {
        // 1. Bake height data (CPU)
        TArray<float> HeightData;
        FHeightGenerator::BakeRegionHeight(Coord, WorldSeed, HeightData);

        // 2. Transfer to game thread for GPU upload
        AsyncTask(ENamedThreads::GameThread, [Region, HeightData = MoveTemp(HeightData)]()
        {
            // CPU cache
            Region->HeightCache = HeightData;

            // Create GPU texture
            Region->HeightTexture = CreateTexture2D(512, 512, PF_R32_FLOAT, HeightData);

            // Mark region as ready
            Region->bIsReady = true;
        });
    });
}
```

---

## Результат Phase 1

После реализации должно работать:

1. ✅ Игрок спавнится в мире
2. ✅ Terrain генерируется вокруг игрока
3. ✅ При движении — новые регионы подгружаются, старые выгружаются
4. ✅ Один и тот же seed = один и тот же terrain
5. ✅ Debug материал показывает форму terrain

**Terrain выглядит как:**
- Холмы и горы (Simplex noise)
- Органичные формы (domain warping)
- Однотонный/gradient цвет (debug визуализация)

**Чего НЕТ (Phase 2+):**
- Биомов и красивых материалов
- Рек и озёр
- Деревьев и растительности
- Деревень и дорог
- Пещер и данжей

---

## Валидация

### Тесты

1. **Determinism test**: Загрузить регион дважды с одним seed → побитовое совпадение
2. **Streaming test**: Ездить по миру 10 минут → нет memory leaks, нет hitches
3. **Performance test**: Baking одного региона < 500ms на background thread
4. **Visual test**: Terrain имеет плавные формы, нет артефактов на границах регионов

### Метрики

| Метрика | Target |
|---------|--------|
| Region bake time | < 500ms |
| HeightTexture memory | 1 MB per region (512² × 4 bytes) |
| Max loaded regions | ~25 (5×5 grid) |
| Total terrain memory | ~25 MB |

---

## Переход к Phase 2

Когда Phase 1 стабилен:

1. **Phase 1b**: Добавить биомы и terrain материалы
2. **Phase 2a**: Добавить реки (river preprocessing, valley carving)
3. **Phase 2b**: Добавить PCG растительность
4. **Phase 2c**: Добавить settlements и дороги
5. **Phase 3**: Добавить данжи и геологические слои
