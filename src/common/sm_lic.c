/* NeuroBench 2003 - CDK 授权码与试用期管理 */
#include "sm.h"

#define CDK_ALPHA   "23456789ABCDEFGHJKLMNPQRSTUVWXYZ"
#define CDK_LEN     20           /* 20 字符 x 5 bit = 100 bit */
#define CDK_GROUPS  5
#define CDK_GRPLEN  4

/* 厂商私钥（那个年代的“加密狗”其实就是这么硬编码的） */
static const char CDK_SECRET[] = "Axiom-NeuroBench-2003-CDKEY-S3CR3T-0x5A71";

#define TRIAL_DAYS  30
#define TRIAL_MSGS  30

/* ------------------------------------------------------------------ */
/* 位流辅助                                                            */
/* ------------------------------------------------------------------ */
static int alpha_index(char c)
{
    const char *p = strchr(CDK_ALPHA, c);
    if (!p) return -1;
    return (int)(p - CDK_ALPHA);
}

/* bits: 100 个 0/1 */
static void bits_put(unsigned char *bits, int pos, int n, unsigned int val)
{
    int i;
    for (i = 0; i < n; i++)
        bits[pos + i] = (unsigned char)((val >> (n - 1 - i)) & 1);
}

static unsigned int bits_get(const unsigned char *bits, int pos, int n)
{
    unsigned int v = 0;
    int i;
    for (i = 0; i < n; i++) v = (v << 1) | bits[pos + i];
    return v;
}

void sm_cdk_normalize(const char *in, char out[32])
{
    int n = 0;
    if (!in) { out[0] = 0; return; }
    for (; *in && n < 31; in++) {
        char c = *in;
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if (c == 'O') c = '0';
        if (c == '0' || c == '1') continue;         /* 字母表里没有 0/1 */
        if (c == 'I') c = 'J';
        if (strchr(CDK_ALPHA, c)) out[n++] = c;
    }
    out[n] = 0;
}

int sm_cdk_generate(int edition, int seats, int days, unsigned int serial,
                    char out[32])
{
    unsigned char bits[CDK_LEN * 5];
    unsigned char body[8];
    unsigned char mac[20];
    unsigned int salt;
    int i, o = 0;

    if (edition < 1 || edition > 3) return 0;
    if (seats < 1) seats = 1;
    if (seats > 32) seats = 32;
    if (days < 0) days = 0;
    if (days > 511) days = 511;

    sm_random_bytes(&salt, sizeof(salt));
    salt &= 0xFFFFF;

    memset(bits, 0, sizeof(bits));
    bits_put(bits, 0, 2, (unsigned int)(edition - 1));
    bits_put(bits, 2, 5, (unsigned int)(seats - 1));
    bits_put(bits, 7, 9, (unsigned int)days);
    bits_put(bits, 16, 24, serial & 0xFFFFFF);
    bits_put(bits, 40, 20, salt);

    /* 60 位主体 -> 8 字节（大端右对齐） */
    memset(body, 0, sizeof(body));
    for (i = 0; i < 8; i++) {
        unsigned int v = 0;
        int b;
        for (b = 0; b < 8; b++) {
            int bitpos = i * 8 + b - 4;            /* 8 字节共 64 位，前 4 位补 0 */
            v = (v << 1) | ((bitpos >= 0 && bitpos < 60) ? bits[bitpos] : 0);
        }
        body[i] = (unsigned char)v;
    }
    sm_hmac_sha1(CDK_SECRET, sizeof(CDK_SECRET) - 1, body, sizeof(body), mac);

    for (i = 0; i < 40; i++)
        bits[60 + i] = (unsigned char)((mac[i / 8] >> (7 - (i % 8))) & 1);

    for (i = 0; i < CDK_LEN; i++) {
        unsigned int v = bits_get(bits, i * 5, 5);
        if (i && (i % CDK_GRPLEN) == 0) out[o++] = '-';
        out[o++] = CDK_ALPHA[v];
    }
    out[o] = 0;
    return 1;
}

int sm_cdk_verify(const char *cdk, int *edition, int *seats, int *days,
                  unsigned int *serial)
{
    char norm[32];
    unsigned char bits[CDK_LEN * 5];
    unsigned char body[8];
    unsigned char mac[20];
    int i;

    sm_cdk_normalize(cdk, norm);
    if ((int)strlen(norm) != CDK_LEN) return 0;

    for (i = 0; i < CDK_LEN; i++) {
        int v = alpha_index(norm[i]);
        if (v < 0) return 0;
        bits_put(bits, i * 5, 5, (unsigned int)v);
    }

    memset(body, 0, sizeof(body));
    for (i = 0; i < 8; i++) {
        unsigned int v = 0;
        int b;
        for (b = 0; b < 8; b++) {
            int bitpos = i * 8 + b - 4;
            v = (v << 1) | ((bitpos >= 0 && bitpos < 60) ? bits[bitpos] : 0);
        }
        body[i] = (unsigned char)v;
    }
    sm_hmac_sha1(CDK_SECRET, sizeof(CDK_SECRET) - 1, body, sizeof(body), mac);

    for (i = 0; i < 40; i++) {
        unsigned char want = (unsigned char)((mac[i / 8] >> (7 - (i % 8))) & 1);
        if (bits[60 + i] != want) return 0;
    }

    if (edition) *edition = (int)bits_get(bits, 0, 2) + 1;
    if (seats)   *seats = (int)bits_get(bits, 2, 5) + 1;
    if (days)    *days = (int)bits_get(bits, 7, 9);
    if (serial)  *serial = bits_get(bits, 16, 24);
    return 1;
}

/* ------------------------------------------------------------------ */
/* 授权状态                                                            */
/* ------------------------------------------------------------------ */
#define LIC_FILE_NAME "NEBENCH.LIC"

static char *lic_file_path(void)
{
    char *d = sm_appdata_dir();
    char *p = sm_path_join(d, LIC_FILE_NAME);
    sm_free(d);
    return p;
}

static DWORD lic_checksum(DWORD firstrun, DWORD msgs, const char *cdk)
{
    Sb b;
    char tmp[512];
    DWORD c;
    sb_init(&b);
    _snprintf(tmp, sizeof(tmp), "%lu|%lu|%s|SM2000",
              (unsigned long)firstrun, (unsigned long)msgs, cdk ? cdk : "");
    tmp[sizeof(tmp) - 1] = 0;
    sb_puts(&b, tmp);
    c = sm_crc32(sb_str(&b), b.len);
    sb_free(&b);
    return c;
}

void sm_license_read(SmLicense *lic)
{
    DWORD firstrun, msgs, chk;
    char *cdk;
    char *lfile;
    int have_reg = 0;

    memset(lic, 0, sizeof(*lic));

    firstrun = sm_reg_get_dw(HKEY_CURRENT_USER, SM_REG_ROOT, "FirstRun", 0);
    msgs     = sm_reg_get_dw(HKEY_CURRENT_USER, SM_REG_ROOT, "TrialMsgs", 0);
    chk      = sm_reg_get_dw(HKEY_CURRENT_USER, SM_REG_ROOT, "Check", 0);
    cdk      = sm_reg_get_sz(HKEY_CURRENT_USER, SM_REG_ROOT, "CDK");
    if (firstrun || cdk) have_reg = 1;
    if (!cdk) cdk = sm_strdup("");

    /* 镜像授权文件 */
    lfile = lic_file_path();
    if (!have_reg) {
        char *txt = sm_read_file(lfile, 4096, NULL);
        if (txt) {
            /* 格式: FirstRun=<n>\r\nTrialMsgs=<n>\r\nCDK=<..>\r\nCheck=<n> */
            char *p;
            p = sm_stristr(txt, "FirstRun="); if (p) firstrun = (DWORD)sm_to_long(p + 9, 0);
            p = sm_stristr(txt, "TrialMsgs="); if (p) msgs = (DWORD)sm_to_long(p + 10, 0);
            p = sm_stristr(txt, "CDK=");
            if (p) {
                char buf[64]; int i = 0;
                p += 4;
                while (p[i] && p[i] != '\r' && p[i] != '\n' && i < 63) { buf[i] = p[i]; i++; }
                buf[i] = 0;
                sm_free(cdk);
                cdk = sm_strdup(buf);
            }
            p = sm_stristr(txt, "Check="); if (p) chk = (DWORD)sm_to_long(p + 6, 0);
            sm_free(txt);
            have_reg = (firstrun != 0);
        }
    }
    sm_free(lfile);

    if (!firstrun) {
        /* 首次运行 */
        firstrun = sm_now();
        msgs = 0;
        chk = lic_checksum(firstrun, msgs, cdk);
        sm_reg_set_dw(HKEY_CURRENT_USER, SM_REG_ROOT, "FirstRun", firstrun);
        sm_reg_set_dw(HKEY_CURRENT_USER, SM_REG_ROOT, "TrialMsgs", msgs);
        sm_reg_set_dw(HKEY_CURRENT_USER, SM_REG_ROOT, "Check", chk);
        sm_reg_set_sz(HKEY_CURRENT_USER, SM_REG_ROOT, "CDK", cdk);
    } else if (chk != lic_checksum(firstrun, msgs, cdk)) {
        lic->tampered = 1;
    }

    lic->install_time = firstrun;

    /* 已激活？ */
    if (cdk[0]) {
        int ed, se, dy;
        unsigned int sn;
        if (sm_cdk_verify(cdk, &ed, &se, &dy, &sn)) {
            lic->state = SM_LIC_ACTIVATED;
            lic->edition = ed;
            lic->seats = se;
            strncpy(lic->cdk, cdk, sizeof(lic->cdk) - 1);
            if (dy > 0) {
                lic->expiry_time = firstrun + (DWORD)dy * 86400u;
                if (sm_now() > lic->expiry_time) lic->state = SM_LIC_EXPIRED;
            }
            sm_free(cdk);
            if (lic->tampered) lic->state = SM_LIC_EXPIRED;
            return;
        }
    }
    sm_free(cdk);

    /* 试用 */
    lic->edition = SM_ED_STANDARD;
    lic->seats = 1;
    {
        DWORD now = sm_now();
        int used_days = (int)((now - firstrun) / 86400u);
        lic->trial_days_left = TRIAL_DAYS - used_days;
        lic->trial_msgs_left = TRIAL_MSGS - (int)msgs;
        if (lic->trial_days_left < 0) lic->trial_days_left = 0;
        if (lic->trial_msgs_left < 0) lic->trial_msgs_left = 0;
        if (lic->tampered || (lic->trial_days_left == 0 && lic->trial_msgs_left == 0))
            lic->state = SM_LIC_EXPIRED;
        else
            lic->state = SM_LIC_TRIAL;
    }
}

static void lic_write_mirror(DWORD firstrun, DWORD msgs, const char *cdk, DWORD chk)
{
    Sb b;
    char *p = lic_file_path();
    sb_init(&b);
    sb_printf(&b, "[license]\r\nFirstRun=%lu\r\nTrialMsgs=%lu\r\nCDK=%s\r\nCheck=%lu\r\n",
              (unsigned long)firstrun, (unsigned long)msgs, cdk, (unsigned long)chk);
    sm_write_file(p, sb_str(&b), (long)b.len);
    sb_free(&b);
    sm_free(p);
}

int sm_license_activate(const char *cdk, SmLicense *out, char *err, int errlen)
{
    int ed, se, dy;
    unsigned int sn;
    char norm[32];
    DWORD firstrun, msgs, chk;
    char fmt[40];
    int i, o = 0;

    if (err && errlen) err[0] = 0;
    sm_cdk_normalize(cdk, norm);
    if (!norm[0]) {
        if (err) { strncpy(err, "请输入产品序列号（CDK）。", errlen - 1); err[errlen - 1] = 0; }
        return 0;
    }
    if (!sm_cdk_verify(norm, &ed, &se, &dy, &sn)) {
        if (err) {
            strncpy(err, "产品序列号无效。请检查输入是否正确，"
                         "或与经销商联系更换。", errlen - 1);
            err[errlen - 1] = 0;
        }
        return 0;
    }

    /* 统一成 XXXX-XXXX-XXXX-XXXX-XXXX 形式保存 */
    for (i = 0; norm[i]; i++) {
        if (i && (i % CDK_GRPLEN) == 0) fmt[o++] = '-';
        fmt[o++] = norm[i];
    }
    fmt[o] = 0;

    firstrun = sm_reg_get_dw(HKEY_CURRENT_USER, SM_REG_ROOT, "FirstRun", 0);
    if (!firstrun) firstrun = sm_now();
    msgs = sm_reg_get_dw(HKEY_CURRENT_USER, SM_REG_ROOT, "TrialMsgs", 0);
    chk = lic_checksum(firstrun, msgs, fmt);

    sm_reg_set_sz(HKEY_CURRENT_USER, SM_REG_ROOT, "CDK", fmt);
    sm_reg_set_dw(HKEY_CURRENT_USER, SM_REG_ROOT, "FirstRun", firstrun);
    sm_reg_set_dw(HKEY_CURRENT_USER, SM_REG_ROOT, "Check", chk);
    sm_reg_set_dw(HKEY_CURRENT_USER, SM_REG_ROOT, "Edition", (DWORD)ed);
    sm_reg_set_dw(HKEY_CURRENT_USER, SM_REG_ROOT, "Seats", (DWORD)se);
    lic_write_mirror(firstrun, msgs, fmt, chk);

    sm_license_read(out);
    return out->state == SM_LIC_ACTIVATED;
}

void sm_license_consume_trial_message(void)
{
    DWORD firstrun, msgs, chk;
    char *cdk;
    char *lfile;
    SmLicense lic;

    sm_license_read(&lic);
    if (lic.state != SM_LIC_TRIAL) return;

    firstrun = sm_reg_get_dw(HKEY_CURRENT_USER, SM_REG_ROOT, "FirstRun", sm_now());
    msgs = sm_reg_get_dw(HKEY_CURRENT_USER, SM_REG_ROOT, "TrialMsgs", 0) + 1;
    cdk = sm_reg_get_sz(HKEY_CURRENT_USER, SM_REG_ROOT, "CDK");
    if (!cdk) cdk = sm_strdup("");
    chk = lic_checksum(firstrun, msgs, cdk);
    sm_reg_set_dw(HKEY_CURRENT_USER, SM_REG_ROOT, "TrialMsgs", msgs);
    sm_reg_set_dw(HKEY_CURRENT_USER, SM_REG_ROOT, "Check", chk);
    lic_write_mirror(firstrun, msgs, cdk, chk);
    sm_free(cdk);
    /* 双写一份到 AppData 目录，防止只删注册表 */
    lfile = lic_file_path();
    sm_free(lfile);
}

int sm_license_edition_name(int edition, char *buf, int len)
{
    const char *n;
    switch (edition) {
    case SM_ED_PROFESSIONAL: n = "专业版"; break;
    case SM_ED_ENTERPRISE:   n = "企业版"; break;
    default:                 n = "标准版"; break;
    }
    if (!buf || len <= 0) return 0;
    strncpy(buf, n, (size_t)len - 1);
    buf[len - 1] = 0;
    return 1;
}

const char *sm_license_state_text(const SmLicense *lic, char *buf, int len)
{
    char ed[32];
    sm_license_edition_name(lic->edition, ed, sizeof(ed));
    switch (lic->state) {
    case SM_LIC_ACTIVATED:
        _snprintf(buf, (size_t)len, "已授权 (%s)", ed);
        break;
    case SM_LIC_EXPIRED:
        _snprintf(buf, (size_t)len, lic->tampered ? "授权已损坏" : "试用已到期");
        break;
    default:
        _snprintf(buf, (size_t)len, "试用版 剩余%d天/%d次",
                  lic->trial_days_left, lic->trial_msgs_left);
        break;
    }
    buf[len - 1] = 0;
    return buf;
}
