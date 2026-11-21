#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "ultrasonic.h"
#include "radar_scan.h"
#include "distance_filter.h"
#include "background_filter.h"
#include "target_detector.h"
#include "target_tracker.h"
#include "target_selector.h"

static bool trigger_level;
static void drive_trigger(void *ctx,bool high) {(void)ctx;trigger_level=high;}
static int32_t last_move;
static unsigned move_count,trigger_count;
static bool echo_available,echo_valid;
static uint32_t echo_distance;
static bool move_servo(void *ctx,int32_t angle) {
    (void)ctx;last_move=angle;++move_count;return true;
}
static bool begin_echo(void *ctx,uint32_t at) {
    (void)ctx;(void)at;++trigger_count;return true;
}
static bool take_echo(void *ctx,uint32_t *mm,bool *valid) {
    (void)ctx;
    if(!echo_available)return false;
    *mm=echo_distance;*valid=echo_valid;echo_available=false;return true;
}
static void check_ultrasonic(void){
    Ultrasonic s;
    UltrasonicConfig cfg={drive_trigger,NULL,10u,30000u,100u,25000u};
    uint32_t mm=0;bool valid=false;
    assert(Ultrasonic_Init(&s,&cfg));
    assert(Ultrasonic_Start(&s,0u));
    assert(trigger_level);
    assert(!Ultrasonic_Start(&s,1u));
    Ultrasonic_Update(&s,10u);assert(!trigger_level);
    Ultrasonic_OnEchoEdge(&s,true,1000u);
    Ultrasonic_OnEchoEdge(&s,false,3000u);
    assert(Ultrasonic_TakeResult(&s,&mm,&valid));
    assert(valid&&mm==343u);
    assert(!Ultrasonic_TakeResult(&s,&mm,&valid));
    assert(Ultrasonic_Start(&s,100000u));
    Ultrasonic_Update(&s,130000u);
    assert(Ultrasonic_TakeResult(&s,&mm,&valid)&&!valid);
    assert(!trigger_level);
}
static void check_scan(void){
    RadarScan s;RadarSample sample;
    RadarScanConfig cfg={30000,90000,30000,20u,40u,move_servo,begin_echo,take_echo,NULL};
    const int32_t expected[]={30000,60000,90000,60000,30000};
    assert(RadarScan_Init(&s,&cfg));
    assert(RadarScan_Start(&s,0u));
    for(unsigned i=0;i<5;i++){
        const uint32_t t=(uint32_t)i*25u+20u;
        RadarScan_Update(&s,t);
        assert(trigger_count==i+1u);
        echo_available=true;echo_valid=true;echo_distance=1000u+i*10u;
        RadarScan_Update(&s,t+1u);
        assert(RadarScan_TakeSample(&s,&sample));
        assert(sample.valid&&sample.angle_mdeg==expected[i]);
        assert(!RadarScan_TakeSample(&s,&sample));
    }
    assert(move_count==6u&&last_move==60000);
    RadarScan_Stop(&s);
    assert(s.state==RADAR_IDLE);
}
static void check_filters(void){
    DistanceFilter f;BackgroundFilter bg;
    uint32_t avg=0;bool fg=false;
    assert(DistanceFilter_Init(&f,3u));
    assert(DistanceFilter_Update(&f,100u,true,&avg)&&avg==100u);
    assert(DistanceFilter_Update(&f,200u,true,&avg)&&avg==150u);
    assert(!DistanceFilter_Update(&f,0u,false,&avg));
    assert(DistanceFilter_Update(&f,300u,true,&avg)&&avg==200u);
    assert(DistanceFilter_Update(&f,600u,true,&avg)&&avg==367u);
    assert(BackgroundFilter_Init(&bg,5u,100u));
    assert(!BackgroundFilter_IsForeground(&bg,2u,500u,&fg));
    assert(BackgroundFilter_Calibrate(&bg,2u,1000u));
    assert(BackgroundFilter_IsForeground(&bg,2u,950u,&fg)&&!fg);
    assert(BackgroundFilter_IsForeground(&bg,2u,700u,&fg)&&fg);
}
static void check_tracking(void){
    TargetDetector d;TargetTracker t;RadarTarget x;
    TargetCandidate c[DETECTOR_MAX_CANDIDATES];
    TargetDetectorConfig dc={100u,12000,2u};
    uint16_t n;
    assert(TargetDetector_Init(&d,&dc));
    assert(TargetDetector_Add(&d,(TargetObservation){60000,1030u,10u}));
    assert(TargetDetector_Add(&d,(TargetObservation){30000,1000u,10u}));
    assert(TargetDetector_Add(&d,(TargetObservation){40000,1020u,10u}));
    assert(TargetDetector_Add(&d,(TargetObservation){70000,1040u,10u}));
    n=TargetDetector_Detect(&d,c,DETECTOR_MAX_CANDIDATES);
    assert(n==2u);
    assert(c[0].samples==2u&&c[0].angle_mdeg==35000);
    assert(c[1].samples==2u&&c[1].angle_mdeg==65000);
    assert(TargetTracker_Init(&t,15000,200u,1000u,2u));
    TargetTracker_Update(&t,c,n,10u);
    assert(TargetTracker_Get(&t,1u,&x)&&x.state==TARGET_NEW);
    TargetTracker_Update(&t,c,n,20u);
    assert(TargetTracker_Get(&t,1u,&x)&&x.state==TARGET_TRACKING);
    TargetTracker_Update(&t,NULL,0u,30u);
    TargetTracker_Update(&t,NULL,0u,40u);
    assert(TargetTracker_Get(&t,1u,&x)&&x.state==TARGET_LOST);
    TargetTracker_Update(&t,c,n,50u);
    assert(TargetTracker_Get(&t,1u,&x)&&x.state==TARGET_TRACKING);
}
static void check_selector(void){
    TargetSelector s;
    assert(TargetSelector_Init(&s,4095u,20u));
    assert(TargetSelector_Update(&s,0u)==1u);
    assert(TargetSelector_Update(&s,4095u)==5u);
    assert(TargetSelector_Update(&s,0u)==1u);
    assert(TargetSelector_Update(&s,819u)==1u); /* edge hysteresis */
    assert(TargetSelector_Update(&s,900u)==2u);
    assert(TargetSelector_GetId(&s)==2u);
}
int main(void) {
    check_ultrasonic();check_scan();check_filters();check_tracking();check_selector();
    puts("PASS: ultrasonic state, scan reversals, filters, 5-ID tracker, ADC selector");
    return 0;
}
