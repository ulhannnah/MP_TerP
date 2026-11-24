# S32K144 보드 실습 코드 따라 하기

보드를 켜서 **OLED 확인 → 초음파 스캔 → 물체 5개 확인** 순서로 진행한다. 코드는 `jun_GPT`의 `33ed0f1`을 기준으로 한다. 주차별 시간표와 역할은 계획서에서 확인한다. [3주 보드 실습 계획서](./MP_TerP_Jun명세_3주_보드실습계획.md)

## 1. 파일을 어디에 넣나

아래 코드 블록마다 파일 하나를 만든다. 같은 파일을 앞 문서에 따라 만들어 뒀다면 이번 문서의 코드로 교체한다. S32 Design Studio의 헤더 검색 경로에 `src/header`를 추가한다.

| 번호 | 저장할 위치                    | 하는 일                                                                                        |
| ---- | ------------------------------ | ---------------------------------------------------------------------------------------------- |
| ①   | `src/header/week_oled.h`     | 화면에 `OLED OK`와 점을 띄운다.                                                              |
| ②   | `src/header/week_frame.h`    | 스캔 한 번이 끝날 때마다 물체 수와 ID를 기록한다.                                              |
| ③   | `src/week_survey.c`          | 센서를 돌리고 거리값을 읽어 화면에 그린다. 설정을 켜면 가변저항과 LCD도 사용한다.              |
| ④   | `src/week_distance_filter.c` | 최근 거리값을 평균 내서 튀는 값을 완화한다. 일반 C와 DSP로 같은 평균을 계산해 시간을 비교한다. |
| ⑤   | `src/header/week_xy.h`       | 물체의 각도와 거리를 X·Y 위치로 바꾼다. 정수 계산과 FPU 계산을 비교할 때 사용한다.            |

## 2. 주차별로 무엇을 실행하나

### 1주차 · 보드와 부품을 하나씩 확인

1. 기존 `main.c`로 빌드해 실행한다. 디버거에서 `g_mp_terp_smoke_signature=0x50415353`, `g_mp_terp_smoke_result=0`, `g_mp_terp_smoke_stage=6`을 확인한다.
2. 보드 시간, 서보 각도, 초음파 거리를 순서대로 확인한다. 기존 `main_stage.c`에서 `MP_TERP_STAGE_MAIN`을 켜고 `MP_TERP_STAGE`를 **1, 2, 3**으로 한 번씩 바꿔 각각 빌드하고 올린다.
   - **1** — `g_stage_tick_ms`가 증가하는지 본다.
   - **2** — `g_stage_servo_request_mdeg`를 바꾸면 서보가 움직이는지 본다. `90000`은 90°다.
   - **3** — 물체를 센서 앞에 놓고 `g_stage_last_mm`에서 거리를 읽는다. `g_stage_samples`, `g_stage_valid`, `g_stage_invalid`는 전체·유효·무효 측정 횟수다.
3. OLED는 ③으로 확인한다. `WEEK_SURVEY_MAIN`, `WEEK_OLED_ONLY=1`을 켜고 올린다. 화면에 `OLED OK`와 점이 뜨고 `g_week_oled_ok=1`이면 통과다.

### 2주차 · 센서를 돌려 물체 1~3개 확인

1. 센서를 서보에 고정한다. ③의 설정을 `WEEK_OLED_ONLY=0`, `WEEK_LIMIT=3`으로 바꾸고 실제 Echo 인터럽트 이름을 `WEEK_ECHO_IRQ`에 넣는다. `WEEK_SURVEY_MAIN`은 켜 둔다.
2. 시험할 물체를 치운 채 프로그램을 시작한다. 센서가 **한쪽 끝에서 반대쪽 끝까지** 이동하는 첫 편도에서 벽이나 배경판까지의 거리를 기억한다. 다음 편도부터 물체를 1개, 2개, 3개씩 놓고 화면에 표시되는 위치와 디버거에 기록된 ID를 비교한다.
3. 가변저항과 문자 LCD가 준비되면 `WEEK_PANEL`을 추가한다. 가변저항을 돌려 OLED의 강조 위치와 LCD의 ID가 같이 바뀌는지 본다.

### 3주차 · 물체 5개와 DSP·FPU 비교

1. 2주차와 같은 구성에서 `WEEK_LIMIT=5`로 바꾸고 다시 올린다. 빈 배경부터 측정한 뒤 물체 4개, 5개 순으로 늘린다.
2. 물체 5개를 배치한 다음 시작하는 **연속 5편도**를 확인한다. 매번 `fresh=5`인지 보고, 화면과 거리·각도 기록을 대조해 **같은 물체에 같은 ID**가 유지되는지 확인한다.
3. 필터 시간을 비교할 때는 `WEEK_PROFILE_FILTER`를 켠다. `WEEK_FILTER_DSP`가 없는 상태로 일반 C를 측정한 뒤 이 설정을 추가해 DSP를 측정한다. 같은 물체 배치, 평균에 쓰는 샘플 개수, 컴파일 최적화 설정을 사용한다.
4. X·Y 계산을 비교할 때는 `WEEK_XY_BENCH`를 켠다. 같은 물체의 정수 좌표와 FPU 좌표를 읽고, 각 계산에 걸린 사이클을 기록한다.

## 3. 빌드하기 전에 맞출 설정

### 어느 파일로 시작하는지 선택

- **기존 기본 검사·Stage 1~3**을 할 때는 기존 거리 필터 `distance_filter.c`를 사용하고 ④를 빌드에서 제외한다. 기존 기본 검사와 Stage 중 하나를 선택한다.
- **③으로 OLED·센서 실습**을 할 때는 `WEEK_SURVEY_MAIN`을 켜고 기존 `src/main.c`를 빌드에서 제외한다. `MP_TERP_STAGE_MAIN`과 `MP_TERP_USE_EXAMPLE_MAIN`도 해제한다. ③이 프로그램의 시작점이다.
- **③으로 센서를 실행**할 때는 ④를 빌드에 넣고 기존 `distance_filter.c`를 제외한다. 두 파일은 같은 이름의 필터 함수를 제공한다. OLED 단독 시험은 두 필터 중 하나만 빌드에 둔다.

### 실제 배선에 맞춰 채우는 값

| 구성          | 채울 값                                                                                                                                                                                                                                                                                                                                                                                            |
| ------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 공통          | `board_config_user.c`에 `MP_TERP_GetBoardConfig`를 한 번만 정의한다. 원본 템플릿은 Stage용이고 기본값으로 실패를 반환한다. `g_week_error=1`이면 해당 함수가 보드 설정을 돌려주는지 본다.                                                                                                                                                                                                     |
| OLED          | 연결한 화면에 맞춰 `custom_oled_port`와 `oled_port`를 정한다.                                                                                                                                                                                                                                                                                                                                  |
| 센서          | `timer`, `servo`, `ultrasonic`을 실제 핀과 타이머에 맞춘다. `application`에는 시작·끝 각도, 각도 간격, 서보 정착 시간, Echo 대기 시간, 평균 샘플 수, 배경 차이 기준, 후보를 묶는 기준, ID 매칭·유실 조건, 화면 표시 거리를 넣는다. 30°~150°를 5° 간격으로 재면 25칸이다. 평균은 최근 3개부터 시험한다.                                                                                |
| Echo 인터럽트 | ③의 `WEEK_ECHO_IRQ`를 실제 FTM 채널쌍에 맞춘다. 스타트업에 등록된 인터럽트 함수에서 `MP_TERP_StageEchoCaptureIrq()`를 호출한다. Stage 3에서는 프로젝트가 인터럽트를 활성화하고, ③에서는 초기화 뒤 코드가 활성화한다. 스캔의 Echo 대기 시간은 센서 자체 제한보다 길게 잡는다.                                                                                                                 |
| 가변저항·LCD | `WEEK_PANEL`을 켜기 전에 ADC 핀을 아날로그 입력으로 맞추고 실제 채널, 선택 경계 여유값, `adc_max=4095`를 설정한다. 프로젝트에서 `bool Week_BoardLcdInit(void)`, `cfg.lcd_write`, `cfg.user`를 준비한다. ③은 클록을 설정한 뒤 LCD 초기화 함수를 호출한다. 4비트 병렬 1602는 기존 `LCDHD44780_Init`과 `LCDHD44780_WriteLine`을 쓸 수 있다. I²C 모듈은 연결한 모델의 드라이버를 쓴다. |
| FPU           | `WEEK_XY_BENCH`를 켜기 전에 Cortex-M4 / `fpv4-sp-d16` 설정, 전체 프로젝트의 동일한 float ABI, 시작 코드의 FPU 사용 권한(CPACR의 CP10/CP11), `mp_trig.c`와 `cycle_profiler.c`가 준비됐는지 본다.                                                                                                                                                                                            |

### S32 Design Studio에서 보드에 올리기

1. 사용할 심볼을 프로젝트 빌드 설정에 넣는다. 끌 심볼은 목록에서 뺀다. 특히 `WEEK_FILTER_DSP`는 `=0`으로 바꾸는 대신 목록에서 제거해야 일반 C로 빌드된다.
2. **Debug_RAM → Build Project**로 빌드한다. **Run → Debug Configurations → GDB PEMicro Interface Debugging**을 연다.
3. 방금 만든 ELF와 OpenSDA로 연결한 보드를 선택하고 **Debug → F8**로 실행한다. 설정을 바꾸거나 전원을 다시 연결했으면 새 ELF를 올린다.

## ① week_oled.h · 화면 연결 확인

화면 연결을 확인할 때 호출하는 함수다. ③이 이 함수를 호출해서 화면을 초기화하고 글자와 점을 띄운다.

```c
#ifndef WEEK_OLED_H
#define WEEK_OLED_H
#include "oled.h"

static inline int Week_OledCheck(void)
{
    int status = OLED_Init();
    if (status != 0) return status;
    OLED_Clear();
    OLED_DrawString(0, 0, "OLED OK", 1u, OLED_WHITE);
    OLED_FillRect(62, 32, 3, 3, OLED_WHITE);
    return OLED_Update();
}
#endif
```

## ② week_frame.h · 스캔 결과 기록

센서가 한쪽 끝에 도착할 때마다 측정 결과를 한 줄로 정리한다. `valid`는 이번에 거리를 읽은 각도 칸 수, `known`은 배경을 학습한 칸 수, `candidates`는 물체로 추정한 후보 수다. `fresh`는 이번 편도에서 새 거리로 갱신한 ID 수다.

```c
#ifndef WEEK_FRAME_H
#define WEEK_FRAME_H
#include "radar_processing.h"

typedef struct {
    uint32_t frame, time_ms;
    uint16_t bins, sampled, valid, known, candidates;
    uint8_t fresh, id_mask;
    int32_t angle_mdeg[TRACKER_MAX_TARGETS];
    uint32_t range_mm[TRACKER_MAX_TARGETS];
} WeekFrame;

static inline WeekFrame Week_ReadFrame(const RadarProcessing *p, uint32_t now)
{
    WeekFrame out = {0};
    const RadarFrame *frame = &p->display_frame;
    out.frame = frame->frame_number;
    out.time_ms = now;
    out.bins = frame->bins;
    out.sampled = frame->received;
    out.candidates = p->candidate_count;
    for (uint16_t i = 0; i < frame->bins; ++i) {
        RadarSample s;
        if (RadarFrame_Get(frame, i, &s) && s.valid) ++out.valid;
        if (p->background.known[i]) ++out.known;
    }
    for (uint8_t i = 0; i < p->tracker.active_limit; ++i) {
        const RadarTarget *t = &p->tracker.targets[i];
        if ((t->state == TARGET_NEW || t->state == TARGET_TRACKING) &&
            t->miss_count == 0u) {
            ++out.fresh;
            out.id_mask |= (uint8_t)(1u << i);
            out.angle_mdeg[i] = t->angle_mdeg;
            out.range_mm[i] = t->range_mm;
        }
    }
    return out;
}
#endif
```

## ③ week_survey.c · 실습을 실행하는 파일

①로 OLED를 먼저 확인한다. 센서 설정을 켜면 **서보 이동 → 거리 측정 → 배경과 비교 → ID 갱신 → 화면 출력**을 반복한다. ②가 만든 기록 중 최근 8편도를 `g_week_log`에 보관한다. 가변저항과 LCD는 `WEEK_PANEL`을 켠 구성에서 사용한다.

```c
#if defined(WEEK_SURVEY_MAIN)
#include "board_system.h"
#include "clocks_and_modes.h"
#include "mp_irq_compat.h"
#include "week_frame.h"
#include "week_oled.h"

#ifndef WEEK_OLED_ONLY
#define WEEK_OLED_ONLY 0
#endif
#ifndef WEEK_LIMIT
#define WEEK_LIMIT 3
#endif
#if WEEK_LIMIT != 3 && WEEK_LIMIT != 5
#error "Select WEEK_LIMIT=3 or 5"
#endif

extern bool MP_TERP_GetBoardConfig(BoardSystemConfig *cfg);
static BoardSystemConfig cfg;
volatile unsigned g_week_error;
volatile int g_week_oled_status;
volatile unsigned g_week_oled_ok;

static void fail(unsigned code)
{
    g_week_error = code;
    for (;;) {}
}

#if !WEEK_OLED_ONLY
#ifndef WEEK_ECHO_IRQ
#error "Set WEEK_ECHO_IRQ to the verified Echo channel-pair IRQn"
#endif
#if defined(WEEK_XY_BENCH)
#include "week_xy.h"
static bool xy_timed;
#endif

#if defined(WEEK_PANEL)
extern bool Week_BoardLcdInit(void);
static ADCChannel pot;
static TargetSelector selector;
static LcdStatus lcd;
static bool adc_pending;
static uint32_t last_adc_ms;
static volatile uint16_t g_week_adc_raw;
static volatile uint32_t g_week_lcd_errors;

static void poll_panel(uint32_t now)
{
    uint16_t raw;
    if (adc_pending) {
        if (ADCChannel_Poll(&pot, &raw)) {
            adc_pending = false;
            g_week_adc_raw = raw;
            TargetSelector_Update(&selector, raw);
        }
    } else if ((uint32_t)(now - last_adc_ms) >= 20u) {
        last_adc_ms = now;
        adc_pending = ADCChannel_Start(&pot);
    }
}
#endif

static Servo servo;
static UltrasonicPort echo;
static RadarScan scan;
static RadarProcessing processing;
static RadarRendererConfig screen;
volatile WeekFrame g_week_log[8];
volatile uint32_t g_week_frames;
volatile uint32_t g_week_oled_failures;

void MP_TERP_StageEchoCaptureIrq(void)
{
    UltrasonicPort_OnCaptureIrq(&echo);
}

static bool move_sensor(void *ctx, int32_t angle)
{
    (void)ctx;
    return Servo_SetAngle(&servo, angle) == SERVO_OK;
}

static bool start_echo(void *ctx, uint32_t now)
{
    (void)ctx;
    if (UltrasonicPort_Start(&echo, now)) return true;
    fail(8u);
    return false;
}

static bool take_echo(void *ctx, uint32_t *mm, bool *valid)
{
    (void)ctx;
    return UltrasonicPort_Take(&echo, mm, valid);
}

#if defined(WEEK_PROFILE_FILTER)
void Week_FilterProfileStart(void);
#endif

static void survey(void)
{
    const AppConfig *a = &cfg.application;
    RadarScanConfig sc = {a->minimum_scan_angle_mdeg, a->maximum_scan_angle_mdeg,
        a->scan_step_mdeg, a->settle_ms, a->echo_timeout_ms,
        move_sensor, start_echo, take_echo, 0};
    if (cfg.timer.tick_channel == TIMER_CHANNEL_NONE ||
        cfg.timer.tick_channel == cfg.ultrasonic.trigger_timer_channel ||
        cfg.servo.ftm == cfg.ultrasonic.ftm ||
        sc.min_angle_mdeg < cfg.servo.minimum_angle_mdeg ||
        sc.max_angle_mdeg > cfg.servo.maximum_angle_mdeg ||
        (uint64_t)sc.echo_timeout_ms * 1000u <= cfg.ultrasonic.timeout_us ||
        !a->maximum_display_distance_mm) fail(3u);
    if (!RadarScan_Init(&scan, &sc) ||
        !RadarProcessing_Init(&processing, sc.min_angle_mdeg, sc.max_angle_mdeg,
            sc.step_mdeg, a->average_window, a->background_threshold_mm,
            &a->detector, a->match_angle_mdeg, a->match_range_mm,
            a->lost_timeout_ms, a->max_misses) ||
        !TargetTracker_SetLimit(&processing.tracker, WEEK_LIMIT)) fail(4u);
    screen = (RadarRendererConfig){sc.min_angle_mdeg, sc.max_angle_mdeg,
        a->maximum_display_distance_mm, 64u, 62u, 58u, WEEK_LIMIT};
    if (TimerManager_Init(&cfg.timer) != TIMER_OK) fail(5u);
    if (Servo_Init(&servo, &cfg.servo) != SERVO_OK) fail(6u);
    if (!UltrasonicPort_Init(&echo, &cfg.ultrasonic)) fail(7u);
    MP_NvicClearPending(WEEK_ECHO_IRQ);
    MP_NvicEnable(WEEK_ECHO_IRQ);
#if defined(WEEK_PROFILE_FILTER)
    Week_FilterProfileStart();
#endif
#if defined(WEEK_XY_BENCH)
#if defined(WEEK_PROFILE_FILTER)
    extern volatile bool g_week_filter_profile_ready;
    xy_timed = g_week_filter_profile_ready;
#else
    xy_timed = CycleProfiler_Enable();
#endif
#endif
#if defined(WEEK_PANEL)
    LcdStatusConfig lines = {cfg.lcd_write, cfg.user, 16u, 2u};
    if (!ADCChannel_Init(&pot, &cfg.adc) ||
        !TargetSelector_InitWithLimit(&selector, a->adc_max, a->adc_hysteresis, WEEK_LIMIT) ||
        !cfg.lcd_write || !Week_BoardLcdInit() || !LcdStatus_Init(&lcd, &lines))
        fail(12u);
#endif
    RadarProcessing_RequestCalibration(&processing);
    if (!RadarScan_Start(&scan, TimerManager_GetTickMs())) fail(9u);

    for (;;) {
        RadarSample sample;
        uint32_t now = TimerManager_GetTickMs();
#if defined(WEEK_PANEL)
        poll_panel(now);
#endif
        UltrasonicPort_Update(&echo, now);
        RadarScan_Update(&scan, now);
        if (scan.state == RADAR_IDLE) fail(10u);
        if (!RadarScan_TakeSample(&scan, &sample)) continue;
        if (!RadarProcessing_PushSample(&processing, &sample)) continue;
        RadarProcessing_ProcessCompleted(&processing, now);
        g_week_log[g_week_frames % 8u] = Week_ReadFrame(&processing, now);
        ++g_week_frames;
        uint8_t selected = 0u;
#if defined(WEEK_PANEL)
        selected = TargetSelector_GetId(&selector);
        RadarTarget t;
        bool found = TargetTracker_Get(&processing.tracker, selected, &t);
        const char *state = "EMPTY";
        if (found) {
            if (t.state == TARGET_LOST) state = "LOST";
            else if (t.miss_count) state = "MISSED";
            else state = t.state == TARGET_NEW ? "NEW" : "TRACKING";
        }
        if (!LcdStatus_Update(&lcd, selected, found ? t.angle_mdeg : 0,
                              found ? t.range_mm : 0u, state)) ++g_week_lcd_errors;
#endif
#if defined(WEEK_XY_BENCH)
        // 가장 최근에 갱신된 선택 ID, 패널이 없으면 첫 유효 ID의 표시 좌표를 계산한다.
        for (uint8_t j = 0u; j < processing.tracker.active_limit; ++j) {
            const RadarTarget *point = &processing.tracker.targets[j];
            if (selected && point->id != selected) continue;
            if ((point->state == TARGET_NEW || point->state == TARGET_TRACKING) &&
                point->miss_count == 0u) {
                Week_XY(point->angle_mdeg, point->range_mm, xy_timed);
                break;
            }
        }
#endif
        if (!RadarRenderer_Draw(&screen, &processing.display_frame,
                                &processing.tracker, selected, sample.angle_mdeg)) fail(11u);
        g_week_oled_status = OLED_Update();
        if (g_week_oled_status != 0) ++g_week_oled_failures;
    }
}
#endif

int main(void)
{
    if (!MP_TERP_GetBoardConfig(&cfg)) fail(1u);
    SOSC_init_8MHz();
    SPLL_init_160MHz();
    NormalRUNmode_80MHz();
    if (cfg.custom_oled_port && !OLED_PortConfigure(&cfg.oled_port)) fail(2u);
    g_week_oled_status = Week_OledCheck();
    if (g_week_oled_status != 0) fail(2u);
    g_week_oled_ok = 1u;
#if !WEEK_OLED_ONLY
    survey();
#endif
    for (;;) {}
}
#endif
```

## ④ week_distance_filter.c · 거리값 평균과 DSP

같은 각도에서 최근에 읽은 유효 거리값을 평균 낸다. 평균에 세 값을 쓰면 새 값이 들어올 때 가장 오래된 값이 빠진다. DSP 구성은 이 덧셈과 뺄셈에 Cortex-M4의 `SMLALD` 명령을 사용한다. 같은 시작 상태와 입력이면 일반 C와 같은 값이 나와야 한다. 예를 들어 창 3에 100 → 200 → 300 → 600 mm를 순서대로 넣으면 출력은 100 → 150 → 200 → 367 mm다.

```c
#include "distance_filter.h"

#if defined(WEEK_PROFILE_FILTER)
#include "cycle_profiler.h"

CycleProfiler g_week_filter_cycles;
volatile bool g_week_filter_profile_ready;

void Week_FilterProfileStart(void)
{
    CycleProfiler_Reset(&g_week_filter_cycles);
    g_week_filter_profile_ready = CycleProfiler_Enable();
}
#endif

bool DistanceFilter_Init(DistanceFilter *filter, uint8_t window)
{
    if (!filter || window == 0u || window > DISTANCE_FILTER_CAPACITY)
        return false;
    filter->window = window;
    DistanceFilter_Reset(filter);
    return true;
}

void DistanceFilter_Reset(DistanceFilter *filter)
{
    if (!filter) return;
    filter->sum = 0u;
    filter->count = 0u;
    filter->next = 0u;
    for (uint8_t i = 0u; i < DISTANCE_FILTER_CAPACITY; ++i)
        filter->samples[i] = 0u;
}

static uint64_t update_sum(uint64_t sum, uint32_t incoming, uint32_t outgoing)
{
#if defined(WEEK_FILTER_DSP)
    // 두 거리값이 signed 16-bit의 양수 범위일 때 한 명령으로 누적한다.
    if (incoming <= INT16_MAX && outgoing <= INT16_MAX) {
        uint32_t low = (uint32_t)sum;
        uint32_t high = (uint32_t)(sum >> 32);
        uint32_t packed_distances = incoming | (outgoing << 16);
        const uint32_t weights = 0xFFFF0001u; // 하위 +1, 상위 -1

        __asm__ volatile ("smlald %0, %1, %2, %3"
                          : "+&r"(low), "+&r"(high)
                          : "r"(packed_distances), "r"(weights));
        return ((uint64_t)high << 32) | low;
    }
#endif
    return sum - outgoing + incoming;
}

bool DistanceFilter_Update(DistanceFilter *filter, uint32_t input_mm,
                           bool valid, uint32_t *output_mm)
{
    if (!filter || !output_mm || !valid || filter->window == 0u) return false;
#if defined(WEEK_PROFILE_FILTER)
    if (g_week_filter_profile_ready) CycleProfiler_Begin(&g_week_filter_cycles);
#endif

    uint32_t outgoing = 0u;
    if (filter->count == filter->window)
        outgoing = filter->samples[filter->next];
    else
        ++filter->count;

    filter->sum = update_sum(filter->sum, input_mm, outgoing);
    filter->samples[filter->next] = input_mm;
    filter->next = (uint8_t)((filter->next + 1u) % filter->window);
    *output_mm = (uint32_t)((filter->sum + filter->count / 2u) / filter->count);

#if defined(WEEK_PROFILE_FILTER)
    if (g_week_filter_profile_ready) CycleProfiler_End(&g_week_filter_cycles);
#endif
    return true;
}
```

## ⑤ week_xy.h · 각도와 거리를 X·Y 위치로 계산

선택한 물체의 각도와 거리로 X·Y를 계산한다. 가변저항을 쓰지 않는 구성에서는 이번 편도에 갱신된 첫 ID를 사용한다. 같은 관측을 정수와 FPU 두 가지로 계산해 위치 차이와 걸린 시간을 기록한다. 비교하는 시간은 X·Y 계산과 결과 저장 구간이다.

```c
#ifndef WEEK_XY_H
#define WEEK_XY_H
#include "mp_trig.h"
#include "cycle_profiler.h"

#if !defined(__ARM_FP) || !(__ARM_FP & 4)
#error "Enable Cortex-M4 single-precision FPU code generation"
#endif

static CycleProfiler xy_float_cycles, xy_integer_cycles;
static volatile float g_week_x_mm, g_week_y_mm;
static volatile int32_t g_week_x_integer_mm, g_week_y_integer_mm;
static volatile uint32_t g_week_xy_samples;

static int32_t rounded_mm(int64_t scaled)
{
    return (int32_t)((scaled + (scaled >= 0 ? 16384 : -16384)) / 32768);
}

// 각도별 sin/cos와 정수 계수 준비는 두 타이밍 구간 밖에서 수행한다.
static void Week_XY(int32_t angle_mdeg, uint32_t mm, bool timed)
{
    float sine, cosine;
    if (mm > INT32_MAX || !MP_TrigSinCosMdeg(angle_mdeg, &sine, &cosine))
        return;
    int32_t c = (int32_t)(cosine * 32768.0f);
    int32_t s = (int32_t)(sine * 32768.0f);
    if (timed) CycleProfiler_Begin(&xy_integer_cycles);
    g_week_x_integer_mm = rounded_mm((int64_t)mm * c);
    g_week_y_integer_mm = rounded_mm((int64_t)mm * s);
    if (timed) CycleProfiler_End(&xy_integer_cycles);
    if (timed) CycleProfiler_Begin(&xy_float_cycles);
    g_week_x_mm = (float)mm * cosine;
    g_week_y_mm = (float)mm * sine;
    if (timed) CycleProfiler_End(&xy_float_cycles);
    ++g_week_xy_samples;
}
#endif
```

## 4. 실행한 뒤 어디를 보나

S32 Design Studio 디버거의 **Expressions**에 아래 값을 넣는다. 가장 최근 결과는 `g_week_frames`가 1 이상일 때 `g_week_log[(g_week_frames - 1) % 8]`에서 읽는다. 앞선 결과도 같은 배열에 남아 있다.

| 상황                               | 확인할 값                                                                                                                                                                                                                                                             |
| ---------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 센서 모드에서 `OLED OK`만 보인다 | `g_week_error`를 먼저 본다. 1은 보드 설정, 2는 OLED, 3은 타이머·각도·시간 설정, 4는 스캔·필터 초기화, 5는 Tick, 6은 서보, 7은 Echo, 8~10은 스캔 동작, 11은 화면 그리기, 12는 ADC·선택기·LCD 초기화다.                                                          |
| 물체를 놓아도 후보가 0개다         | `g_week_frames`가 늘고 있는지 본다. `valid`가 적으면 Echo 파형과 인터럽트를 확인한다. `known`이 적으면 물체를 치운 상태에서 벽이나 배경판이 센서에 잘 잡히는 위치로 옮겨 다시 시작한다. 최근 3개를 평균 내면 배치를 바꾼 뒤 몇 편도 지나야 값이 안정될 수 있다. |
| 원래 거리와 평균을 비교한다        | 같은 각도 칸의 값을 읽는다. 원래 거리는 `processing.display_frame`에서 보고, 평균은 `processing.filters`의 같은 칸에서 `(sum + count / 2) / count`로 계산한다. 이번에 유효값이 들어오고 `count`가 1 이상인 칸을 사용한다.                                     |
| 가변저항을 돌려도 표시가 그대로다  | `g_week_adc_raw`가 바뀌는지 먼저 본다. 다음 편도가 끝나면 OLED와 LCD가 갱신된다. LCD 쓰기 오류는 `g_week_lcd_errors`에 쌓인다. `MISSED`는 이번 편도에서 놓친 상태, `LOST`는 추적기가 유실로 판정한 상태이며 둘 다 마지막 유효 거리를 표시한다.                |
| 물체 5개의 ID를 확인한다           | 후보 수 `candidates`와 이번 편도에서 갱신한 ID 수 `fresh`를 비교한다. `id_mask=0x1F`는 ID 1~5가 모두 갱신됐다는 뜻이다. 실제 위치마다 ID가 바뀌지 않았는지도 각도·거리로 확인한다.                                                                             |
| FPU와 정수 좌표를 비교한다         | `g_week_xy_samples`가 늘어날 때마다 값이 갱신된다. FPU의 `g_week_x_mm`, `g_week_y_mm`와 정수의 `g_week_x_integer_mm`, `g_week_y_integer_mm`를 같은 시점에 읽는다.                                                                                           |

### 걸린 시간을 계산하는 방법

1. 거리 필터는 `g_week_filter_profile_ready`가 true인지 보고 `g_week_filter_cycles`를 읽는다. 좌표 계산은 `xy_timed`가 true인지 보고 `xy_integer_cycles`와 `xy_float_cycles`를 읽는다. 코드에서 DWT라는 보드 내부 사이클 카운터를 켠다.
2. 각 기록의 `count`는 측정 횟수, `total`은 걸린 사이클의 합, `maximum`은 한 번의 측정에서 가장 오래 걸린 사이클이다. `count`가 1 이상이면 **평균 사이클 = total ÷ count**로 계산한다.
3. 인터럽트가 들어오면 시간이 달라질 수 있다. 같은 조건으로 몇 번 측정하고 디버거로 멈춰 있던 구간은 비교에서 뺀다.

| 모드   | 측정한 부분         | 횟수 | 평균 사이클 | 최대 사이클 | 편도 시간(ms) | 후보 수 / 갱신 ID 수 |
| ------ | ------------------- | ---: | ----------: | ----------: | ------------: | -------------------- |
| 일반 C | 거리 필터           |      |             |             |               |                      |
| DSP    | 거리 필터           |      |             |             |               |                      |
| 정수   | 같은 점의 X·Y 계산 |      |             |             |               |                      |
| FPU    | 같은 점의 X·Y 계산 |      |             |             |               |                      |

| 실제 위치    | 원래 거리(mm) | 평균 거리(mm) | 정수 X·Y(mm) | FPU X·Y(mm) | 축별 차이(mm) |
| ------------ | ------------: | ------------: | ------------- | ------------ | ------------- |
| 실측 후 기입 |               |               |               |              |               |

## 5. 전체 앱을 실행할 때

센서 실습은 ③을 쓰고, 스텝 모터와 ADC를 포함한 팀 전체 앱은 기존 Stage 5 설정으로 빌드한다. `MP_TERP_STAGE_MAIN`과 `MP_TERP_STAGE=5`를 선택한다. 실제로 서보가 센서를 함께 돌리는지 확인한 뒤 `MP_TERP_CONFIRM_COUPLED_GEOMETRY`를 켠다. 전체 앱의 추적 한도를 5로 쓰려면 보드 설정 함수에서 `cfg->application.operating_target_limit=5u`로 지정한다. LCD 문구는 센서 실습용 ③의 문구와 다를 수 있다.

## 6. 확인을 마친 범위

S32DS Arm GCC 11.4에서 OLED 단독, 기본 3개, FPU 3개, 패널과 일반 C 필터 3개, 패널과 DSP 필터 5개 구성을 컴파일했다. 센서 코드를 기존 처리 모듈과 연결하는 부분 링크도 확인했다. 빌드 결과에서 FPU의 `vmul.f32`와 DSP의 `smlald`가 생성됐다. 최종 ELF는 실물 핀 설정, Echo 인터럽트, LCD 초기화 함수와 시작 코드를 넣어 빌드하고 보드에서 실행한다.

이 문서의 DSP는 **거리 이동평균**, FPU 비교는 **X·Y 계산**에 적용한다. Q15 FIR 자체를 비교하려면 저장소의 `DSPFIR_ProcessReference`와 `DSPFIR_ProcessPaired`에 같은 입력과 계수를 주고 각 함수의 이력 버퍼를 따로 유지한다. Q31, 속도·위치 예측 매칭, eDMA, WDOG는 팀 계획서에서 진행 상태를 확인한다.
