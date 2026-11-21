# Servo PWM 드라이버 — 설계 및 검증

## 목적

FTM0~FTM3 인스턴스와 채널, PORT 핀·ALT, 클록 입력·PWM 주기·펄스폭·각도 범위를 설정 구조체로 선택합니다. PWM은 FTM 하드웨어가 계속 출력하며 `Servo_SetAngle()`은 비교값(`CnV`)만 갱신합니다. ISR/Blocking Delay 없음.

## 사용 예시 (실제 핀 MUX 미확정)

```c
Servo s;
ServoConfig cfg = {
    .ftm = FTM1,
    .channel = 1u,
    .port = PORTA,
    .port_pcc_index = PCC_PORTA_INDEX,
    .pin = 1u,
    .pin_alt = 2u,             /* 반드시 보드의 Pin MUX로 재확인 */
    .clock_pcs = 6u,           /* SPLLDIV2: clocks_and_modes 기준 40 MHz */
    .clock_hz = 40000000u,
    .period_us = 20000u,
    .minimum_pulse_us = 1000u,
    .maximum_pulse_us = 2000u,
    .minimum_angle_mdeg = 0,
    .maximum_angle_mdeg = 180000,
    .initial_angle_mdeg = 90000
};
Servo_Init(&s, &cfg);
Servo_SetAngle(&s, 30000); /* 30.000 degrees */
```

**이 값들은 예시이며 실제 PTA1과 FTM1_CH1의 ALT 매핑을 확인한 결과가 아닙니다.** 실제 서보 허용 펄스 폭도 해당 모델 데이터시트를 따르세요. 출력축의 실각은 엔코더가 없으면 확인할 수 없습니다.

## 필수 제약

- **FTM 인스턴스 전체 전용**: MOD, CNT, prescaler가 해당 인스턴스의 모든 채널에서 공유되므로 다른 모듈(특히 초음파 Input Capture)이 동일 인스턴스를 초기화하지 않아야 합니다.
- 동일 FTM에 두 번째 Servo 객체를 생성하는 작업은 거부합니다. 다른 채널의 동일 주기 PWM을 동시에 사용하려면 별도의 FTM 공유 관리 계층이 필요합니다.
- `Servo_Stop()`은 FTM 클록을 정지하지만 출력 핀을 무조건 LOW로 만드는 보장은 없습니다.
- `Servo_Deinit()`은 인스턴스 예약을 해제하지만 핀 MUX/전원까지 초기화하지 않습니다.
- `Servo_SetAngle()`은 하드웨어 목표 펄스를 갱신할 뿐 정착 시간 대기/이동 완료 확인을 하지 않습니다. 이 책임은 Radar Scan FSM에 있습니다.
- 올바른 `clock_hz`와 해당 PCS를 확인해야 합니다.
- 16비트 MOD 범위에 맞춰 분주비(1,2,4,8,16,32,64,128)를 자동 선택합니다.

## PC 단위 테스트

```bash
bash tests/run_servo_tests.sh
```

검증 항목: FTM/채널 선택, MOD 및 분주 계산, 1.0/1.5/2.0ms 비교값, 각도 변환 및 범위 오류, 중복 점유, Stop/Start/Deinit, 다른 소스 클록. **PC mock 테스트만 통과**했으며 실제 S32DS 빌드·오실로스코프 파형·실물 서보 동작은 미검증입니다. `tests/servo_mock`은 MCU 빌드에 포함하지 않습니다.
