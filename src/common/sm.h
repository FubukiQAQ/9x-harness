/* =====================================================================
 * NeuroBench 智能伴侣 2000
 * 公共头文件 / Common header
 *
 * 目标平台: Windows 95/98/2000/XP (Win32, 32 位, ANSI/GBK)
 * 编译:     i686-w64-mingw32-gcc -D_WIN32_WINNT=0x0501
 * ===================================================================== */
#ifndef SM_H
#define SM_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

/* ---------------- 版本 / 品牌信息 ----------------
 * 这些宏由 tools/mkbrand.py 从 brand.ini 生成到 nb_brand.h。
 * 换产品名、公司、版本、图标、安装目录，改 brand.ini 后重新编译即可。 */
#include "nb_brand.h"

/* 版本 / 授权版本 */
#define SM_ED_STANDARD    1
#define SM_ED_PROFESSIONAL 2
#define SM_ED_ENTERPRISE  3

/* ---------------- 通用 ---------------- */
void *sm_alloc(size_t n);
void *sm_calloc(size_t n);
void *sm_realloc(void *p, size_t n);
int   sm_memory_info(unsigned long *totalMB, unsigned long *availMB, unsigned long *loadPct);
char *sm_sysprompt_load(void);          /* 用户自定义系统提示词，无则返回空串 */
int   sm_sysprompt_save(const char *s); /* 空串则删除文件 */
void  sm_free(void *p);
char *sm_strdup(const char *s);
char *sm_strndup(const char *s, size_t n);

/* ---------------- 可增长字符串缓冲 ---------------- */
typedef struct {
    char  *p;
    size_t len;
    size_t cap;
    int    oom;
} Sb;

void  sb_init(Sb *b);
void  sb_free(Sb *b);
void  sb_clear(Sb *b);
void  sb_putn(Sb *b, const char *s, size_t n);
void  sb_puts(Sb *b, const char *s);
void  sb_putc(Sb *b, int c);
void  sb_printf(Sb *b, const char *fmt, ...);
void  sb_vprintf(Sb *b, const char *fmt, va_list ap);
/* 追加 UTF-8 文本（内部转成 ANSI/GBK 后追加） */
void  sb_put_utf8(Sb *b, const char *utf8);
char *sb_take(Sb *b);            /* 交出缓冲区（调用者 free） */
const char *sb_str(Sb *b);       /* 保证 NUL 结尾，可能返回 "" */

/* ---------------- 字符集转换 ---------------- */
/* 返回静态/新建缓冲区；*out 需 sm_free */
char *sm_utf8_to_ansi(const char *u);          /* UTF-8 -> GBK/CP_ACP */
char *sm_ansi_to_utf8(const char *a);
int sm_utf8_sanitize(char *s);          /* GBK/CP_ACP -> UTF-8 */
char *sm_conv(UINT from, UINT to, const char *s, int len); /* len<0 => strlen */

/* ---------------- 文件 / 路径 ---------------- */
char *sm_exe_dir(void);                        /* 含尾部反斜杠 */
char *sm_app_dir(void);                        /* 程序目录，含尾部反斜杠 */
char *sm_path_join(const char *dir, const char *name);
char *sm_appdata_dir(void);
char *sm_ini_path(void);
char *sm_temp_dir(void);

int   sm_file_exists(const char *path);
long  sm_file_size(const char *path);
char *sm_read_file(const char *path, long maxlen, long *outlen);
int   sm_write_file(const char *path, const void *data, long len);
int   sm_mkdirs(const char *path);
int   sm_copy_file(const char *src, const char *dst);
int   sm_delete_tree(const char *path);
void  sm_touch_file(const char *path, const char *text);

/* 目录列举：返回 '\0' 分隔的条目列表，*count 为条目数 */
typedef struct {
    char name[MAX_PATH];
    DWORD attrib;
    DWORD size;
    FILETIME mtime;
} SmDirEnt;
int sm_list_dir(const char *path, SmDirEnt **ents, int *count);

/* ---------------- 时间 ---------------- */
void  sm_now_str(char *buf, size_t len);                 /* 2003-06-01 12:34:56 */
void  sm_now_str_era(char *buf, size_t len);             /* 2003年6月1日 星期日 12:34 */
void  sm_filetime_str(const FILETIME *ft, char *buf, size_t len);
DWORD sm_now(void);

/* ---------------- 杂项 ---------------- */
void  sm_center_window(HWND hwnd);
void  sm_fix_scroll_repaint(HWND h);   /* 只读多行框滚动后整块重画 */

/* ---------------- 抓屏 / PNG（给读图用） ---------------- */
int   sm_grab_screen(unsigned char **outBgr, int *outW, int *outH);
int   sm_grab_screen_max(unsigned char **o, int *w, int *h, int maxW, int maxH);
unsigned char *sm_load_image_png(const char *path, long *outLen);
unsigned char *sm_png_encode(const unsigned char *bgr, int w, int h, long *outLen);
void *sm_realloc(void *p, size_t n);DWORD sm_crc32(const void *data, size_t len);
void  sm_random_bytes(void *out, size_t n);
int   sm_stricmp(const char *a, const char *b);
int   sm_strnicmp(const char *a, const char *b, size_t n);
const char *sm_stristr(const char *hay, const char *needle);
void  sm_logf(const char *fmt, ...);
const char *sm_log_path(void);
void  sm_trim(char *s);
char *sm_reflow_text(const char *in);   /* 合并模型硬折行 */
long  sm_to_long(const char *s, long def);
UINT  sm_system_codepage(void);
char *sm_get_os_name(void);      /* "Windows XP Service Pack 3" 等 */

/* ---------------- 加密 / 编码 ---------------- */
void  sm_sha1(const void *data, size_t len, unsigned char out[20]);
void  sm_hmac_sha1(const void *key, size_t keylen, const void *data, size_t len,
                   unsigned char out[20]);
size_t sm_b64_encode(const unsigned char *in, size_t len, char *out, int wrap);
long  sm_b64_decode(const char *in, size_t len, unsigned char *out);

/* ---------------- 简易 JSON ---------------- */
typedef enum { JS_NULL, JS_BOOL, JS_NUM, JS_STR, JS_ARR, JS_OBJ } JsType;

typedef struct Js {
    JsType        t;
    double        num;
    int           b;
    char         *str;      /* JS_STR: UTF-8 文本 */
    char         *key;      /* 作为对象成员时的键名 */
    struct Js    *child;    /* 数组/对象首元素 */
    struct Js    *next;     /* 兄弟 */
} Js;

Js   *js_parse(const char *text, const char **err);
void  js_free(Js *v);
Js   *js_obj_get(Js *obj, const char *key);
Js   *js_arr_get(Js *arr, int idx);
int   js_arr_count(Js *arr);
const char *js_str(Js *v, const char *def);
double js_num(Js *v, double def);
void  sb_put_json_str(Sb *b, const char *ansi);  /* 写一个 JSON 字符串（ANSI->UTF-8 转义） */
void  sb_put_json_raw(Sb *b, const char *utf8);  /* 写一个 JSON 字符串（已是 UTF-8，仅转义） */

/* ---------------- INI ---------------- */
char *sm_ini_get(const char *file, const char *sec, const char *key,
                 const char *def);
int   sm_ini_set(const char *file, const char *sec, const char *key,
                 const char *val);

/* ---------------- 注册表 ---------------- */
int   sm_reg_set_sz(HKEY root, const char *path, const char *name, const char *val);
char *sm_reg_get_sz(HKEY root, const char *path, const char *name);
int   sm_reg_set_dw(HKEY root, const char *path, const char *name, DWORD val);
DWORD sm_reg_get_dw(HKEY root, const char *path, const char *name, DWORD def);
int   sm_reg_del_key(HKEY root, const char *path);

/* ---------------- 配置（SMATE.INI 封装） ---------------- */
typedef struct {
    char  endpoint[260];   /* https://api.example.com/v1/chat/completions */
    char  model[96];
    char  apikey[260];
    int   temperature10;   /* 温度 x10, 例如 7 => 0.7 */
    int   maxtokens;
    int   useTools;        /* 允许调用工具 */
    int   ignoreCertDate;  /* 忽略证书日期 */
    int   confirmWrite;    /* 危险工具需确认 */
    int   fontSize;        /* 显示字号(逻辑) */
    int   timeoutSec;
    int   think;           /* 深度思考：0=关 1=开 */
    int   thinkLevel;      /* 思考挡位：0=低 1=中 2=高 */
    int   thinkStyle;      /* 0=两种参数都发 1=只发 reasoning_effort 2=只发 enable_thinking */
    int   toolRounds;      /* 单轮最多连续调用工具的轮数（1..50） */
    int   permMode;        /* 工具默认权限：0=只读 1=总是询问 2=完全权限 */
    int   reflow;          /* 1=自动合并模型硬折行（默认开） */
} SmConfig;

/* ---------------- 多供应商 / 多模型 ---------------- */
#define SM_MAX_PROVIDERS 16

typedef struct {
    char  name[48];        /* 显示名，例如 "默认"、"DeepSeek" */
    char  endpoint[260];
    char  model[96];
    char  apikey[260];
    int   temperature10;
    int   maxtokens;
    int   timeoutSec;
    int   think;
    int   thinkLevel;
    int   thinkStyle;
    int   vision;          /* 1 = 该模型支持读图 */
} SmProvider;

typedef struct {
    SmProvider list[SM_MAX_PROVIDERS];
    int        count;
    int        active;
} SmProviders;

void sm_providers_load(SmProviders *p);      /* 首次自动把单段配置迁移成一行 */
int  sm_providers_save(const SmProviders *p);
void sm_provider_to_config(const SmProviders *p, int i, SmConfig *c);
void sm_config_to_provider(const SmConfig *c, SmProvider *p, const char *name);

void sm_config_defaults(SmConfig *c);
int  sm_config_load(SmConfig *c);
int  sm_config_save(const SmConfig *c);

/* ---------------- HTTPS 客户端 (BearSSL) ---------------- */
typedef struct {
    const char    *endpoint;      /* https://host[:port]/path */
    const char    *apikey;        /* Bearer 令牌，可为 NULL */
    const char    *body;          /* POST 内容 */
    int            bodylen;
    const char    *extra_headers; /* 附加请求头，"Name: v\r\n" */
    int            timeoutSec;
    int            ignoreCertDate;/* 证书日期错误时用网络时间重试 */
    volatile LONG *cancel;        /* 非 0 时中止 */
} SmHttpArgs;

typedef struct {
    int   status;                 /* HTTP 状态码 */
    char  errmsg[700];            /* 失败时的中文说明 */
    char *body;
    int   bodylen;
    char  proto[64];
    char  certinfo[300];
} SmHttpResult;

int  sm_https_post(const SmHttpArgs *a, SmHttpResult *res);
void sm_http_result_free(SmHttpResult *res);
int  sm_tls_test(const char *endpoint, const char *apikey, int timeoutSec,
                 int ignoreCertDate, char *report, int len);

/* ---------------- 授权 (CDK) ---------------- */
#define SM_LIC_NONE      0
#define SM_LIC_TRIAL     1
#define SM_LIC_ACTIVATED 2
#define SM_LIC_EXPIRED   3

typedef struct {
    int   state;        /* SM_LIC_* */
    int   edition;      /* SM_ED_* */
    int   seats;
    char  cdk[40];
    DWORD install_time;
    DWORD expiry_time;  /* 0 = 永久 */
    int   trial_days_left;
    int   trial_msgs_left;
    int   tampered;
} SmLicense;

/* CDK 生成/校验 */
int  sm_cdk_generate(int edition, int seats, int days, unsigned int serial,
                     char out[32]);
int  sm_cdk_verify(const char *cdk, int *edition, int *seats, int *days,
                   unsigned int *serial);
void sm_cdk_normalize(const char *in, char out[32]);

/* 试用/激活状态 */
void sm_license_read(SmLicense *lic);
int  sm_license_activate(const char *cdk, SmLicense *out, char *err, int errlen);
void sm_license_consume_trial_message(void);
int  sm_license_edition_name(int edition, char *buf, int len);
const char *sm_license_state_text(const SmLicense *lic, char *buf, int len);

#endif /* SM_H */
