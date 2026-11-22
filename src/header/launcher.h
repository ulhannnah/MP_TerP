#ifndef LAUNCHER_H
#define LAUNCHER_H
#include <stdbool.h>
#include <stdint.h>
typedef void (*LauncherIndicator)(void *context,bool on);
typedef struct {
    LauncherIndicator indicator;
    void *context;
    uint32_t active_ms;
    uint32_t debounce_ms;
} LauncherConfig;
typedef struct {
    LauncherConfig config;
    uint32_t active_since_ms;
    uint32_t last_request_ms;
    uint8_t active;
    uint8_t requested;
    uint8_t request_seen;
} Launcher;
bool Launcher_Init(Launcher *launcher,const LauncherConfig *config);
void Launcher_Request(Launcher *launcher,uint32_t now_ms);
void Launcher_Update(Launcher *launcher,uint32_t now_ms,bool target_valid,bool aimed);
bool Launcher_IsActive(const Launcher *launcher);
#endif
