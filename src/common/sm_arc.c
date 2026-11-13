/* Axiom NeuroBench 2003 - SMARC 安装包容器读取实现 */
#include "sm_arc.h"

static int sm_arc_parse(SmArc *arc)
{
    long len = arc->len;
    SmArcHeader hdr;
    int i;
    unsigned int off;

    if (len < (long)sizeof(SmArcHeader)) {
        strcpy(arc->error, "安装数据文件已损坏（文件头不完整）。");
        return 0;
    }
    memcpy(&hdr, arc->raw, sizeof(hdr));
    if (memcmp(hdr.magic, "SMARC1", 6) != 0) {
        strcpy(arc->error, "安装数据文件格式不正确（不是 SMARC 安装包）。");
        return 0;
    }
    if (hdr.count == 0 || hdr.count > 4000) {
        strcpy(arc->error, "安装数据文件已损坏（文件数量异常）。");
        return 0;
    }
    if (len < (long)(sizeof(SmArcHeader) + hdr.count * sizeof(SmArcEntry))) {
        strcpy(arc->error, "安装数据文件已损坏（目录区不完整）。");
        return 0;
    }
    arc->count = (int)hdr.count;
    arc->items = (SmArcItem *)sm_calloc(sizeof(SmArcItem) * hdr.count);
    off = hdr.data_off;
    for (i = 0; i < (int)hdr.count; i++) {
        SmArcEntry e;
        memcpy(&e, arc->raw + sizeof(SmArcHeader) + i * sizeof(SmArcEntry), sizeof(e));
        strncpy(arc->items[i].name, e.name, sizeof(arc->items[i].name) - 1);
        arc->items[i].size = e.size;
        arc->items[i].attrib = e.attrib;
        if (off + e.size > (unsigned int)len) {
            strcpy(arc->error, "安装数据文件已损坏（数据区被截断）。");
            return 0;
        }
        arc->items[i].data = arc->raw + off;
        {
            DWORD crc = sm_crc32(arc->items[i].data, e.size);
            if (crc != e.crc) {
                _snprintf(arc->error, sizeof(arc->error),
                          "安装数据校验失败：%s\r\n\r\n"
                          "安装光盘可能已损坏，请换一张光盘或重新下载安装包。",
                          arc->items[i].name);
                arc->error[sizeof(arc->error) - 1] = 0;
                return 0;
            }
        }
        off += e.size;
    }
    return 1;
}


int sm_arc_open(const char *path, SmArc *arc)
{
    long len = 0;

    memset(arc, 0, sizeof(*arc));
    arc->raw = (unsigned char *)sm_read_file(path, 512L * 1024 * 1024, &len);
    if (!arc->raw) {
        _snprintf(arc->error, sizeof(arc->error),
                  "找不到或无法读取安装数据文件：\r\n%s\r\n\r\n"
                  "请确认光盘已经放入光驱，并且没有在资源管理器中移动过文件。", path);
        arc->error[sizeof(arc->error) - 1] = 0;
        return 0;
    }
    arc->len = len;
    return sm_arc_parse(arc);
}

/* 直接从内存里的字节流打开（单文件安装包用） */
int sm_arc_open_mem(void *buf, long len, SmArc *arc)
{
    memset(arc, 0, sizeof(*arc));
    arc->raw = (unsigned char *)buf;
    arc->len = len;
    return sm_arc_parse(arc);
}

void sm_arc_close(SmArc *arc)
{
    if (arc->raw) sm_free(arc->raw);
    if (arc->items) sm_free(arc->items);
    memset(arc, 0, sizeof(*arc));
}

int sm_arc_find(SmArc *arc, const char *name)
{
    int i;
    for (i = 0; i < arc->count; i++)
        if (!sm_stricmp(arc->items[i].name, name)) return i;
    return -1;
}

int sm_arc_extract(SmArc *arc, const char *destDir,
                   int (*progress)(void *ctx, int idx, int total, const char *name),
                   void *ctx)
{
    int i;
    for (i = 0; i < arc->count; i++) {
        SmArcItem *it = &arc->items[i];
        char *full = sm_path_join(destDir, it->name);
        if (it->attrib & FILE_ATTRIBUTE_DIRECTORY) {
            sm_mkdirs(full);
        } else {
            char *slash = strrchr(full, '\\');
            if (slash) {
                *slash = 0;
                sm_mkdirs(full);
                *slash = '\\';
            }
            SetFileAttributesA(full, FILE_ATTRIBUTE_NORMAL);
            if (!sm_write_file(full, it->data, (long)it->size)) {
                sm_free(full);
                return 0;
            }
            if (it->attrib & FILE_ATTRIBUTE_READONLY)
                SetFileAttributesA(full, it->attrib);
        }
        sm_free(full);
        if (progress && progress(ctx, i + 1, arc->count, it->name)) return 0;
    }
    return 1;
}
