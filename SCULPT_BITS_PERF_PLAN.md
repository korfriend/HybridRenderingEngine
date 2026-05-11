# SCULPT_BITS 성능 개선 플랜 (A + B)

## 0. 배경 요약

- 현상: `VR_MODE == 2` (transparency, alpha-modulated, ERT 실질 미작동) 에서 `SCULPT_BITS == 1` 일 때 성능 저하가 매우 큽니다.
- 원인 (요점):
  1. `sculpt_bits` 가 `Buffer<uint>` (R32_UINT typed buffer) 라 GPU 3D 텍스처 캐시의 공간 인접성 이점을 못 받습니다. 특히 Z 방향으로 진행하는 레이는 매 샘플마다 캐시 라인을 새로 가져옵니다.
  2. `LOAD_BLOCK_INFO` 가 보는 `tex3D_volblk` 는 OTF 활성 블록만 인지하고 sculpt 정보를 모릅니다. 통째로 깎인 블록도 inner loop 를 그대로 돕니다.
  3. ERT 가 사실상 작동하지 않으므로 위 두 비용이 매 픽셀 × 전체 `num_ray_samples` 만큼 쌓입니다.

- 본 플랜의 범위:
  - **A**. `sculpt_bits` 자원 타입을 `Buffer<uint>` → `Texture3D<uint>` 로 변경하여 캐시 적중률을 향상시킵니다.
  - **B**. `UpdateOtfBlocks` 에서 sculpt_bits 정보를 OTF 블록 태깅에 반영하여 통째로 sculpt 된 블록을 `LOAD_BLOCK_INFO` 단계에서 스킵 가능하게 합니다.
  - C, D (단락 평가, 정수 연산 최적화) 는 본 플랜의 후속 작업으로 남겨둡니다.

---

## 1. 영향 범위

수정 대상 파일들입니다.

| 파일 | 수정 성격 |
|---|---|
| `renderer/VolumeRenderer.cpp` | 자원 업로드 호출부 (`UpdateCustomBuffer` → `UpdateCustomTexture3D` 또는 신규 helper) 교체, 시그니처에 sculpt 정보 추가 |
| `CurvedSlicerVR.cpp` | 동일한 패턴이 있으면 동기화 |
| `gpu manager/gpures_helper.cpp` | `UpdateOtfBlocks` 에 SCULPT_BITS 경로 추가, Texture3D 업로드 헬퍼 추가 (또는 기존 헬퍼 활용) |
| `gpu manager/gpures_helper.h` | 시그니처 변경 사항 반영 |
| `hlsl/dvr/DvrCS.hlsl` | `Buffer<uint> sculpt_bits` 선언을 `Texture3D<uint> sculpt_bits_tex` 로 변경, 5곳의 SCULPT_BITS 분기에서 인덱싱 로직 교체 |
| `hlsl/dvr/DvrSampleTest.hlsl` | (있으면) 동일 패턴 동기화 |
| `ShaderCompile.bat`, `ShaderCompile_4_0.bat` | (필요 시) 매크로 정의는 그대로 유지, 헤더 include 만 확인 |

핵심 데이터 흐름은 다음과 같습니다.

```
CPU: SCULPT_PACKEDBITS (vector<uint32_t>, 한 비트 = 한 voxel)
        │
        ├─[A] Texture3D<R32_UINT> 으로 업로드 (32 voxel(X) × 1 × 1 per texel)
        │       └─ 셰이더는 sculpt_bits_tex.Load(int4(x/32, y, z, 0))
        │
        └─[B] UpdateOtfBlocks 안에서 블록 단위 AND 검사
                └─ 통째로 sculpt 된 블록 → tag_blks[blk] = 0 (volblk 0 으로 다운로드)
```

---

## 2. A안: `sculpt_bits` Texture3D 화

### 2.1 자료 구조 결정

- 포맷: `DXGI_FORMAT_R32_UINT` (1 texel = 32 bit = 32 voxel along X).
- 차원: `texW = ceil(original_size.x / 32)`, `texH = original_size.y`, `texD = original_size.z`.
- 인덱싱:
  - `tx = voxel_id.x >> 5` (= /32)
  - `bit_in_word = voxel_id.x & 31`
  - `word = sculpt_bits_tex.Load(int4(tx, voxel_id.y, voxel_id.z, 0))`
  - `visible = !(word & (1u << bit_in_word))`

X 방향 32 voxel 이 하나의 texel 에 들어가므로 X 인접성은 자동으로 양호하고, Y/Z 인접성은 3D 텍스처 캐시가 처리합니다.

> 옵션 변형: 4×4×2 = 32 voxel 블록 packing 으로 3D 등방 캐시 적중을 더 끌어올릴 수 있으나, CPU 측 pack 포맷까지 바꿔야 해서 1차 작업에서는 보류합니다. 향후 측정 결과에 따라 별도 작업 항목으로 분리합니다.

### 2.2 자원 차원 정합 확인

`g_cbVobj.vol_original_size` 가 CPU 측의 원본 voxel 차원과 일치하는지 확인이 필요합니다 (Texture3D X 차원이 `ceil/32` 라서 셰이더 쪽에서 원본 X 를 알아야 비트 위치 계산이 맞습니다). 현재 코드에서 이미 `vol_original_size` 를 SCULPT_BITS 경로가 사용 중이므로 그대로 활용 가능합니다.

### 2.3 CPU 업로드 변경

- 현재: `grd_helper::UpdateCustomBuffer(gres_sculpt_bits, sculptBitPackedObj, "SculptPackedBits", data, size, DXGI_FORMAT_R32_UINT, 4);` (`renderer/VolumeRenderer.cpp:814`)
- 변경: `Texture3D<R32_UINT>` 업로드용 helper 신설 (또는 기존 3D 텍스처 helper 재사용).
  - `gres_sculpt_bits.options["FORMAT"] = DXGI_FORMAT_R32_UINT`
  - 3D 차원은 위 2.1 의 `texW, texH, texD`
  - 데이터는 기존 `vector<uint32_t>` 의 메모리 레이아웃을 row-major 3D 텍스처로 그대로 매핑. CPU 측 packing 이 `bit_id = x + y * W + z * W * H` 의 비트열이라 가정하고, GPU 측 texel `(tx, y, z)` 의 32bit 가 X = 32·tx .. 32·tx+31 의 비트를 정확히 포함하도록 동일한 endian/bit-order 로 업로드해야 합니다.
  - **검증 포인트**: CPU 의 `vector<uint32_t>` packing 규약이 `bit_id % 32` 의 LSB 우선인지 확인 (현재 셰이더는 `(0x1u << mod)`, `mod = bit_id % 32` 로 가정).
- DX11 의 `Texture3D` `RowPitch`/`DepthPitch` padding 을 고려해 `Map`/`memcpy` 로 row 별 복사가 필요합니다 (한 번에 `memcpy` 불가). 이는 `UpdateOtfBlocks` 가 이미 같은 패턴으로 수행 중이라 그 코드를 그대로 참고하면 됩니다 (`gpu manager/gpures_helper.cpp:1276-1309`).
- 자원 timestamp 재사용: `sculptBitPackedObj` 의 content update time 을 GpuRes 의 dirty check 에 연동해 sculpt 가 안 바뀌면 재업로드하지 않도록 합니다.

### 2.4 셰이더 변경

`hlsl/dvr/DvrCS.hlsl` 의 5곳 (라인 210, 240, 276, 1288, 1336) 을 동일 패턴으로 교체합니다.

**선언부 (라인 14)**
```hlsl
// 변경 전
Buffer<uint> sculpt_bits : register(t7);
// 변경 후
Texture3D<uint> sculpt_bits_tex : register(t7);
```

**인덱싱 매크로화 (중복 제거)**
중복된 로직을 1곳에 모읍니다. `CommonShader.hlsl` 또는 `DvrCS.hlsl` 상단에 다음을 추가합니다.

```hlsl
bool SculptVisible(const float3 pos_sample_ts)
{
    int3 voxel_id = (int3)(pos_sample_ts * (g_cbVobj.vol_original_size - uint3(1, 1, 1)));
    uint word = sculpt_bits_tex.Load(int4(voxel_id.x >> 5, voxel_id.y, voxel_id.z, 0));
    return !(bool)(word & (1u << (voxel_id.x & 31u)));
}
```

각 호출부는 `bool visible = SculptVisible(pos_sample_ts);` 1줄로 단순화됩니다. 인라이닝은 컴파일러가 처리합니다.

> NOTE: 함수화 자체로 wave 단위 codegen 이 달라지지는 않지만, 후속으로 단락 평가(`C` 항목)를 적용할 때 분기를 깔끔하게 다룰 수 있게 됩니다.

### 2.5 검증 절차 (A)

1. 작은 테스트 케이스: sculpt 가 완전히 비어있는 상태(모든 비트 0)와 완전히 차 있는 상태(모든 비트 1) 에서 `SCULPT_BITS == 0` 결과와 픽셀 단위로 비교합니다. 비트 0 이면 렌더링 결과가 SCULPT_BITS == 0 과 동일해야 합니다.
2. 절반만 sculpt 된 케이스: 결과가 시각적으로 기존 `Buffer<uint>` 구현과 동일해야 합니다.
3. 경계 voxel: 가장자리 (`x == W-1`, `y == 0` 등) 의 voxel 이 올바르게 켜지고 꺼지는지 확인합니다.

---

## 3. B안: `UpdateOtfBlocks` 에 sculpt_bits 통합

### 3.1 개요

`gpures_helper.cpp:1208 UpdateOtfBlocks` 는 이미 `SCULPT_MASK` (8-bit Texture3D) 용 sculpt 정보를 OTF 활성 블록 태깅에 반영하는 분기를 갖고 있습니다 (라인 1253-1274). 같은 통합을 `SCULPT_PACKEDBITS` 경로에서도 수행하도록 추가합니다.

### 3.2 동작 정의

`tag_blks[blk_idx]` 는 "이 블록 안에 OTF 알파 > 0 인 voxel 이 하나라도 있으면 1, 아니면 0" 으로 OTF 단계에서 결정됩니다. 여기에 다음 조건을 AND 로 합칩니다.

> 블록 안의 모든 voxel 이 sculpt 되어 있으면 `tag_blks[blk_idx] = 0` 로 강제

여기서 "모든 voxel 이 sculpt" 의 판정은 sculpt_bits 비트 전부가 set 인 경우입니다. 부분 sculpt 는 블록 단위로는 0 으로 만들지 않습니다 (시각적 보존 우선).

### 3.3 CPU 측 변경

`UpdateOtfBlocks` 시그니처에 sculpt_bits 소스를 받도록 추가합니다.

```cpp
bool grd_helper::UpdateOtfBlocks(
    GpuRes& gres,
    VmVObjectVolume* main_vobj,
    VmVObjectVolume* mask_vobj,
    VmObject* tobj,
    const int sculpt_value,
    const std::vector<uint32_t>* sculpt_bits_packed,  // 신규
    const vmint3 sculpt_bits_dim,                      // 신규: 원본 voxel 차원 (vol_original_size)
    LocalProgress* progress = nullptr);
```

`renderer/VolumeRenderer.cpp` 와 `CurvedSlicerVR.cpp` 의 호출부도 sculpt_bits 가 존재할 때 그 포인터/dim 을 전달하도록 수정합니다 (없으면 nullptr → 기존 동작 유지).

추가 처리는 다음 형태로 SCULPT_MASK 분기와 나란히 둡니다.

```cpp
// 기존 SCULPT_MASK 분기 (line 1253~) 바로 뒤에 추가
if (sculpt_bits_packed && sculpt_bits_packed->size() > 0)
{
    const uint32_t* bits = sculpt_bits_packed->data();
    const int W = sculpt_bits_dim.x;
    const int H = sculpt_bits_dim.y;
    const int D = sculpt_bits_dim.z;

    const vmint3 blk_vol_size = volblk->blk_vol_size;
    const vmint3 blk_bnd_size = volblk->blk_bnd_size;
    const vmint3 unit         = volblk->unitblk_size; // 블록 1개가 덮는 voxel 수 (보통 8 또는 16)
    const int blk_sample_w    = blk_vol_size.x + blk_bnd_size.x * 2;
    const int blk_sample_slice= blk_sample_w * (blk_vol_size.y + blk_bnd_size.y * 2);

    for (int bz = 0; bz < blk_vol_size.z; bz++)
    for (int by = 0; by < blk_vol_size.y; by++)
    for (int bx = 0; bx < blk_vol_size.x; bx++)
    {
        const int blk_addr =
            (bx + blk_bnd_size.x) +
            (by + blk_bnd_size.y) * blk_sample_w +
            (bz + blk_bnd_size.z) * blk_sample_slice;
        if (tag_blks[blk_addr] == 0) continue; // 이미 OTF 단계에서 비활성

        // 이 블록이 덮는 voxel 영역
        const int x0 = bx * unit.x, x1 = min(x0 + unit.x, W);
        const int y0 = by * unit.y, y1 = min(y0 + unit.y, H);
        const int z0 = bz * unit.z, z1 = min(z0 + unit.z, D);

        bool all_sculpted = true;
        for (int z = z0; z < z1 && all_sculpted; z++)
        for (int y = y0; y < y1 && all_sculpted; y++)
        {
            // 한 (y,z) 라인에서 비트 32개 단위로 검사
            for (int x = x0; x < x1; )
            {
                const uint32_t bit_id_base = (uint32_t)x + (uint32_t)y * W + (uint32_t)z * (uint32_t)W * (uint32_t)H;
                const uint32_t word_idx = bit_id_base >> 5;
                const uint32_t off_in_word = bit_id_base & 31u;
                const int run = min(32 - (int)off_in_word, x1 - x);

                uint32_t mask = (run == 32) ? 0xFFFFFFFFu
                                            : (((1u << run) - 1u) << off_in_word);
                if ((bits[word_idx] & mask) != mask) { all_sculpted = false; break; }
                x += run;
            }
        }

        if (all_sculpted) tag_blks[blk_addr] = 0;
    }
}
```

체크 포인트:

- `unitblk_size` 가 voxel 단위인지 확인 (현재 코드에 `unitblk_size` 가 `cb_volume.volblk_size_ts` 계산에 voxel 단위로 사용 중이므로 OK 로 추정. 실제로는 빌드 직전에 한 번 print 로 검증).
- `tag_blks` 의 인덱스 계산이 기존 SCULPT_MASK 분기와 동일한 컨벤션 (`+ blk_bnd_size` 오프셋) 인지 동일하게 따라야 합니다.
- sculpt bit 와 OTF 블록의 시간 정합성: sculpt 변경 → OTF 블록 재빌드 트리거가 필요합니다. 현재 `_tob_time_cpu < _blk_time_cpu` 짧은 캐시 로직 (라인 1236-1244) 에 sculpt timestamp 도 함께 보도록 확장합니다. 예: `sculpt_volblk_time = max(_blk_time_cpu, sculpt_update_time)` 로 비교.

### 3.4 셰이더 측 변경 없음

`LOAD_BLOCK_INFO` / `tex3D_volblk` 경로는 그대로입니다. 통째로 sculpt 된 블록에 대해 `blk_value == 0` 이 되므로 `if (blk_skip.blk_value > 0)` 분기에서 자동으로 스킵됩니다.

### 3.5 검증 절차 (B)

1. sculpt 가 비어있을 때: `tag_blks` 가 변하지 않아야 함 (기존 동작과 동일).
2. 한 블록 전체를 sculpt 로 덮은 케이스: 해당 블록 영역에서 sample 호출이 발생하지 않는지 확인 (디버그용 `count++` 카운터를 fragment_vis 의 R 채널에 임시 출력해 시각화 가능).
3. 블록 경계 voxel 한두 개만 sculpt: 블록은 활성 유지되어야 함 → 시각적으로 sculpt 경계가 그대로 렌더되는지 확인.

---

## 4. 작업 순서

원자적이고 되돌리기 쉬운 순서로 진행하시면 됩니다.

1. **준비**
   - `vol_original_size` 가 CPU 측 sculpt_bits 의 차원과 정확히 일치하는지 한 줄 로그/`assert` 로 확인 (이후 단계 디버깅 비용 큰 게 여기서 막힘).
   - `unitblk_size` 의 실제 값(예: 8 또는 16) 확인.
2. **A안 구현**
   - 2.3 의 CPU 업로드 경로 추가 (Texture3D 헬퍼).
   - 2.4 의 셰이더 자원 선언과 5곳의 인덱싱 교체.
   - 빌드 → ShaderCompile.bat 재실행 → SCULPT_BITS == 0/1 두 케이스가 동일하게 보이는지 시각 검증.
   - 성능 측정: `VR_MODE == 2` 에서 SCULPT_BITS 1 vs 0 의 격차 변화.
3. **B안 구현**
   - 3.3 의 `UpdateOtfBlocks` 확장.
   - sculpt 변경 timestamp 가 OTF 블록 재빌드를 유발하는지 확인.
   - 시각 검증 (sculpt 전체/부분/경계).
   - 성능 측정: A 만 적용했을 때 대비 추가 개선분 측정.

각 단계 끝에 SCULPT_BITS == 0 결과와 1 결과의 픽셀/성능 비교를 남겨두시면 회귀 추적이 쉬워집니다.

---

## 5. 리스크 및 주의 사항

- **비트 순서 (endian / LSB-first) 정합**: CPU 의 `vector<uint32_t>` packing 이 `(bit_id % 32)` 위치의 LSB-first 인지 확인 필요. Texture3D 로 옮기더라도 비트 의미가 바뀌면 안 됩니다.
- **Texture3D `RowPitch` padding**: `Map`/`memcpy` 시 row 별 복사를 빠뜨리면 X 방향 일부 비트가 깨질 수 있습니다.
- **블록 단위 sculpt 판정의 보수성**: 부분 sculpt 인 경우 절대로 블록 전체를 0 으로 만들지 않는 규칙을 지키지 않으면, sculpt 경계가 거칠게 끊겨 보입니다.
- **CPU 비용**: B 의 블록 스캔은 sculpt 변경 시에만 실행됩니다. interactive sculpting 빈도에 따라 부담이 있다면, 변경된 블록 영역만 재스캔하는 dirty-region 갱신으로 좁힐 수 있습니다 (후속).
- **A 만으로 충분할 가능성**: 일부 케이스에서는 A 만 적용해도 SCULPT_BITS == 0 수준에 근접할 수 있습니다. A 측정 결과를 보고 B 우선순위를 다시 판단하시면 됩니다.
- **회귀 대상**: SCULPT_MASK == 1 경로, OTF_MASK == 1 경로, VR_MODE 0/1/3 경로는 본 작업의 직접 대상이 아니지만 `UpdateOtfBlocks` 시그니처 변경 시 호출부가 모두 컴파일되는지 확인이 필요합니다.

---

## 6. 측정 방법 제안

- 단위 시나리오:
  - VR_MODE = 2 (transparency, alpha-modulated)
  - 동일 카메라/뷰포트/볼륨
  - SCULPT_BITS = 0 vs 1 (sculpt 가 일부 영역만 적용된 상태)
- 지표: 평균/95p 프레임 시간 (ms). dispatch 기준 GPU 타임스탬프가 있으면 더 정확합니다.
- 측정 포인트:
  - 베이스라인 (현재 코드)
  - A 적용 후
  - A + B 적용 후
- 기대치: A 만으로도 큰 폭의 개선, B 로 sculpt 가 큰 영역을 덮을 때 추가 절감.
