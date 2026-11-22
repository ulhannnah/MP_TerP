# MP_TerP S32 Design Studio 통합 가이드

**대상**: `jun_GPT` · 소프트웨어 구현 완료 범위 확인용 문서  
**검증**: 기존 4종 호스트 테스트 PASS 기록은 있으나, 신규 `BoardSystem` 및 MCU 주변장치 소스의 실제 S32DS 컴파일/보드 검증은 아직 수행하지 않음

## 1. 아키텍처

```text
BoardSystem / BoardSystemConfig (핀·클록·채널·IRQ 설정은 사용자)
  ├── TimerManager (LPIT0 선택 채널, 1ms Tick)
  ├── Servo (FTM PWM, 인스턴스 전용)
  ├── UltrasonicPort (별도 FTM Input Capture + LPIT One-shot)
  ├── StepMotor + StepperController (GPIO + LPIT)
  ├── ADCChannel (ADC0/1 + ADC 채널)
  ├── OLED (선택형 LPI2C + SSD1306)
  ├── GpioButton / GpioOutput (선택형 버튼/LED)
  └── App
      ├── RadarScan FSM
      ├── RadarFrame (편도 스캔 버퍼)
      ├── RadarProcessing (각도별 이동평균, 배경 차분, 검출, 5-ID 추적)
      ├── TargetSelector + AimController
      ├── RadarRenderer → OLED
      ├── LcdStatus → 선택형 LCD 전송 콜백
      └── Launcher → LED 표시 전용 시뮬레이션
```

모듈 설정에 **하드웨어 채널이나 핀을 고정하지 않음**. 다만 FTM마다 CNT/MOD/Prescaler를 공유하기 때문에 Servo와 Ultrasonic은 **서로 다른 FTM 인스턴스**를 사용해야 함.

## 2. S32DS 프로젝트 구성

1. `src/header/`를 include 디렉터리에 추가.
2. `src/*.c`를 S32DS 프로젝트 소스에 복사/추가. 테스트 폴더의 `device_registers.h`를 MCU 프로젝트에 넣지 말 것.
3. S32DS가 제공하는 S32K144 `device_registers.h`, CMSIS, startup/interrupt vector 파일을 사용.
4. 기존 프로젝트에 `main()`이 있으면 그대로 사용. 원한다면 `main_example.c`에 `MP_TERP_USE_EXAMPLE_MAIN`을 정의하고 사용자 구현 `MP_TERP_GetBoardConfig()`를 연결.
5. `radar_renderer.c` 및 `coordinate_math.c`는 `sinf/cosf`를 사용하므로 링커에 `-lm` 필요할 수 있음.
6. 전체 시스템 객체(`BoardSystem`)는 SRAM 크기가 크므로 **정적 전역 변수**로 선언할 것. ISR/메인 루프 콜스택에 할당하지 말 것.

## 3. 보드 설정이 반드시 필요한 자원

| 항목 | 설정 위치 | 주의 |
|---|---|---|
| LPIT 클록/시스템 틱 | `BoardSystemConfig.timer` | 주파수와 PCS 일치 |
| 스텝 GPIO 4개 | `stepper_pins` | 실제 모터 드라이버 핀/전원 확인 |
| 스텝 타이머 | `stepper.timer_channel` | 시스템 Tick/Trigger와 다르게 |
| 스텝 방식·감속비 | `stepper_mode, stepper.steps_per_revolution` | 실제 회전 수 보정 |
| Servo FTM/PWM/핀 | `servo` | 실제 FTM Pin MUX와 20ms 파형 확인 |
| Echo FTM/핀/클록 | `ultrasonic` | Servo와 다른 FTM 사용 |
| Trigger GPIO/LPIT | `ultrasonic` | Echo 신호 전압 허용 확인 |
| ADC 인스턴스/채널 | `adc` | 아날로그 핀 MUX, Vref |
| OLED 버스·핀 | `custom_oled_port, oled_port` | 미설정 시 기존 LPI2C0 PTA2/3 기본값 |
| Text LCD | `lcd_write` 콜백 | LCD 실물 규격 확인 후 구현 |
| 버튼·LED | `use_gpio_button/led`, `button_gpio/led_gpio` | PORT IRQ 별도 NVIC 설정 |

## 4. 타이머 채널 배정 예시 (실물 확정 전)

```c
BoardSystemConfig cfg = {0};
cfg.timer.pcc_clock_source=6u;  /* SPLLDIV2: 기존 clocks_and_modes 기준 */
cfg.timer.clock_hz=40000000u;
cfg.timer.tick_channel=TIMER_CH3;
cfg.stepper.timer_channel=TIMER_CH0;
cfg.ultrasonic.trigger_timer_channel=TIMER_CH1;
/* CH2 remains available. Pins/FTM/ADC/LCD/OLED/LED/etc are still required. */
```

모터 원점은 `BoardSystem_Init()`에서 임의로 설정하지 않음. 물리적 기준 확인 후 `StepperController_SetHome(&board.stepper, angle_mdeg)`을 호출해야 함. 엔코더가 없으면 스텝 누락은 감지할 수 없음.

## 5. Echo 및 버튼 IRQ 연결

FTM Input Capture는 S32K144 기준 **채널쌍 IRQ**를 사용함. 예를 들어 해당 보드에서 Echo에 FTM0_CH6을 선택했고 스타트업 벡터가 이를 지원한다면 다음과 같이 연결 가능.

```c
/* 사용자 S32DS 프로젝트의 단일 IRQ 소유 파일에 구현 */
extern BoardSystem g_board;
void FTM0_Ch6_Ch7_IRQHandler(void) {
    BoardSystem_OnEchoCaptureIrq(&g_board);
    /* 동일한 IRQ 벡터를 다른 채널과 공유하는 경우 별도 분배 필요 */
}
```

버튼이 있는 PORTx의 IRQ도 `BoardSystem_OnFireButtonPortIrq(&g_board)`를 호출해야 함. **각 IRQ 번호·이름 및 NVIC 활성화는 사용하는 S32DS SDK/보드의 startup 파일에서 확인**해야 함. 이 저장소는 모든 FTM/PORT IRQ 핸들러를 강제 정의하지 않음.

FTM capture 어댑터는 측정 시작 시 16비트 카운터를 0으로 재설정하고, Timeout까지 랩어라운드되지 않는 Prescaler를 선택함. 1MHz로 고정되는 것이 아니라 선택된 FTM 입력 클록에 맞춰 CnV를 microsecond로 환산함. 실제 파형 검증 필요.

## 6. App 파라미터

30~150도, 10/15/30도 등 스캔 간격, 서보 정착시간, 초음파 Timeout, 감지 임계값, ADC 히스테리시스, 화면 주기, 추적 유효 기간은 `AppConfig`에서 설정한다. **초기 한 편도 스캔은 자동 배경 캘리브레이션**으로 처리하고, 재보정은 `App_RequestBackgroundCalibration()`으로 요청 가능.

`App_Update()`는 메인 루프에서 빈번히 호출하며, 디스플레이 전송은 느리거나 Blocking일 수 있음. 높은 갱신 주기로 테스트할 때 모터 지터와 Echo Timeout 발생률을 반드시 측정.

## 7. 호스트 테스트 (수동)

```bash
bash tests/run_host_tests.sh
bash tests/run_stepper_tests.sh
bash tests/run_servo_tests.sh
bash tests/run_pipeline_tests.sh
bash tests/run_integration_tests.sh
```

GitHub Actions는 **수동 실행 전용**이다. 추가된 5번째 통합 테스트 스크립트는 이번 작성 시점에 실제 GCC 실행을 확인하지 못했으므로 결과를 PASS로 표기하지 않는다.

## 8. 알려진 미완성·실기기 의존 사항

- 실제 Text LCD의 전송 방식은 미확정. `LcdStatus`는 16x2 텍스트 생성·변경분 갱신까지 담당하고, 실제 HD44780 4-bit 병렬/LPI2C 백팩 등 전송은 `lcd_write`에서 구현 필요
- `ADCChannel`에서 ADC 채널에 대응하는 핀의 아날로그 MUX 설정과 보드 Vref 확인 필요
- S32DS에서 FTM 매크로, PORT IRQ 벡터, `DWT` 레지스터 및 NVIC 설정 검증 필요
- FTM 입력 캡처와 메인 루프의 `Ultrasonic` 상태 공유 시 경쟁 조건·인터럽트 타이밍 검증 필요
- 배경 자동 보정은 간단한 1회 스냅샷으로, 대상 존재 시 잘못 학습할 수 있음
- 현재 Tracker는 각도/거리 기반 매칭. 표적 교차/가림 및 속도 모델이 추가되어야 정확한 다중 표적 성능 평가 가능
- DSP 최적화 FIR, eDMA 표시 전송, WDOG 복구는 선택적 성능 고도화로 남겨둠

## 9. 보드 최초 검증 순서

1. Clock, LPIT Tick, 선택 채널 IRQ
2. Servo PWM 펄스 주기/폭, 실제 서보 기계적 한계
3. Stepper 코일 순서, 회전·원점 확인
4. Trigger GPIO 10us, Echo FTM capture 및 Timeouts
5. ADC 채널과 Vref
6. OLED/LCD/버튼/LED
7. Frame → Tracker → Aim 연결
8. DWT로 지터·CPU 점유율 및 필터 최적화 비교

실제 장비 없이 GPIO Echo 5V 출력을 S32K144에 바로 연결하지 말 것. **센서 모델의 출력 전압과 MCU 핀 절대 최대 정격**을 먼저 확인할 것.
