/* Axiom NeuroBench 2003 - 极简 JSON 解析器（足够处理 chat/completions 响应） */
#include "sm.h"
#include <ctype.h>

/* 自己解析十进制浮点数。
 * 不用 atof()/strtod()：它们会把 mingw 的 __gdtoa/__strtodg 链进来，
 * 那部分是按 Pentium Pro 编译的，含 CMOV，在 Pentium MMX 上是非法指令。 */
static double js_atof(const char *s)
{
    double sign = 1.0, val = 0.0, scale = 1.0;
    int any = 0, e = 0, es = 1;

    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { sign = -1.0; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') { val = val * 10.0 + (double)(*s - '0'); s++; any = 1; }
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') {
            scale *= 0.1;
            val += (double)(*s - '0') * scale;
            s++; any = 1;
        }
    }
    if (!any) return 0.0;
    if (*s == 'e' || *s == 'E') {
        s++;
        if (*s == '-') { es = -1; s++; }
        else if (*s == '+') s++;
        while (*s >= '0' && *s <= '9') { e = e * 10 + (*s - '0'); s++; }
        for (; e > 0; e--) { if (es > 0) val *= 10.0; else val /= 10.0; }
    }
    return val * sign;
}


typedef struct {
    const char *p;
    const char *end;
    const char *err;
} JP;

static Js *jp_value(JP *j);

static void jp_ws(JP *j)
{
    while (j->p < j->end) {
        char c = *j->p;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') j->p++;
        else break;
    }
}

static Js *js_new(JsType t)
{
    Js *v = (Js *)sm_calloc(sizeof(Js));
    v->t = t;
    return v;
}

void js_free(Js *v)
{
    while (v) {
        Js *n = v->next;
        if (v->child) js_free(v->child);
        if (v->str) sm_free(v->str);
        if (v->key) sm_free(v->key);
        sm_free(v);
        v = n;
    }
}

static void utf8_put(Sb *b, unsigned int cp)
{
    if (cp < 0x80) sb_putc(b, (int)cp);
    else if (cp < 0x800) {
        sb_putc(b, (int)(0xC0 | (cp >> 6)));
        sb_putc(b, (int)(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        sb_putc(b, (int)(0xE0 | (cp >> 12)));
        sb_putc(b, (int)(0x80 | ((cp >> 6) & 0x3F)));
        sb_putc(b, (int)(0x80 | (cp & 0x3F)));
    } else {
        sb_putc(b, (int)(0xF0 | (cp >> 18)));
        sb_putc(b, (int)(0x80 | ((cp >> 12) & 0x3F)));
        sb_putc(b, (int)(0x80 | ((cp >> 6) & 0x3F)));
        sb_putc(b, (int)(0x80 | (cp & 0x3F)));
    }
}

static int hex4(JP *j, unsigned int *out)
{
    unsigned int v = 0;
    int i;
    if (j->end - j->p < 4) return 0;
    for (i = 0; i < 4; i++) {
        char c = *j->p++;
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned int)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned int)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned int)(c - 'A' + 10);
        else return 0;
    }
    *out = v;
    return 1;
}

static char *jp_string(JP *j)
{
    Sb b;
    sb_init(&b);
    if (j->p >= j->end || *j->p != '"') { j->err = "expected string"; sb_free(&b); return NULL; }
    j->p++;
    while (j->p < j->end) {
        char c = *j->p++;
        if (c == '"') {
            char *r = sb_take(&b);
            return r;
        }
        if (c == '\\') {
            char e;
            if (j->p >= j->end) break;
            e = *j->p++;
            switch (e) {
            case 'n': sb_putc(&b, '\n'); break;
            case 't': sb_putc(&b, '\t'); break;
            case 'r': sb_putc(&b, '\r'); break;
            case 'b': sb_putc(&b, '\b'); break;
            case 'f': sb_putc(&b, '\f'); break;
            case '/': sb_putc(&b, '/'); break;
            case '\\': sb_putc(&b, '\\'); break;
            case '"': sb_putc(&b, '"'); break;
            case 'u': {
                unsigned int cp;
                if (!hex4(j, &cp)) { j->err = "bad \\u"; sb_free(&b); return NULL; }
                if (cp >= 0xD800 && cp <= 0xDBFF && j->end - j->p >= 6 &&
                    j->p[0] == '\\' && j->p[1] == 'u') {
                    unsigned int lo;
                    const char *save = j->p;
                    j->p += 2;
                    if (hex4(j, &lo) && lo >= 0xDC00 && lo <= 0xDFFF)
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    else
                        j->p = save;
                }
                utf8_put(&b, cp);
                break;
            }
            default: sb_putc(&b, e); break;
            }
        } else {
            sb_putc(&b, c);
        }
    }
    j->err = "unterminated string";
    sb_free(&b);
    return NULL;
}

static Js *jp_object(JP *j)
{
    Js *obj = js_new(JS_OBJ), *tail = NULL;
    j->p++; /* { */
    jp_ws(j);
    if (j->p < j->end && *j->p == '}') { j->p++; return obj; }
    for (;;) {
        char *key;
        Js *v;
        jp_ws(j);
        key = jp_string(j);
        if (!key) { j->err = j->err ? j->err : "bad key"; js_free(obj); return NULL; }
        jp_ws(j);
        if (j->p >= j->end || *j->p != ':') { j->err = "expected ':'"; sm_free(key); js_free(obj); return NULL; }
        j->p++;
        v = jp_value(j);
        if (!v) { sm_free(key); js_free(obj); return NULL; }
        v->key = key;
        if (tail) tail->next = v; else obj->child = v;
        tail = v;
        jp_ws(j);
        if (j->p < j->end && *j->p == ',') { j->p++; continue; }
        if (j->p < j->end && *j->p == '}') { j->p++; break; }
        j->err = "expected ',' or '}'";
        js_free(obj);
        return NULL;
    }
    return obj;
}

static Js *jp_array(JP *j)
{
    Js *arr = js_new(JS_ARR), *tail = NULL;
    j->p++; /* [ */
    jp_ws(j);
    if (j->p < j->end && *j->p == ']') { j->p++; return arr; }
    for (;;) {
        Js *v = jp_value(j);
        if (!v) { js_free(arr); return NULL; }
        if (tail) tail->next = v; else arr->child = v;
        tail = v;
        jp_ws(j);
        if (j->p < j->end && *j->p == ',') { j->p++; continue; }
        if (j->p < j->end && *j->p == ']') { j->p++; break; }
        j->err = "expected ',' or ']'";
        js_free(arr);
        return NULL;
    }
    return arr;
}

static Js *jp_value(JP *j)
{
    jp_ws(j);
    if (j->p >= j->end) { j->err = "unexpected end"; return NULL; }
    switch (*j->p) {
    case '{': return jp_object(j);
    case '[': return jp_array(j);
    case '"': {
        char *s = jp_string(j);
        Js *v;
        if (!s) return NULL;
        v = js_new(JS_STR);
        v->str = s;
        return v;
    }
    case 't':
        if (j->end - j->p >= 4 && !memcmp(j->p, "true", 4)) {
            Js *v = js_new(JS_BOOL); v->b = 1; j->p += 4; return v;
        }
        break;
    case 'f':
        if (j->end - j->p >= 5 && !memcmp(j->p, "false", 5)) {
            Js *v = js_new(JS_BOOL); v->b = 0; j->p += 5; return v;
        }
        break;
    case 'n':
        if (j->end - j->p >= 4 && !memcmp(j->p, "null", 4)) {
            Js *v = js_new(JS_NULL); j->p += 4; return v;
        }
        break;
    default: break;
    }
    {
        const char *s = j->p;
        char tmp[64];
        size_t n;
        Js *v;
        while (j->p < j->end && (isdigit((unsigned char)*j->p) || *j->p == '-' ||
               *j->p == '+' || *j->p == '.' || *j->p == 'e' || *j->p == 'E')) j->p++;
        n = (size_t)(j->p - s);
        if (n == 0 || n >= sizeof(tmp)) { j->err = "bad value"; return NULL; }
        memcpy(tmp, s, n);
        tmp[n] = 0;
        v = js_new(JS_NUM);
        v->num = js_atof(tmp);
        return v;
    }
}

Js *js_parse(const char *text, const char **err)
{
    JP j;
    Js *v;
    if (!text) { if (err) *err = "null input"; return NULL; }
    j.p = text;
    j.end = text + strlen(text);
    j.err = NULL;
    v = jp_value(&j);
    if (!v && err) *err = j.err ? j.err : "parse error";
    return v;
}

Js *js_obj_get(Js *obj, const char *key)
{
    Js *c;
    if (!obj || obj->t != JS_OBJ) return NULL;
    for (c = obj->child; c; c = c->next)
        if (c->key && !strcmp(c->key, key)) return c;
    return NULL;
}

Js *js_arr_get(Js *arr, int idx)
{
    Js *c;
    int i = 0;
    if (!arr || arr->t != JS_ARR) return NULL;
    for (c = arr->child; c; c = c->next, i++)
        if (i == idx) return c;
    return NULL;
}

int js_arr_count(Js *arr)
{
    Js *c;
    int i = 0;
    if (!arr || arr->t != JS_ARR) return 0;
    for (c = arr->child; c; c = c->next) i++;
    return i;
}

const char *js_str(Js *v, const char *def)
{
    if (v && v->t == JS_STR && v->str) return v->str;
    return def;
}

double js_num(Js *v, double def)
{
    if (v && v->t == JS_NUM) return v->num;
    return def;
}

/* 把 ANSI 字符串按 JSON 字符串字面量写入（含首尾引号），内部转 UTF-8 */
void sb_put_json_str(Sb *b, const char *ansi)
{
    char *u;
    if (!ansi) { sb_puts(b, "\"\""); return; }
    u = sm_ansi_to_utf8(ansi);
    if (!u) u = sm_strdup("");
    sb_put_json_raw(b, u);
    sm_free(u);
}

/* 已经是 UTF-8 的文本：只做转义 */
void sb_put_json_raw(Sb *b, const char *utf8)
{
    const unsigned char *p;
    if (!utf8) { sb_puts(b, "\"\""); return; }
    sb_putc(b, '"');
    for (p = (const unsigned char *)utf8; *p; p++) {
        unsigned char c = *p;
        switch (c) {
        case '"':  sb_puts(b, "\\\""); break;
        case '\\': sb_puts(b, "\\\\"); break;
        case '\n': sb_puts(b, "\\n"); break;
        case '\r': sb_puts(b, "\\r"); break;
        case '\t': sb_puts(b, "\\t"); break;
        default:
            if (c < 0x20) sb_printf(b, "\\u%04x", c);
            else sb_putc(b, (int)c);
        }
    }
    sb_putc(b, '"');
}
