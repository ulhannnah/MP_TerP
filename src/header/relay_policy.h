#ifndef MP_TERP_RELAY_POLICY_H_
#define MP_TERP_RELAY_POLICY_H_
#include <stdbool.h>
#include <stdint.h>

/* Pure, testable interlock predicates. The caller performs GPIO writes.
 * A new successful aim arms ONE relay pulse; holding the button cannot
 * repeatedly energize an external actuator. */
static inline bool MP_RelayNewAimCompleted(uint8_t previous_state,
                                            uint8_t current_state,
                                            uint8_t completed_state)
{
    return current_state==completed_state &&
           previous_state!=completed_state;
}
static inline bool MP_RelayCanPulse(bool armed,bool currently_high,
                                    bool homed,bool stepper_moving)
{
    return armed && !currently_high && homed && !stepper_moving;
}
static inline bool MP_RelayPulseExpired(bool currently_high,
                                        uint32_t now_ms,
                                        uint32_t start_ms,
                                        uint32_t pulse_ms)
{
    return currently_high && pulse_ms!=0u &&
           (uint32_t)(now_ms-start_ms)>=pulse_ms;
}
#endif
