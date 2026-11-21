# Timer Manager v0.2 — 채널 선택형 LPIT0

기존의 고정 할당(`CH0=모터 / CH1=초음파 / CH2=1ms`)을 제거했습니다. **채널은 애플리케이션이 선택**합니다.

## 사용 예시

```c
#include "clocks_and_modes.h"
#include "timer_manager.h"

static void motor_step(void *ctx) { (void)ctx; /* 짧은 ISR 작업 */ }
static void echo_trigger_off(void *ctx) { (void)ctx; /* Trigger GPIO LOW */ }

void board_start(void)
{
    TimerManagerConfig timer_cfg = { 6u, 40000000u, TIMER_CH3 };
    SOSC_init_8MHz();
    SPLL_init_160MHz();
    NormalRUNmode_80MHz();
    TimerManager_Init(&timer_cfg); /* 시스템 틱: CH3 */
    TimerManager_Configure(TIMER_CH1, 2000u, false); /* 모터: CH1 */
    TimerManager_RegisterCallback(TIMER_CH1, motor_step, 0);
    TimerManager_Start(TIMER_CH1);
    TimerManager_Configure(TIMER_CH0, 10u, true); /* Trigger 종료: CH0 */
    TimerManager_RegisterCallback(TIMER_CH0, echo_trigger_off, 0);
}
```

시스템 틱 채널을 사용하지 않으려면 `TIMER_CHANNEL_NONE`을 사용합니다. 이 경우 `TimerManager_GetTickMs()`는 0을 반환합니다.

OLED/LCD 갱신 주기는 Timer Manager에 묶지 않습니다. **애플리케이션 스케줄러가 시스템 틱을 읽어 주기적으로 갱신**합니다.

## 테스트

```bash
bash tests/run_host_tests.sh
```

`tests/device_registers.h`는 PC 테스트 전용 모의 레지스터입니다. 실제 S32DS include 경로에 추가하지 마세요.

## 주의

- `Init` 시 클록 값과 PCC PCS 선택은 **실제 clock tree와 동일해야 합니다**. Hz 값을 확인하는 하드웨어 계측 기능은 없습니다.
- CH0~CH3 IRQ 벡터는 이 모듈이 전부 소유합니다. 다른 파일에서 중복 정의하면 안 됩니다.
- 콜백은 LPIT IRQ에서 실행되므로 Blocking, OLED 전송, 긴 연산을 수행하지 않습니다.
- 이미 실행 중인 타이머에 `Start`를 호출하면 주기가 재시작됩니다. `Configure`는 기존 채널 동작을 중지합니다.
- 테스트는 호스트 모의 레지스터 기반이며 **S32DS 실기기 컴파일·파형 검증은 아직 수행되지 않았습니다**.
- 스텝 모터·서보·ADC·I2C 등 다른 드라이버에도 채널/핀 선택형 API를 동일한 원칙으로 적용할 예정입니다.
