#ifndef LCD_STATUS_H
#define LCD_STATUS_H
#include <stdbool.h>
#include <stdint.h>
#define LCD_STATUS_MAX_COLS 20u
typedef bool (*LcdStatusWrite)(void *ctx,uint8_t row,const char *text);
typedef struct {
    LcdStatusWrite write_line;
    void *context;
    uint8_t columns;
    uint8_t rows;
} LcdStatusConfig;
typedef struct {
    LcdStatusConfig config;
    char cached[2][LCD_STATUS_MAX_COLS+1u];
    uint8_t initialized;
} LcdStatus;
bool LcdStatus_Init(LcdStatus *lcd,const LcdStatusConfig *cfg);
bool LcdStatus_Update(LcdStatus *lcd,uint8_t selected_id,
                      int32_t angle_mdeg,uint32_t distance_mm,
                      const char *state);
#endif
