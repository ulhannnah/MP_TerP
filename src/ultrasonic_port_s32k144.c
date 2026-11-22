#include "ultrasonic_port_s32k144.h"
#include <stddef.h>
#define US_FTM_SOURCE 1u
static uint32_t ftm_pcc(const FTM_Type *ftm){
    if(ftm==FTM0)return PCC_FTM0_INDEX;
    if(ftm==FTM1)return PCC_FTM1_INDEX;
    if(ftm==FTM2)return PCC_FTM2_INDEX;
    return PCC_FTM3_INDEX;
}
static bool valid_ftm(const FTM_Type *ftm){
    return ftm==FTM0||ftm==FTM1||ftm==FTM2||ftm==FTM3;
}
static void trigger_write(void *context,bool high){
    UltrasonicPort *p=(UltrasonicPort *)context;
    uint32_t mask=(uint32_t)1u<<p->cfg.trigger_pin;
    if(high)p->cfg.trigger_gpio->PSOR=mask;
    else p->cfg.trigger_gpio->PCOR=mask;
}
static void trigger_end(void *context){
    UltrasonicPort *p=(UltrasonicPort *)context;
    /* Trigger falling edge is timed by the selected LPIT one-shot. */
    Ultrasonic_Update(&p->sensor,p->sensor.start_us+p->cfg.trigger_us);
}
bool UltrasonicPort_Init(UltrasonicPort *p,const UltrasonicPortConfig *cfg){
    uint8_t ps=0u;
    uint32_t hz=0u;
    UltrasonicConfig sensor_cfg;
    if(!p||!cfg||!valid_ftm(cfg->ftm)||cfg->ftm_channel>7u||
       !cfg->trigger_port||!cfg->trigger_gpio||!cfg->echo_port||
       cfg->trigger_pin>=32u||cfg->echo_pin>=32u||
       cfg->echo_pin_alt<2u||cfg->echo_pin_alt>7u||
       cfg->ftm_clock_pcs==0u||cfg->ftm_clock_pcs>7u||
       cfg->ftm_clock_hz<1000000u||cfg->timeout_us==0u||
       (unsigned)cfg->trigger_timer_channel>3u)return false;
    /* Choose slowest available FTM counter whose full cycle exceeds timeout.
     * Counter is reset for each measurement, so one wrap must not occur.
     */
    for(ps=0u;ps<8u;ps++){
        uint32_t candidate=cfg->ftm_clock_hz>>ps;
        if(candidate==0u)break;
        if((uint64_t)candidate*cfg->timeout_us<65000ull*1000000ull) {
            hz=candidate;break;
        }
    }
    if(hz==0u)return false;
    p->initialized=0u;p->cfg=*cfg;p->prescaler=ps;p->ftm_frequency_hz=hz;
    sensor_cfg.trigger_write=trigger_write;sensor_cfg.context=p;
    sensor_cfg.trigger_us=cfg->trigger_us;
    sensor_cfg.timeout_us=cfg->timeout_us;
    sensor_cfg.min_echo_us=cfg->minimum_echo_us;
    sensor_cfg.max_echo_us=cfg->maximum_echo_us;
    if(!Ultrasonic_Init(&p->sensor,&sensor_cfg))return false;
    if(TimerManager_Configure(cfg->trigger_timer_channel,cfg->trigger_us,true)!=TIMER_OK)
        return false;
    if(TimerManager_RegisterCallback(cfg->trigger_timer_channel,trigger_end,p)!=TIMER_OK)
        return false;

    PCC->PCCn[cfg->trigger_port_pcc_index]|=PCC_PCCn_CGC_MASK;
    PCC->PCCn[cfg->echo_port_pcc_index]|=PCC_PCCn_CGC_MASK;
    cfg->trigger_gpio->PCOR=(uint32_t)1u<<cfg->trigger_pin;
    cfg->trigger_port->PCR[cfg->trigger_pin]=PORT_PCR_MUX(1);
    cfg->trigger_gpio->PDDR|=(uint32_t)1u<<cfg->trigger_pin;
    cfg->echo_port->PCR[cfg->echo_pin]=PORT_PCR_MUX(cfg->echo_pin_alt);
    uint32_t idx=ftm_pcc(cfg->ftm);
    PCC->PCCn[idx]&=~PCC_PCCn_CGC_MASK;
    PCC->PCCn[idx]=(PCC->PCCn[idx]&~PCC_PCCn_PCS_MASK)|
                    PCC_PCCn_PCS(cfg->ftm_clock_pcs)|PCC_PCCn_CGC_MASK;
    cfg->ftm->SC=0u;
    cfg->ftm->MODE|=FTM_MODE_WPDIS_MASK;
    cfg->ftm->CNTIN=0u;
    cfg->ftm->MOD=65535u;
    cfg->ftm->CONTROLS[cfg->ftm_channel].CnSC=FTM_CnSC_ELSA_MASK|FTM_CnSC_CHIE_MASK;
    cfg->ftm->CNT=0u;
    p->initialized=1u;
    return true;
}
bool UltrasonicPort_Start(UltrasonicPort *p,uint32_t now_ms){
    if(!p||!p->initialized||p->sensor.state!=US_IDLE)return false;
    p->cfg.ftm->SC=0u;
    p->cfg.ftm->CONTROLS[p->cfg.ftm_channel].CnSC=FTM_CnSC_ELSA_MASK|FTM_CnSC_CHIE_MASK;
    p->cfg.ftm->CNT=0u;
    p->cfg.ftm->SC=FTM_SC_PS(p->prescaler)|FTM_SC_CLKS(US_FTM_SOURCE);
    if(!Ultrasonic_Start(&p->sensor,now_ms*1000u))return false;
    if(TimerManager_Start(p->cfg.trigger_timer_channel)!=TIMER_OK){
        Ultrasonic_Update(&p->sensor,p->sensor.start_us+p->cfg.timeout_us);
        p->cfg.ftm->SC=0u;
        return false;
    }
    return true;
}
void UltrasonicPort_Update(UltrasonicPort *p,uint32_t now_ms){
    if(!p||!p->initialized)return;
    Ultrasonic_Update(&p->sensor,now_ms*1000u);
}
bool UltrasonicPort_Take(UltrasonicPort *p,uint32_t *mm,bool *valid){
    bool result;
    if(!p||!p->initialized)return false;
    result=Ultrasonic_TakeResult(&p->sensor,mm,valid);
    if(result){p->cfg.ftm->SC=0u;(void)TimerManager_Stop(p->cfg.trigger_timer_channel);}
    return result;
}
void UltrasonicPort_OnCaptureIrq(UltrasonicPort *p){
    FTM_Type *ftm;
    uint32_t captured,elapsed_us;
    uint32_t csc;
    if(!p||!p->initialized)return;
    ftm=p->cfg.ftm;
    csc=ftm->CONTROLS[p->cfg.ftm_channel].CnSC;
    if((csc&FTM_CnSC_CHF_MASK)==0u)return;
    captured=ftm->CONTROLS[p->cfg.ftm_channel].CnV;
    /* FTM CHF is read-then-write-zero to clear, unlike LPIT W1C. */
    ftm->CONTROLS[p->cfg.ftm_channel].CnSC=csc&~FTM_CnSC_CHF_MASK;
    elapsed_us=(uint32_t)(((uint64_t)captured*1000000ull)/p->ftm_frequency_hz);
    if(p->sensor.state==US_WAIT_RISE){
        Ultrasonic_OnEchoEdge(&p->sensor,true,p->sensor.start_us+elapsed_us);
        ftm->CONTROLS[p->cfg.ftm_channel].CnSC=FTM_CnSC_ELSB_MASK|FTM_CnSC_CHIE_MASK;
    } else if(p->sensor.state==US_WAIT_FALL){
        Ultrasonic_OnEchoEdge(&p->sensor,false,p->sensor.start_us+elapsed_us);
    }
}
