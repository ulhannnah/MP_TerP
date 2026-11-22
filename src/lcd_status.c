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
static void fixed_width(char *dest,const char *src,uint8_t columns){
    uint8_t i=0u;
    while(i<columns && src[i]!='\0'){dest[i]=src[i];++i;}
    while(i<columns){dest[i]=' ';++i;}
    dest[columns]='\0';
}
bool LcdStatus_Update(LcdStatus *lcd,uint8_t id,int32_t angle,
                      uint32_t distance,const char *state) {
    char lines[2][LCD_STATUS_MAX_COLS+1u];
    char raw[48];
    int32_t degrees=angle/1000;
    if(!lcd||!lcd->initialized||!state)return false;
    (void)snprintf(raw,sizeof(raw),"ID:%u A:%ld D:%lumm",
                  (unsigned)id,(long)degrees,(unsigned long)distance);
    fixed_width(lines[0],raw,lcd->config.columns);
    fixed_width(lines[1],state,lcd->config.columns);
    for(uint8_t r=0;r<lcd->config.rows;r++) {
        if(strcmp(lcd->cached[r],lines[r])!=0) {
            if(!lcd->config.write_line(lcd->config.context,r,lines[r]))return false;
            (void)strcpy(lcd->cached[r],lines[r]);
        }
    }
    return true;
}
