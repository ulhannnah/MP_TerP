# Stepper Controller — PC 검증

```bash
bash tests/run_stepper_tests.sh
```

이 테스트는 `StepperController`를 실제 GCC로 컴파일하고, `TimerManager`와 `StepMotor`는 모의 구현으로 연결합니다. 따라서 **컨트롤러의 상태 전이/방향/위치 계산은 확인하지만 실제 LPIT 타이밍과 GPIO 신호는 검증하지 않습니다**. 다른 기존 LPIT 테스트는 `bash tests/run_host_tests.sh`로 별도 실행합니다.

## 사용 순서 (예시: CH1)

```c
StepMotor motor;
StepMotorPin pins[4] = { /* MCU/확장 보드 실제 핀 설정 */ };
StepperController controller;
StepperControllerConfig cfg = {
    &motor, TIMER_CH1, 2000u, 4096u, -90000, 90000
};
TimerManagerConfig tc = { 6u, 40000000u, TIMER_CH3 };

TimerManager_Init(&tc);
StepMotor_Init(&motor, pins, delay_ms); /* 기존 드라이버는 콜백 필수 */
StepperController_Init(&controller, &cfg);
StepperController_SetHome(&controller, 0); /* 기구적으로 원점 확인 후 */
StepperController_SetTargetAngle(&controller, 45000);
```

`4096u`는 예시일 뿐 **실제 출력축 감속비와 스텝 방식에 맞춰 보정해야 합니다.** 원점 설정은 물리적 홈 탐색이 아니라 소프트웨어의 기준점 선언입니다. 엔코더가 없으므로 누락된 스텝은 검출할 수 없습니다. 메인 루프에서만 `SetTargetAngle`, `SetHome`, `Stop` 등을 호출하고 Timer Manager와 동일 LPIT 채널을 다른 모듈에서 사용하지 않아야 합니다.

**주의:** 현재 테스트는 기존 드라이버의 동작을 별도로 모의하므로 S32DS 크로스 컴파일 및 실제 하드웨어 검증이 남아 있습니다.
