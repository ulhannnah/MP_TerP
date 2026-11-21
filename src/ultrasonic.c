#include "ultrasonic.h"
#include <stddef.h>
bool Ultrasonic_Init(Ultrasonic *s, const UltrasonicConfig *cfg) {
    if (!s || !cfg || !cfg->trigger_write || cfg->trigger_us < 10u ||
        !cfg->timeout_us || cfg->trigger_us >= cfg->timeout_us ||
        cfg->min_echo_us == 0u || cfg->min_echo_us >= cfg->max_echo_us ||
        cfg->max_echo_us >= cfg->timeout_us || cfg->timeout_us >= 0x80000000u) return false;
    s->config=*cfg; s->state=US_IDLE; s->start_us=0; s->rise_us=0; s->echo_us=0;
    cfg->trigger_write(cfg->context,false); return true;
}
bool Ultrasonic_Start(Ultrasonic *s, uint32_t now) {
    if (!s || s->state!=US_IDLE) return false;
    s->start_us=now; s->echo_us=0; s->state=US_TRIGGER_HIGH;
    s->config.trigger_write(s->config.context,true); return true;
}
void Ultrasonic_Update(Ultrasonic *s, uint32_t now) {
    if (!s) return;
    if (s->state==US_TRIGGER_HIGH && (uint32_t)(now-s->start_us)>=s->config.trigger_us) {
        s->config.trigger_write(s->config.context,false); s->state=US_WAIT_RISE;
    }
    if ((s->state==US_TRIGGER_HIGH || s->state==US_WAIT_RISE || s->state==US_WAIT_FALL) &&
        (uint32_t)(now-s->start_us)>=s->config.timeout_us) {
        s->config.trigger_write(s->config.context,false); s->state=US_TIMEOUT;
    }
}
void Ultrasonic_OnEchoEdge(Ultrasonic *s,bool high,uint32_t when) {
    if (!s || (uint32_t)(when-s->start_us)>=s->config.timeout_us) return;
    if (high && s->state==US_WAIT_RISE) {s->rise_us=when; s->state=US_WAIT_FALL;}
    else if (!high && s->state==US_WAIT_FALL) {
        s->echo_us=(uint32_t)(when-s->rise_us);
        s->state=(s->echo_us>=s->config.min_echo_us &&
                  s->echo_us<=s->config.max_echo_us)?US_DONE:US_TIMEOUT;
    }
}
bool Ultrasonic_TakeResult(Ultrasonic *s,uint32_t *mm,bool *valid) {
    uint32_t us;
    if (!s || !mm || !valid || (s->state!=US_DONE && s->state!=US_TIMEOUT)) return false;
    us=s->echo_us; *valid=s->state==US_DONE;
    /* speed of sound ~343m/s => 0.343mm/us, round trip /2 */
    *mm=*valid ? (uint32_t)(((uint64_t)us*343u+1000u)/2000u):0u;
    s->state=US_IDLE; return true;
}
