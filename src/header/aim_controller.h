#ifndef AIM_CONTROLLER_H
#define AIM_CONTROLLER_H
#include <stdbool.h>
#include <stdint.h>
#include "target_tracker.h"
typedef bool (*AimMove)(void *context,int32_t angle_mdeg);
typedef struct {
    AimMove move;
    void *context;
    uint32_t max_target_age_ms;
    int32_t minimum_angle_mdeg;
    int32_t maximum_angle_mdeg;
} AimControllerConfig;
typedef enum { AIM_IDLE, AIM_TRACKING, AIM_TARGET_LOST, AIM_OUT_OF_RANGE } AimState;
typedef struct {
    AimControllerConfig config;
    AimState state;
    uint8_t selected_id;
    int32_t last_commanded_angle_mdeg;
} AimController;
bool AimController_Init(AimController *a,const AimControllerConfig *config);
void AimController_Select(AimController *a,uint8_t id);
/* Update from latest snapshot; never commands movement to LOST or stale targets. */
void AimController_Update(AimController *a,const TargetTracker *tracker,uint32_t now_ms);
#endif
