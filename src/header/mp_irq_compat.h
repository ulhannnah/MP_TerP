#ifndef MP_TERP_IRQ_COMPAT_H_
#define MP_TERP_IRQ_COMPAT_H_

#include <stdint.h>
#include "device_registers.h"

/* S32K144 Cortex-M4F compatibility for SDK releases which provide IRQn_Type
 * but not CMSIS Core NVIC helpers / PRIMASK intrinsics.
 *
 * ARMv7-M NVIC: ISER 0xE000E100, ICER 0xE000E180, ICPR 0xE000E280.
 * Do not use the register address path on non-ARM/host builds.
 */
#if defined(__arm__) || defined(__thumb__)
static inline uint32_t MP_IrqSave(void)
{
    uint32_t mask;
    __asm volatile ("mrs %0, primask\n\tcpsid i" : "=r"(mask) :: "memory");
    return mask;
}

static inline void MP_IrqRestore(uint32_t mask)
{
    __asm volatile ("msr primask, %0" :: "r"(mask) : "memory");
}

static inline void MP_NvicEnable(IRQn_Type irqn)
{
    const int32_t n = (int32_t)irqn;
    if (n >= 0 && n < 240) {
        volatile uint32_t *const iser =
            (volatile uint32_t *)(uintptr_t)(0xE000E100u + ((uint32_t)n / 32u) * 4u);
        *iser = 1u << ((uint32_t)n & 31u);
        __asm volatile ("dsb\n\tisb" ::: "memory");
    }
}

static inline void MP_NvicDisable(IRQn_Type irqn)
{
    const int32_t n = (int32_t)irqn;
    if (n >= 0 && n < 240) {
        volatile uint32_t *const icer =
            (volatile uint32_t *)(uintptr_t)(0xE000E180u + ((uint32_t)n / 32u) * 4u);
        *icer = 1u << ((uint32_t)n & 31u);
        __asm volatile ("dsb\n\tisb" ::: "memory");
    }
}

static inline void MP_NvicClearPending(IRQn_Type irqn)
{
    const int32_t n = (int32_t)irqn;
    if (n >= 0 && n < 240) {
        volatile uint32_t *const icpr =
            (volatile uint32_t *)(uintptr_t)(0xE000E280u + ((uint32_t)n / 32u) * 4u);
        *icpr = 1u << ((uint32_t)n & 31u);
        __asm volatile ("dsb\n\tisb" ::: "memory");
    }
}
#else
/* Retain the existing test register mocks without ARM instructions. */
static inline uint32_t MP_IrqSave(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    return mask;
}
static inline void MP_IrqRestore(uint32_t mask)
{
    if ((mask & 1u) == 0u) __enable_irq();
}
static inline void MP_NvicEnable(IRQn_Type irqn) { NVIC_EnableIRQ(irqn); }
static inline void MP_NvicDisable(IRQn_Type irqn) { NVIC_DisableIRQ(irqn); }
static inline void MP_NvicClearPending(IRQn_Type irqn) { NVIC_ClearPendingIRQ(irqn); }
#endif

#endif /* MP_TERP_IRQ_COMPAT_H_ */
