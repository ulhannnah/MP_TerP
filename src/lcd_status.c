#include "lcd_status.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
bool LcdStatus_Init(LcdStatus *lcd,const LcdStatusConfig *cfg) {
    if(!lcd||!cfg||!cfg->write_line||cfg->columns==0u||
       cfg->columns>LCD_STATUS_MAX_COLS||cfg->rows==0u||cfg->rows>2u)return false;
    lcd->config=*cfg;lcd->initialized=1u;
    for(unsigned r=0;r<2;r++)lcd->cached[r][0]='\0';
    return true;
}
bool LcdStatus_Update(LcdStatus *lcd,uint8_t id,int32_t angle,
                      uint32_t distance,const char *state) {
    char lines[2][LCD_STATUS_MAX_COLS+1u];
    int32_t degrees=angle/1000;
    if(!lcd||!lcd->initialized||!state)return false;
    (void)snprintf(lines[0],sizeof(lines[0]),"ID:%u A:%ld D:%lumm",
                  (unsigned)id,(long)degrees,(unsigned long)distance);
    (void)snprintf(lines[1],sizeof(lines[1]),"%-20.20s",state);
    for(uint8_t r=0;r<lcd->config.rows;r++) {
        char *line=lines[r];
        line[lcd->config.columns]='\0';
        if(strcmp(lcd->cached[r],line)!=0) {
            if(!lcd->config.write_line(lcd->config.context,r,line))return false;
            (void)strcpy(lcd->cached[r],line);
        }
    }
    return true;
}
