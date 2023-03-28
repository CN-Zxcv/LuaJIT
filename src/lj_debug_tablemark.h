#ifndef _LJ_DEBUG_TABLEMARK
#define _LJ_DEBUG_TABLEMARK

#include <stdbool.h>
#include "lua.h"
void * lj_debug_tablemark(lua_State *L);
void lj_debug_tableunmark();
int lj_debug_tablemark_info(lua_State *L, int max);
void lj_debug_tablemark_enable(bool enable);
#endif