# S32 Design Studio — 테스트 main 빌드 절차

## 목적

`src/main.c`는 **하드웨어를 구동하지 않는 SDK 컴파일·링크 및 소프트웨어 Smoke Test 전용**입니다. 기존 `src/main_example.c`는 `MP_TERP_USE_EXAMPLE_MAIN`을 정의하지 않으면 `main()`을 제공하지 않습니다.

### 1단계: S32DS에 소스 추가

1. 프로젝트의 MCU를 **S32K144**로 설정하고, SDK/CMSIS/Startup/Linker 파일이 포함된 기존 S32DS 프로젝트를 사용합니다.
2. `src/header`를 include 경로에 추가하고, `src/*.c`를 프로젝트 소스에 추가합니다.
3. 기존 S32DS 예제 프로젝트에 `main.c`가 이미 있으면 **기존 `main.c`를 빌드에서 제외**합니다. `main()`은 정확히 하나만 있어야 합니다.
4. `MP_TERP_USE_EXAMPLE_MAIN`은 **정의하지 않습니다**.
5. `tests/device_registers.h`, `tests/stepper_mock/device_registers.h`, `tests/servo_mock/device_registers.h` 등 모의 레지스터 헤더는 S32DS include 경로에 넣지 않습니다.
6. `radar_renderer.c`와 `coordinate_math.c`에서 `sinf/cosf`를 사용하므로 링커에 `-lm`이 필요할 수 있습니다.

### 2단계: 빌드

전체 `src/*.c` 파일을 한 번에 포함한 빌드는 새로 추가된 MCU 포트 코드까지 컴파일하므로, SDK의 `FTM_*`, `ADC_*`, `LPI2C_*`, `DWT_*` 매크로 불일치가 있을 수 있습니다. 발생한 오류의 **첫 번째 원인부터** 수정해야 합니다.

이 테스트 메인은 **하드웨어를 초기화하지 않습니다.** 실행하는 것은 `RadarProcessing`, `TargetSelector`, `TaskScheduler`, `DSPFIR`, `CoordinateMath`의 기본 검증입니다.

### 3단계: 디버거에서 결과 보기

`main()` 또는 `MP_TERP_TestMain_Run()`에 브레이크포인트를 걸고 실행한 뒤 다음 전역 변수를 살펴보세요.

| 변수 | 성공 값 | 설명 |
|---|---|---|
| `g_mp_terp_smoke_signature` | `0x50415353` | ASCII `PASS` |
| `g_mp_terp_smoke_result` | `0` | 0이 아니면 실패한 시험 단계 |
| `g_mp_terp_smoke_stage` | `6` | 마지막 완료 단계 |

실패 시 signature는 `0x4641494C` (`FAIL`)입니다. 초기 `0x4D505450`는 실행 중임을 뜻합니다.

단계는 다음과 같습니다.

- 1: RadarProcessing/프레임·추적 초기화
- 2: ADC TargetSelector 논리
- 3: Cooperative TaskScheduler 실행 횟수
- 4: Q15 FIR 참조 연산
- 5: FPU 좌표 변환
- 6: 테스트 완료

### 주의

- 이 메인으로 **실제 모터나 센서의 동작은 검증되지 않습니다.** 실행 도중 핀 출력 설정이나 클록 재설정을 하지 않습니다.
- `BoardSystem_Init()` 호출은 실제 핀/채널/전원 안전성이 확인된 다음 단계에서 활성화합니다.
- 오류가 나면 **Build Console의 첫 번째 error 및 해당 파일/라인**을 알려주시면 SDK 설정에 맞게 수정할 수 있습니다.
- `main_example.c`는 전 기능 가동용 예시이며, 빌드 테스트와 혼동하지 않도록 기본 비활성 상태로 유지합니다.
