
#include "lj_debug_tablemark.h"

#include <stdlib.h>
#include <stdio.h>
#include "mylist.h"
#include "lj_obj.h"
#include "lj_frame.h"
#include "lj_debug.h"

#define __FILENAME__ \
    ( __builtin_strrchr(__FILE__, '/') ?  __builtin_strrchr(__FILE__, '/') + 1: __FILE__)
                                                                                         
// Hx@2022-08-26: 日志添加位置信息                                                       
#define LSTR(fmt, ...) \
    "[%s] %s, " fmt, __FILENAME__, __func__, ##__VA_ARGS__                               

typedef struct Tmark {
    struct list_head link;
    void* pc;
    int count;
    char src[64];
    int line;
} Tmark;

static __thread bool gTableMarkInit = false;
static bool gTableMarkEnable = true;

#define MARK_SIZE 0x010000
#define MARK_MASK 0x0FFFF
__thread struct list_head vMark[MARK_SIZE];

// Hx@2023-03-28: hash桶热数据长度
#define HASH_BUCK_WARM 20

void * lj_debug_tablemark(lua_State *L)
{
    if (!gTableMarkEnable) {
        return NULL;
    }

    if (!gTableMarkInit) {
        gTableMarkInit = true;
        for (int i = 0; i < MARK_SIZE; ++i) {
            INIT_LIST_HEAD(&vMark[i]);
        }
    }

    GCfunc *fn = curr_func(L);
    if (!isluafunc(fn)) {
        return NULL;
    }

    void *cf = cframe_raw(L->cframe);
    if (!cf) {
        return NULL;
    }

    void *pc = (void *)cframe_pc(cf);

    uint32_t idx = ((uint64_t) pc >> 2) & MARK_MASK;
    struct list_head *pos;
    int cnt = 0;
    LIST_FOR_EACH(pos, &vMark[idx]) {
        Tmark *node = LIST_ENTRY(pos, Tmark, link);
        if (node->pc == pc) {
            node->count += 1;
            if (cnt > HASH_BUCK_WARM) {
                LIST_DEL(pos);
                LIST_ADD(pos, &vMark[idx]);
            }
            return pc;
        }
        cnt ++;
    }

    GCproto *pt = funcproto(fn);

    Tmark* node = calloc(1, sizeof(Tmark));
    node->count = 1;
    node->pc = pc;
    node->line = debug_frameline(L, fn, NULL);

    const char *source = strdata(proto_chunkname(pt));
    size_t len = strlen(source);
    if (len < 64) {
        snprintf(node->src, 64, "%s", source);
    } else {
        snprintf(node->src, 64, "%s", source + len - 63);
    }

    LIST_ADD(&node->link, &vMark[idx]);

    return pc;
}


void lj_debug_tableunmark(void *pc)
{
    if (!gTableMarkEnable) {
        return;
    }

    if (!gTableMarkInit) {
        return;
    }

    if (!pc) {
        return;
    }

    uint32_t idx = ((uint64_t)pc >> 2) & MARK_MASK;

    struct list_head *pos;
    LIST_FOR_EACH(pos, &vMark[idx]) {
        Tmark *node = LIST_ENTRY(pos, Tmark, link);
        if (node->pc == pc) {
            node->count -= 1;
        }
    }
}

int lj_debug_tablemark_info(lua_State *L, int max)
{
    if (!gTableMarkInit) {
        return 0;
    }

    char str[90] = {};
    int count = 0;
    struct list_head *pos;
    lua_newtable(L);
    for (int idx = 0; idx < MARK_SIZE; ++idx) {
        LIST_FOR_EACH(pos, &vMark[idx]) {
            Tmark *node = LIST_ENTRY(pos, Tmark, link);
            if (node->count >= max) {
                snprintf(str, sizeof(str), "%d,%d,%s", node->count, node->line, node->src);
                lua_pushstring(L, str);
                lua_rawseti(L, -2, ++count);
            }
        }
    }
    return 1;
}

void lj_debug_tablemark_enable(bool enable)
{
    gTableMarkEnable = enable;
}