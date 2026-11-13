/* =====================================================================
 * Axiom NeuroBench 2003 - 安装包容器 (SMARC) 读取
 *
 * 那个年代 InstallShield 用 data1.cab，我们用自家的 DATA1.CAB，
 * 结构简单、带 CRC32 校验，安装程序与补丁程序共用。
 * ===================================================================== */
#ifndef SM_ARC_H
#define SM_ARC_H

#include "sm.h"

#pragma pack(push, 1)
typedef struct {
    char          magic[8];      /* "SMARC1\0\0" */
    unsigned int  version;
    unsigned int  count;
    unsigned int  data_off;      /* 数据区起始偏移 */
} SmArcHeader;

typedef struct {
    char          name[100];     /* 相对路径，用反斜杠 */
    unsigned int  size;
    unsigned int  crc;
    unsigned int  attrib;        /* FILE_ATTRIBUTE_* ；0x10 表示目录 */
} SmArcEntry;
#pragma pack(pop)

typedef struct {
    char               name[100];
    unsigned int       size;
    unsigned int       attrib;
    const unsigned char *data;
} SmArcItem;

typedef struct {
    unsigned char *raw;
    long           len;
    int            count;
    SmArcItem     *items;
    char           error[256];
} SmArc;

int  sm_arc_open(const char *path, SmArc *arc);
int  sm_arc_open_mem(void *buf, long len, SmArc *arc);
void sm_arc_close(SmArc *arc);
int  sm_arc_find(SmArc *arc, const char *name);
/* 回调: 返回 0 表示继续，非 0 表示用户取消 */
int  sm_arc_extract(SmArc *arc, const char *destDir,
                    int (*progress)(void *ctx, int idx, int total, const char *name),
                    void *ctx);

#endif
