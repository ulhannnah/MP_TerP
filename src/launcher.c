#include "launcher.h"
#include <stddef.h>
bool Launcher_Init(Launcher *l,const LauncherConfig *cfg){
    if(!l||!cfg||!cfg->indicator||!cfg->active_ms||!cfg->debounce_ms)return false;
    l->config=*cfg;l->active=0;l->requested=0;l->request_seen=0;
    l->active_since_ms=0;l->last_request_ms=0;cfg->indicator(cfg->context,false);
    return true;
}
void Launcher_Request(Launcher *l,uint32_t now){
    if(!l)return;
    if(l->request_seen && (uint32_t)(now-l->last_request_ms)<l->config.debounce_ms)return;
    l->request_seen=1u;l->last_request_ms=now;l->requested=1u;
}
void Launcher_Update(Launcher *l,uint32_t now,bool valid,bool aimed){
    if(!l)return;
    if(l->active && (uint32_t)(now-l->active_since_ms)>=l->config.active_ms) {
        l->active=0u;l->config.indicator(l->config.context,false);
    }
    if(l->requested){
        l->requested=0u;
        if(!l->active && valid && aimed){
            l->active=1u;l->active_since_ms=now;
            l->config.indicator(l->config.context,true);
        }
    }
}
bool Launcher_IsActive(const Launcher *l){return l&&l->active!=0u;}
