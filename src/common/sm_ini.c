/* Axiom NeuroBench 2003 - INI 读写 / 配置 / 注册表 */
#include "sm.h"

/* ------------------------------------------------------------------ */
static char *trim_dup(const char *s, size_t n)
{
    while (n && (*s == ' ' || *s == '\t')) { s++; n--; }
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) n--;
    return sm_strndup(s, n);
}

char *sm_ini_get(const char *file, const char *sec, const char *key,
                 const char *def)
{
    FILE *f;
    char line[2048];
    char cur[256];
    int found = 0;
    char *res = NULL;

    if (!file || !sec || !key) return sm_strdup(def ? def : "");
    f = fopen(file, "rb");
    if (!f) return sm_strdup(def ? def : "");
    cur[0] = 0;
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ';' || *p == '#' || !*p) continue;
        if (*p == '[') {
            char *e = strchr(p, ']');
            if (e) {
                char *s = trim_dup(p + 1, (size_t)(e - p - 1));
                strncpy(cur, s, sizeof(cur) - 1);
                cur[sizeof(cur) - 1] = 0;
                sm_free(s);
            }
            continue;
        }
        if (sm_stricmp(cur, sec)) continue;
        {
            char *eq = strchr(p, '=');
            if (eq) {
                char *k = trim_dup(p, (size_t)(eq - p));
                if (!sm_stricmp(k, key)) {
                    char *v = trim_dup(eq + 1, strlen(eq + 1));
                    /* 去掉引号 */
                    size_t vl = strlen(v);
                    if (vl >= 2 && v[0] == '"' && v[vl - 1] == '"') {
                        char *v2 = sm_strndup(v + 1, vl - 2);
                        sm_free(v);
                        v = v2;
                    }
                    res = v;
                    sm_free(k);
                    found = 1;
                    break;
                }
                sm_free(k);
            }
        }
    }
    fclose(f);
    if (!found) return sm_strdup(def ? def : "");
    return res;
}

int sm_ini_set(const char *file, const char *sec, const char *key,
               const char *val)
{
    FILE *f;
    char line[2048];
    Sb out;
    int insec = 0, done = 0, sawsec = 0;

    sb_init(&out);
    f = fopen(file, "rb");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            char *p = line;
            char t[2048];
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '[') {
                if (insec && !done) {
                    sb_printf(&out, "%s=%s\r\n", key, val ? val : "");
                    done = 1;
                }
                {
                    char *e = strchr(p, ']');
                    if (e) {
                        char *s = trim_dup(p + 1, (size_t)(e - p - 1));
                        insec = (sm_stricmp(s, sec) == 0);
                        if (insec) sawsec = 1;
                        sm_free(s);
                    }
                }
                sb_puts(&out, line);
                if (line[strlen(line) - 1] != '\n') sb_puts(&out, "\r\n");
                continue;
            }
            if (insec) {
                char *eq = strchr(p, '=');
                if (eq) {
                    char *k = trim_dup(p, (size_t)(eq - p));
                    int iskey = (sm_stricmp(k, key) == 0);
                    sm_free(k);
                    if (iskey) {
                        if (!done) {
                            sb_printf(&out, "%s=%s\r\n", key, val ? val : "");
                            done = 1;
                        }
                        continue;
                    }
                }
            }
            strncpy(t, line, sizeof(t) - 1);
            sb_puts(&out, t);
            if (t[0] && t[strlen(t) - 1] != '\n') sb_puts(&out, "\r\n");
        }
        fclose(f);
    }
    if (!done) {
        if (!sawsec) {
            sb_printf(&out, "%s[%s]\r\n", out.len ? "\r\n" : "", sec);
        } else if (insec) {
            /* 已被上面的分支处理 */
        }
        sb_printf(&out, "%s=%s\r\n", key, val ? val : "");
    }
    {
        int ok = sm_write_file(file, sb_str(&out), (long)out.len);
        sb_free(&out);
        return ok;
    }
}

/* ------------------------------------------------------------------ */
int sm_reg_set_sz(HKEY root, const char *path, const char *name, const char *val)
{
    HKEY k;
    LONG r;
    if (RegCreateKeyExA(root, path, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) != ERROR_SUCCESS)
        return 0;
    r = RegSetValueExA(k, name, 0, REG_SZ, (const BYTE *)val, (DWORD)(strlen(val) + 1));
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

char *sm_reg_get_sz(HKEY root, const char *path, const char *name)
{
    HKEY k;
    DWORD type = 0, size = 0;
    char *buf;
    if (RegOpenKeyExA(root, path, 0, KEY_QUERY_VALUE, &k) != ERROR_SUCCESS) return NULL;
    if (RegQueryValueExA(k, name, NULL, &type, NULL, &size) != ERROR_SUCCESS) { RegCloseKey(k); return NULL; }
    if (size == 0 || size > 65536) { RegCloseKey(k); return NULL; }
    buf = (char *)sm_alloc(size + 2);
    if (RegQueryValueExA(k, name, NULL, &type, (BYTE *)buf, &size) != ERROR_SUCCESS) {
        sm_free(buf); RegCloseKey(k); return NULL;
    }
    buf[size] = 0;
    RegCloseKey(k);
    return buf;
}

int sm_reg_set_dw(HKEY root, const char *path, const char *name, DWORD val)
{
    HKEY k;
    LONG r;
    if (RegCreateKeyExA(root, path, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) != ERROR_SUCCESS)
        return 0;
    r = RegSetValueExA(k, name, 0, REG_DWORD, (const BYTE *)&val, sizeof(val));
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

DWORD sm_reg_get_dw(HKEY root, const char *path, const char *name, DWORD def)
{
    HKEY k;
    DWORD type = 0, val = 0, size = sizeof(val);
    if (RegOpenKeyExA(root, path, 0, KEY_QUERY_VALUE, &k) != ERROR_SUCCESS) return def;
    if (RegQueryValueExA(k, name, NULL, &type, (BYTE *)&val, &size) != ERROR_SUCCESS) {
        RegCloseKey(k); return def;
    }
    RegCloseKey(k);
    return (type == REG_DWORD) ? val : def;
}

int sm_reg_del_key(HKEY root, const char *path)
{
    return RegDeleteKeyA(root, path) == ERROR_SUCCESS;
}

/* ------------------------------------------------------------------ */
void sm_config_defaults(SmConfig *c)
{
    memset(c, 0, sizeof(*c));
    strcpy(c->endpoint, "https://api.deepseek.com/chat/completions");
    strcpy(c->model, "deepseek-chat");
    c->apikey[0] = 0;
    c->temperature10 = 7;
    c->maxtokens = 2048;
    c->useTools = 1;
    c->ignoreCertDate = 0;
    c->confirmWrite = 1;
    c->fontSize = 12;            /* 12px == 9pt 宋体 */
    c->timeoutSec = 60;
    c->think = 0;
    c->thinkLevel = 1;
    c->thinkStyle = 0;
    c->toolRounds = 8;
    c->permMode = 1;          /* 默认「总是询问」，与旧的 confirmWrite=1 一致 */
    c->reflow = 1;
}

/* 密钥用固定的简短异或掩码，再套一层 Base64。不是高强度的加密。 */
static const unsigned char OBF_K[] = { 0x53, 0x6D, 0x61, 0x72, 0x74, 0x4D, 0x38, 0x37 };

/* 必须按显式长度做异或：异或结果里可能出现 0x00，
 * 用 strlen() 会把密钥截断（表现为服务器回 401 密钥无效）。 */
static void obf_n(char *s, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) s[i] = (char)(s[i] ^ OBF_K[i % sizeof(OBF_K)]);
}

static void deobf(char *s)
{
    obf_n(s, strlen(s));
}

static void obf(char *s)
{
    obf_n(s, strlen(s)); /* 对称 */
}


/* ------------------------------------------------------------------ */
/* 用户自定义系统提示词：单独存一个 UTF-8/ANSI 文本文件，              */
/* 因为 INI 里塞多行文本太别扭，而且用户可能想直接用记事本编辑。        */
/* ------------------------------------------------------------------ */
static char *prompt_path(void)
{
    char *ini = sm_ini_path();
    char *p;
    char *bs = strrchr(ini, '\\');
    if (bs) bs[1] = 0;
    p = sm_path_join(ini, "SYSPROMPT.TXT");
    sm_free(ini);
    return p;
}

char *sm_sysprompt_load(void)
{
    char *path = prompt_path();
    FILE *f = fopen(path, "rb");
    char *buf;
    long n = 0;

    sm_free(path);
    if (!f) return sm_strdup("");

    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) n = 0;
    if (n > 65536) n = 65536;

    buf = (char *)sm_alloc((size_t)n + 1);
    if (!buf) { fclose(f); return sm_strdup(""); }
    if (n > 0) {
        size_t got = fread(buf, 1, (size_t)n, f);
        buf[got] = 0;
    } else {
        buf[0] = 0;
    }
    fclose(f);
    return buf;
}

int sm_sysprompt_save(const char *s)
{
    char *path = prompt_path();
    FILE *f;

    if (!s || !s[0]) {
        DeleteFileA(path);            /* 清空 = 删除文件，回到默认 */
        sm_free(path);
        return 1;
    }
    f = fopen(path, "wb");
    if (!f) { sm_free(path); return 0; }
    fwrite(s, 1, strlen(s), f);
    fclose(f);
    sm_free(path);
    return 1;
}

int sm_config_load(SmConfig *c)
{
    char *ini;
    char *v;
    char tmp[64];

    sm_config_defaults(c);
    ini = sm_ini_path();
    if (!sm_file_exists(ini)) { sm_free(ini); return 0; }

    v = sm_ini_get(ini, "connection", "endpoint", c->endpoint);
    strncpy(c->endpoint, v, sizeof(c->endpoint) - 1);
    c->endpoint[sizeof(c->endpoint) - 1] = 0;
    sm_free(v);

    v = sm_ini_get(ini, "connection", "model", c->model);
    strncpy(c->model, v, sizeof(c->model) - 1); sm_free(v);

    v = sm_ini_get(ini, "connection", "apikey", "");
    if (v[0]) {
        unsigned char raw[512];
        long n = sm_b64_decode(v, strlen(v), raw);
        if (n > 0 && n < (long)sizeof(raw)) {
            obf_n((char *)raw, (size_t)n);   /* 按实际字节数解掩码 */
            raw[n] = 0;
            strncpy(c->apikey, (char *)raw, sizeof(c->apikey) - 1);
        } else {
            strncpy(c->apikey, v, sizeof(c->apikey) - 1);
        }
    }
    sm_free(v);

    _snprintf(tmp, sizeof(tmp), "%d", c->temperature10);
    v = sm_ini_get(ini, "connection", "temperature", tmp);
    c->temperature10 = (int)sm_to_long(v, c->temperature10); sm_free(v);

    _snprintf(tmp, sizeof(tmp), "%d", c->maxtokens);
    v = sm_ini_get(ini, "connection", "maxtokens", tmp);
    c->maxtokens = (int)sm_to_long(v, c->maxtokens); sm_free(v);

    _snprintf(tmp, sizeof(tmp), "%d", c->timeoutSec);
    v = sm_ini_get(ini, "connection", "timeout", tmp);
    c->timeoutSec = (int)sm_to_long(v, c->timeoutSec); sm_free(v);

    _snprintf(tmp, sizeof(tmp), "%d", c->think);
    v = sm_ini_get(ini, "connection", "think", tmp);
    c->think = (int)sm_to_long(v, c->think); sm_free(v);

    _snprintf(tmp, sizeof(tmp), "%d", c->thinkLevel);
    v = sm_ini_get(ini, "connection", "thinklevel", tmp);
    c->thinkLevel = (int)sm_to_long(v, c->thinkLevel); sm_free(v);

    _snprintf(tmp, sizeof(tmp), "%d", c->thinkStyle);
    v = sm_ini_get(ini, "connection", "thinkstyle", tmp);
    c->thinkStyle = (int)sm_to_long(v, c->thinkStyle); sm_free(v);

    _snprintf(tmp, sizeof(tmp), "%d", c->useTools);
    v = sm_ini_get(ini, "options", "usetools", tmp);
    c->useTools = (int)sm_to_long(v, c->useTools); sm_free(v);

    _snprintf(tmp, sizeof(tmp), "%d", c->ignoreCertDate);
    v = sm_ini_get(ini, "options", "ignorecertdate", tmp);
    c->ignoreCertDate = (int)sm_to_long(v, c->ignoreCertDate); sm_free(v);

    _snprintf(tmp, sizeof(tmp), "%d", c->confirmWrite);
    v = sm_ini_get(ini, "options", "confirmwrite", tmp);
    c->confirmWrite = (int)sm_to_long(v, c->confirmWrite); sm_free(v);

    _snprintf(tmp, sizeof(tmp), "%d", c->toolRounds);
    v = sm_ini_get(ini, "agent", "rounds", tmp);
    c->toolRounds = (int)sm_to_long(v, c->toolRounds); sm_free(v);

    /* 旧版本只有 confirmwrite（是/否）。没写过 perm 的，按它迁移。 */
    _snprintf(tmp, sizeof(tmp), "%d", c->confirmWrite ? 1 : 2);
    v = sm_ini_get(ini, "agent", "perm", tmp);
    c->permMode = (int)sm_to_long(v, c->permMode); sm_free(v);

    _snprintf(tmp, sizeof(tmp), "%d", c->reflow);
    v = sm_ini_get(ini, "display", "reflow", tmp);
    c->reflow = (int)sm_to_long(v, c->reflow); sm_free(v);

    if (c->toolRounds < 1) c->toolRounds = 1;
    if (c->toolRounds > 50) c->toolRounds = 50;
    if (c->permMode < 0 || c->permMode > 2) c->permMode = 2;
    c->confirmWrite = (c->permMode == 1) ? 1 : 0;   /* 保持旧字段一致 */

    _snprintf(tmp, sizeof(tmp), "%d", c->fontSize);
    v = sm_ini_get(ini, "display", "fontsize", tmp);
    c->fontSize = (int)sm_to_long(v, c->fontSize); sm_free(v);

    sm_free(ini);
    return 1;
}

/* ================= 多供应商配置 ================= */

static void key_enc(const char *plain, char *out, int outlen)
{
    char enc[512];
    size_t n;
    strncpy(enc, plain, sizeof(enc) - 1);
    enc[sizeof(enc) - 1] = 0;
    n = strlen(enc);
    obf_n(enc, n);
    sm_b64_encode((const unsigned char *)enc, n, out, outlen);
}

static void key_dec(const char *b64, char *out, int outlen)
{
    unsigned char raw[512];
    long n;
    out[0] = 0;
    if (!b64 || !b64[0]) return;
    n = sm_b64_decode(b64, (int)strlen(b64), raw);
    if (n <= 0 || n >= (long)sizeof(raw)) {
        strncpy(out, b64, (size_t)outlen - 1);
        out[outlen - 1] = 0;
        return;
    }
    obf_n((char *)raw, (size_t)n);
    raw[n] = 0;
    strncpy(out, (char *)raw, (size_t)outlen - 1);
    out[outlen - 1] = 0;
}

void sm_config_to_provider(const SmConfig *c, SmProvider *p, const char *name)
{
    memset(p, 0, sizeof(*p));
    if (name) { strncpy(p->name, name, sizeof(p->name) - 1); }
    strncpy(p->endpoint, c->endpoint, sizeof(p->endpoint) - 1);
    strncpy(p->model, c->model, sizeof(p->model) - 1);
    strncpy(p->apikey, c->apikey, sizeof(p->apikey) - 1);
    p->temperature10 = c->temperature10;
    p->maxtokens = c->maxtokens;
    p->timeoutSec = c->timeoutSec;
    p->think = c->think;
    p->thinkLevel = c->thinkLevel;
    p->thinkStyle = c->thinkStyle;
    p->vision = 0;
}

void sm_provider_to_config(const SmProviders *p, int i, SmConfig *c)
{
    const SmProvider *s;
    if (!p || i < 0 || i >= p->count) return;
    s = &p->list[i];
    /* 注意：这里只覆盖「属于这一个供应商」的字段。
     * 以前这里调用了 sm_config_defaults()，会把 [options]、[agent]、[display]
     * 里的设置（工具开关、权限、轮数、字号）统统打回默认值。 */
    strncpy(c->endpoint, s->endpoint, sizeof(c->endpoint) - 1);
    strncpy(c->model, s->model, sizeof(c->model) - 1);
    strncpy(c->apikey, s->apikey, sizeof(c->apikey) - 1);
    c->temperature10 = s->temperature10;
    c->maxtokens = s->maxtokens;
    c->timeoutSec = s->timeoutSec;
    c->think = s->think;
    c->thinkLevel = s->thinkLevel;
    c->thinkStyle = s->thinkStyle;
}

void sm_providers_load(SmProviders *p)
{
    char *ini;
    char sec[32];
    char tmp[64];
    char *v;
    int i;

    memset(p, 0, sizeof(*p));
    ini = sm_ini_path();
    v = sm_ini_get(ini, "connection", "count", "0");
    p->count = (int)sm_to_long(v, 0);
    sm_free(v);
    if (p->count < 0) p->count = 0;
    if (p->count > SM_MAX_PROVIDERS) p->count = SM_MAX_PROVIDERS;

    for (i = 0; i < p->count; i++) {
        SmProvider *s = &p->list[i];
        _snprintf(sec, sizeof(sec), "provider.%d", i + 1);
        v = sm_ini_get(ini, sec, "name", "");        strncpy(s->name, v, sizeof(s->name)-1); sm_free(v);
        v = sm_ini_get(ini, sec, "endpoint", "");    strncpy(s->endpoint, v, sizeof(s->endpoint)-1); sm_free(v);
        v = sm_ini_get(ini, sec, "model", "");       strncpy(s->model, v, sizeof(s->model)-1); sm_free(v);
        v = sm_ini_get(ini, sec, "apikey", "");      key_dec(v, s->apikey, sizeof(s->apikey)); sm_free(v);
        _snprintf(tmp, sizeof(tmp), "%d", 7);
        v = sm_ini_get(ini, sec, "temperature", tmp); s->temperature10 = (int)sm_to_long(v, 7); sm_free(v);
        _snprintf(tmp, sizeof(tmp), "%d", 2048);
        v = sm_ini_get(ini, sec, "maxtokens", tmp);  s->maxtokens = (int)sm_to_long(v, 2048); sm_free(v);
        _snprintf(tmp, sizeof(tmp), "%d", 60);
        v = sm_ini_get(ini, sec, "timeout", tmp);    s->timeoutSec = (int)sm_to_long(v, 60); sm_free(v);
        _snprintf(tmp, sizeof(tmp), "%d", 0);
        v = sm_ini_get(ini, sec, "think", tmp);      s->think = (int)sm_to_long(v, 0); sm_free(v);
        _snprintf(tmp, sizeof(tmp), "%d", 1);
        v = sm_ini_get(ini, sec, "thinklevel", tmp); s->thinkLevel = (int)sm_to_long(v, 1); sm_free(v);
        _snprintf(tmp, sizeof(tmp), "%d", 0);
        v = sm_ini_get(ini, sec, "thinkstyle", tmp); s->thinkStyle = (int)sm_to_long(v, 0); sm_free(v);
        _snprintf(tmp, sizeof(tmp), "%d", 0);
        v = sm_ini_get(ini, sec, "vision", tmp);     s->vision = (int)sm_to_long(v, 0); sm_free(v);
        if (!s->name[0]) _snprintf(s->name, sizeof(s->name), "Provider %d", i + 1);
    }

    v = sm_ini_get(ini, "connection", "active", "");
    p->active = 0;
    for (i = 0; i < p->count; i++)
        if (sm_stricmp(v, p->list[i].name) == 0) { p->active = i; break; }
    sm_free(v);

    /* 老配置（单段 [connection]）自动迁移成一行，用户无感 */
    if (p->count == 0) {
        SmConfig c;
        if (sm_config_load(&c) && c.endpoint[0]) {
            sm_config_to_provider(&c, &p->list[0], "默认");
            p->count = 1;
            p->active = 0;
        }
    }
    sm_free(ini);
}

int sm_providers_save(const SmProviders *p)
{
    char *ini;
    char sec[32];
    char tmp[64];
    char b64[1400];
    int i;

    ini = sm_ini_path();
    _snprintf(tmp, sizeof(tmp), "%d", p->count);
    sm_ini_set(ini, "connection", "count", tmp);
    sm_ini_set(ini, "connection", "active",
               (p->active >= 0 && p->active < p->count) ? p->list[p->active].name : "");

    for (i = 0; i < p->count; i++) {
        const SmProvider *s = &p->list[i];
        _snprintf(sec, sizeof(sec), "provider.%d", i + 1);
        sm_ini_set(ini, sec, "name", s->name);
        sm_ini_set(ini, sec, "endpoint", s->endpoint);
        sm_ini_set(ini, sec, "model", s->model);
        key_enc(s->apikey, b64, sizeof(b64));
        sm_ini_set(ini, sec, "apikey", b64);
        _snprintf(tmp, sizeof(tmp), "%d", s->temperature10); sm_ini_set(ini, sec, "temperature", tmp);
        _snprintf(tmp, sizeof(tmp), "%d", s->maxtokens);     sm_ini_set(ini, sec, "maxtokens", tmp);
        _snprintf(tmp, sizeof(tmp), "%d", s->timeoutSec);    sm_ini_set(ini, sec, "timeout", tmp);
        _snprintf(tmp, sizeof(tmp), "%d", s->think);         sm_ini_set(ini, sec, "think", tmp);
        _snprintf(tmp, sizeof(tmp), "%d", s->thinkLevel);    sm_ini_set(ini, sec, "thinklevel", tmp);
        _snprintf(tmp, sizeof(tmp), "%d", s->thinkStyle);    sm_ini_set(ini, sec, "thinkstyle", tmp);
        _snprintf(tmp, sizeof(tmp), "%d", s->vision);        sm_ini_set(ini, sec, "vision", tmp);
    }
    sm_free(ini);
    return 1;
}

int sm_config_save(const SmConfig *c)
{
    char *ini = sm_ini_path();
    char tmp[64];
    char enc[1024];

    sm_ini_set(ini, SM_SHORT_NAME, "Product", SM_SHORT_NAME);
    sm_ini_set(ini, SM_SHORT_NAME, "Version", SM_VERSION_STR);

    sm_ini_set(ini, "connection", "endpoint", c->endpoint);
    sm_ini_set(ini, "connection", "model", c->model);

    strcpy(enc, c->apikey);
    {
        /* 长度要在加掩码之前取：加掩码后可能出现 0x00，strlen 会截断。 */
        size_t klen = strlen(enc);
        char b64[1400];
        obf_n(enc, klen);
        sm_b64_encode((const unsigned char *)enc, klen, b64, 0);
        sm_ini_set(ini, "connection", "apikey", b64);
    }

    _snprintf(tmp, sizeof(tmp), "%d", c->temperature10);
    sm_ini_set(ini, "connection", "temperature", tmp);
    _snprintf(tmp, sizeof(tmp), "%d", c->maxtokens);
    sm_ini_set(ini, "connection", "maxtokens", tmp);
    _snprintf(tmp, sizeof(tmp), "%d", c->timeoutSec);
    sm_ini_set(ini, "connection", "timeout", tmp);

    _snprintf(tmp, sizeof(tmp), "%d", c->think);
    sm_ini_set(ini, "connection", "think", tmp);
    _snprintf(tmp, sizeof(tmp), "%d", c->thinkLevel);
    sm_ini_set(ini, "connection", "thinklevel", tmp);
    _snprintf(tmp, sizeof(tmp), "%d", c->thinkStyle);
    sm_ini_set(ini, "connection", "thinkstyle", tmp);

    _snprintf(tmp, sizeof(tmp), "%d", c->useTools);
    sm_ini_set(ini, "options", "usetools", tmp);
    _snprintf(tmp, sizeof(tmp), "%d", c->ignoreCertDate);
    sm_ini_set(ini, "options", "ignorecertdate", tmp);
    _snprintf(tmp, sizeof(tmp), "%d", c->confirmWrite);
    sm_ini_set(ini, "options", "confirmwrite", tmp);
    _snprintf(tmp, sizeof(tmp), "%d", c->toolRounds);
    sm_ini_set(ini, "agent", "rounds", tmp);
    _snprintf(tmp, sizeof(tmp), "%d", c->permMode);
    sm_ini_set(ini, "agent", "perm", tmp);
    _snprintf(tmp, sizeof(tmp), "%d", c->fontSize);
    sm_ini_set(ini, "display", "fontsize", tmp);
    _snprintf(tmp, sizeof(tmp), "%d", c->reflow);
    sm_ini_set(ini, "display", "reflow", tmp);
    sm_free(ini);
    return 1;
}
