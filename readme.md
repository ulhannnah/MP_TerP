# MP_TerP — S32K144 초음파 레이더 시스템

S32K144(Cortex-M4F) 기반으로 서보 초음파 왕복 스캔, 배경 차분, 최대 5개 표적 추적, 스텝 모터 조준, OLED/LCD 표시 및 **LED만 사용하는 가상 발사**를 구현하는 수업 프로젝트입니다.

## 현재 구현된 소프트웨어

| 계층 | 주요 파일 |
|---|---|
| 공통/하드웨어 | `clocks_and_modes`, `timer_manager`, `adc_channel`, `servo`, `step_motor`, `stepper_controller` |
| 센서 | `ultrasonic`, `ultrasonic_port_s32k144` |
| 레이더·추적 | `radar_scan`, `radar_frame`, `radar_processing`, `distance_filter`, `background_filter`, `target_detector`, `target_tracker` |
| 조작·조준 | `target_selector`, `aim_controller`, `gpio_io`, `launcher` |
| 화면 | `oled`, `oled_port_s32k144`, `radar_renderer`, `lcd_status`, 선택형 `lcd_hd44780` |
| 시스템·성능 | `app`, `board_system`, `task_scheduler`, `coordinate_math`, `dsp_fir`, `cycle_profiler` |

모든 핀/채널은 가능한 한 설정 구조체에서 지정합니다. FTM Servo와 초음파 Input Capture는 인스턴스별 카운터를 공유하므로 **서로 다른 FTM을 사용**해야 합니다.

## 먼저 읽을 문서

- [S32K144 핀 배치 및 단계별 실행·독립 센서 벤치](docs/20261124_S32K144_핀배치_단계별_실행_독립센서_벤치.md) — **내일 보드 시험 전 먼저 확인**\n- [S32DS 통합 및 검증 가이드](docs/S32DS_통합_및_검증_가이드.md)
- [레이더 처리 파이프라인 설명](docs/레이더_파이프라인_구현현황.md)
- [기능별 상세 명세](docs/세부 명세서.md)

## 로컬 GCC 수동 테스트

```bash
bash tests/run_host_tests.sh
bash tests/run_stepper_tests.sh
bash tests/run_servo_tests.sh
bash tests/run_pipeline_tests.sh
bash tests/run_integration_tests.sh
bash tests/run_math_tests.sh
```

이 테스트 중 **기존 4종**은 과거 GitHub Actions 성공 기록이 있지만, 이후 추가된 **통합·수치 연산 테스트 2종은 작성 후 미실행**입니다. GitHub Actions는 메일 알림 방지를 위해 **수동 실행 전용**으로 유지합니다.


## Python + 원본 C 레이더 시뮬레이션 (보드 불필요)

Python이 가상 센서와 이동 표적을 만들고, **실제 C Scan/Processing/Tracker 소스를 호스트 GCC 공유 라이브러리로 컴파일**해서 실행합니다. S32DS ARM 컴파일러가 아닌 PC용 `gcc`가 필요합니다.

```bash
python sim/run.py --scenario static --sweeps 12
python sim/run.py --scenario moving --sweeps 16 --plot
python sim/selftest.py
```

`--plot`은 `python -m pip install -r sim/requirements.txt`로 matplotlib 설치 후 사용합니다. `sim/results/`에 CSV·JSON·PNG 결과가 저장됩니다. [시나리오·WSL 환경·검증 범위 상세](sim/README.md)

## S32DS에서 필요한 작업

1. 실제 S32K144/실습 확장 보드의 MCU 패키지·Pin MUX·전압·LCD 규격 확인
2. 기존 S32DS 프로젝트의 SDK/CMSIS와 스타트업 IRQ 벡터 사용
3. `src/header/` 포함, `src/*.c` 소스 추가, 실제 GPIO·FTM·LPIT·ADC 자원 설정
4. `BoardSystemConfig` 구성 및 `BoardSystem_Init/Update` 연결
5. FTM 채널쌍 IRQ → `BoardSystem_OnEchoCaptureIrq`; 버튼 PORT IRQ → `BoardSystem_OnFireButtonPortIrq`
6. 기계적 원점 확인 후에만 `StepperController_SetHome` 호출
7. 보드에서 PWM·Echo·I2C·ADC·모터·LCD 동작 순차 검증

**주의:** 현재는 MCU용 전체 S32DS 빌드, 보드 실기기 검증, Pin MUX 확정, 전력/실행 성능 측정이 완료되지 않았습니다. `main_example.c`는 `MP_TERP_USE_EXAMPLE_MAIN`을 정의했을 때만 `main`을 제공하며 사용자 보드 설정 함수가 필요합니다. 실제 발사체나 고전력 장치를 제어하지 않습니다.
\n## 보드 단계별 실행 메인 (2026-11-24)\n\n기존 `src/main.c`는 주변장치를 건드리지 않는 기본 Smoke Test로 유지한다. `src/main_stage.c`는 `MP_TERP_STAGE_MAIN` 및 `MP_TERP_STAGE=1..5` 심볼로 선택할 수 있다. Stage 1=Tick, Stage 2=Servo, Stage 3=고정 초음파, Stage 4=가상 Echo+실제 C App, Stage 5=전체 보드 연동(실제 각도 결합 확인 필요). 실물 핀맵 확인 전에는 `src/board_config_user_template.c`가 초기화 실패로 안전하게 차단한다.\n\n**중요:** 현재 초음파 센서와 서보가 따로 고정되어 있으므로 서보 각도를 초음파 관측 방향으로 사용하면 안 된다. 실제 센서 방향을 확인하지 않은 상태에서 Stage 5를 활성화하지 않는다.\n