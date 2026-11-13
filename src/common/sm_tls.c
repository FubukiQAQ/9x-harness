/* =====================================================================
 * Axiom NeuroBench 2003 - TLS 1.2 HTTPS 客户端
 *
 * Windows XP 的 SChannel 只支持到 TLS 1.0，所以本程序自带 BearSSL，
 * 自己完成 TLS 1.2 握手、证书链校验与加解密，不依赖系统任何加密组件。
 * ===================================================================== */
#include "sm.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include "bearssl.h"

/* 解析 HTTP 的 Date 头，支持两种格式：
 *   Sun, 06 Nov 1994 08:49:37 GMT     (RFC 1123)
 *   Sun Nov  6 08:49:37 1994          (asctime)
 * 不用 sscanf：mingw 的 sscanf 会链进按 Pentium Pro 编译的格式化代码（含 CMOV），
 * 在 Pentium MMX 上直接非法指令。这里手写分词。 */
static long long parse_http_date(const char *p)
{
    static const char *M[] = { "Jan","Feb","Mar","Apr","May","Jun",
                               "Jul","Aug","Sep","Oct","Nov","Dec" };
    char tmp[80];
    char *tok[12];
    int nt = 0, i, mi = -1, day = 0, year = 0, hh = 0, mm = 0, ss = 0;
    struct tm tm;
    size_t k;

    strncpy(tmp, p, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = 0;
    for (k = 0; tmp[k]; k++) if (tmp[k] == ',') tmp[k] = ' ';

    {
        char *q = tmp;
        while (*q && nt < 12) {
            while (*q == ' ' || *q == '\t') q++;
            if (!*q) break;
            tok[nt++] = q;
            while (*q && *q != ' ' && *q != '\t') q++;
            if (*q) *q++ = 0;
        }
    }

    for (i = 0; i < nt; i++) {
        const char *t = tok[i];
        int allnum = 1, hascolon = 0;
        const char *c;
        int m;

        for (c = t; *c; c++) {
            if (*c == ':') hascolon = 1;
            else if (*c < '0' || *c > '9') allnum = 0;
        }

        if (hascolon) {
            int v[3] = { 0, 0, 0 }, idx = 0;
            const char *q2 = t;
            while (*q2 && idx < 3) {
                while (*q2 >= '0' && *q2 <= '9') { v[idx] = v[idx] * 10 + (*q2 - '0'); q2++; }
                idx++;
                if (*q2 == ':') q2++;
                else break;
            }
            hh = v[0]; mm = v[1]; ss = v[2];
        } else if (allnum && *t) {
            int v = (int)strtol(t, NULL, 10);
            if (strlen(t) >= 4) year = v;
            else if (!day) day = v;
        } else {
            for (m = 0; m < 12; m++)
                if (!_strnicmp(t, M[m], 3)) { mi = m; break; }
        }
    }

    if (mi < 0 || day <= 0 || year < 1970) return 0;

    ZeroMemory(&tm, sizeof(tm));
    tm.tm_mday = day;
    tm.tm_mon = mi;
    tm.tm_year = year - 1900;
    tm.tm_hour = hh;
    tm.tm_min = mm;
    tm.tm_sec = ss;
    tm.tm_isdst = 0;
    return (long long)_mkgmtime(&tm);
}



/* ------------------------------------------------------------------ */
static int g_wsa = 0;

static int net_init(void)
{
    WSADATA wd;
    if (g_wsa) return 1;
    if (WSAStartup(MAKEWORD(2, 2), &wd) != 0) return 0;
    g_wsa = 1;
    return 1;
}

/* ---------------- 简易 DER 解析 ---------------- */
static int der_len(const unsigned char *p, const unsigned char *end, long *len, int *hlen)
{
    int h;
    long v;
    if (p >= end) return 0;
    if (*p < 0x80) { *len = *p; *hlen = 1; return 1; }
    h = *p & 0x7F;
    if (h < 1 || h > 4) return 0;
    if (p + 1 + h > end) return 0;
    v = 0;
    {
        int i;
        for (i = 0; i < h; i++) v = (v << 8) | p[1 + i];
    }
    *len = v;
    *hlen = 1 + h;
    return 1;
}

static int der_child(const unsigned char *p, const unsigned char *end, int idx,
                     const unsigned char **out, const unsigned char **outend)
{
    long len;
    int hlen;
    const unsigned char *q;
    int i;
    if (p >= end) return 0;
    if (!der_len(p + 1, end, &len, &hlen)) return 0;
    q = p + 1 + hlen;
    if (q + len > end) return 0;
    for (i = 0; i <= idx; i++) {
        long clen;
        int ch;
        if (q >= end) return 0;
        if (!der_len(q + 1, end, &clen, &ch)) return 0;
        if (q + 1 + ch + clen > end) return 0;
        if (i == idx) { *out = q; *outend = q + 1 + ch + clen; return 1; }
        q += 1 + ch + clen;
    }
    return 0;
}

static unsigned char *cert_subject_dn(const unsigned char *der, size_t dlen, size_t *outlen)
{
    const unsigned char *end = der + dlen;
    const unsigned char *tbs, *tbsend;
    const unsigned char *first, *firstend;
    const unsigned char *subj, *subjend;
    int idx;

    /* Certificate ::= SEQUENCE { tbsCertificate, ... } -> 取第 0 个元素 */
    if (!der_child(der, end, 0, &tbs, &tbsend)) return NULL;
    /* tbsCertificate ::= SEQUENCE { [0] version OPTIONAL, serial, sigalg, issuer,
       validity, subject, subjectPublicKeyInfo, ... } */
    if (!der_child(tbs, tbsend, 0, &first, &firstend)) return NULL;
    idx = (*first == 0xA0) ? 5 : 4;
    if (!der_child(tbs, tbsend, idx, &subj, &subjend)) return NULL;
    *outlen = (size_t)(subjend - subj);
    {
        unsigned char *r = (unsigned char *)sm_alloc(*outlen);
        memcpy(r, subj, *outlen);
        return r;
    }
}

static br_x509_pkey *pkey_copy(const br_x509_pkey *src)
{
    br_x509_pkey *d = (br_x509_pkey *)sm_calloc(sizeof(br_x509_pkey));
    d->key_type = src->key_type;
    if (src->key_type == BR_KEYTYPE_RSA) {
        d->key.rsa.nlen = src->key.rsa.nlen;
        d->key.rsa.elen = src->key.rsa.elen;
        d->key.rsa.n = (unsigned char *)sm_alloc(src->key.rsa.nlen);
        memcpy(d->key.rsa.n, src->key.rsa.n, src->key.rsa.nlen);
        d->key.rsa.e = (unsigned char *)sm_alloc(src->key.rsa.elen);
        memcpy(d->key.rsa.e, src->key.rsa.e, src->key.rsa.elen);
    } else if (src->key_type == BR_KEYTYPE_EC) {
        d->key.ec.curve = src->key.ec.curve;
        d->key.ec.qlen = src->key.ec.qlen;
        d->key.ec.q = (unsigned char *)sm_alloc(src->key.ec.qlen);
        memcpy(d->key.ec.q, src->key.ec.q, src->key.ec.qlen);
    }
    return d;
}

static int ta_add(br_x509_trust_anchor **tas, size_t *num, size_t *cap,
                  const unsigned char *der, size_t dlen)
{
    br_x509_decoder_context dc;
    br_x509_pkey *pk;
    unsigned char *dn;
    size_t dnlen;
    br_x509_trust_anchor *ta;

    br_x509_decoder_init(&dc, NULL, NULL);
    br_x509_decoder_push(&dc, der, dlen);
    pk = br_x509_decoder_get_pkey(&dc);
    if (!pk) return 0;
    dn = cert_subject_dn(der, dlen, &dnlen);
    if (!dn) return 0;

    if (*num == *cap) {
        *cap = *cap ? (*cap * 2) : 64;
        *tas = (br_x509_trust_anchor *)sm_realloc(*tas, sizeof(br_x509_trust_anchor) * (*cap));
    }
    ta = &(*tas)[*num];
    ZeroMemory(ta, sizeof(*ta));
    ta->dn.data = dn;
    ta->dn.len = dnlen;
    ta->flags = BR_X509_TA_CA;
    ta->pkey = *pkey_copy(pk);
    (*num)++;
    return 1;
}

static void pem_sink(void *cc, const void *data, size_t len)
{
    sb_putn((Sb *)cc, data, len);
}

/* 解一段 PEM 文本（可含多个证书），逐个加入信任锚 */
static int ta_add_pem(br_x509_trust_anchor **tas, size_t *num, size_t *cap,
                      const char *pem, size_t plen)
{
    br_pem_decoder_context pd;
    Sb cur;
    size_t u = 0;
    int added = 0, inobj = 0, iscert = 0, extra_nl = 1;

    sb_init(&cur);
    br_pem_decoder_init(&pd);
    while (plen > 0) {
        size_t n = br_pem_decoder_push(&pd, pem + u, plen);
        int ev;
        u += n;
        plen -= n;
        ev = br_pem_decoder_event(&pd);
        if (ev == BR_PEM_BEGIN_OBJ) {
            const char *nm = br_pem_decoder_name(&pd);
            inobj = 1;
            iscert = (nm && strstr(nm, "CERTIFICATE") && !strstr(nm, "REQUEST") &&
                      !strstr(nm, "PRIVATE"));
            sb_clear(&cur);
            br_pem_decoder_setdest(&pd, pem_sink, &cur);
        } else if (ev == BR_PEM_END_OBJ) {
            inobj = 0;
            if (iscert && cur.len > 64 &&
                ta_add(tas, num, cap, (const unsigned char *)sb_str(&cur), cur.len))
                added++;
            sb_clear(&cur);
        } else if (ev == BR_PEM_ERROR) {
            break;
        }
        if (plen == 0 && extra_nl) {
            extra_nl = 0;
            pem = "\n";
            u = 0;
            plen = 1;
        }
        if (n == 0 && plen > 0) break;
    }
    sb_free(&cur);
    return added;
}

/* ---------------- 信任锚缓存 ---------------- */
static br_x509_trust_anchor *g_tas = NULL;
static size_t g_tas_num = 0;
static int g_tas_state = 0;   /* 0=未加载 1=已加载 -1=失败 */

static size_t ca_load(void)
{
    const char *dirs[] = { "CERTS", "certs", "CA", "SSL" };
    char *base;
    size_t cap = 0;
    int i;

    if (g_tas_state) return g_tas_num;
    g_tas_state = -1;
    base = sm_app_dir();
    for (i = 0; i < (int)(sizeof(dirs) / sizeof(dirs[0])); i++) {
        char *dir = sm_path_join(base, dirs[i]);
        char *pat = sm_path_join(dir, "*.*");
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pat, &fd);
        sm_free(pat);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                char *full;
                char *data;
                long len;
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                full = sm_path_join(dir, fd.cFileName);
                len = sm_file_size(full);
                if (len <= 0 || len > 6 * 1024 * 1024) { sm_free(full); continue; }
                data = sm_read_file(full, len, &len);
                sm_free(full);
                if (!data) continue;
                if (strstr(data, "-----BEGIN"))
                    ta_add_pem(&g_tas, &g_tas_num, &cap, data, (size_t)len);
                else
                    ta_add(&g_tas, &g_tas_num, &cap, (const unsigned char *)data, (size_t)len);
                sm_free(data);
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
        sm_free(dir);
        if (g_tas_num > 0) break;
    }
    sm_free(base);
    if (g_tas_num > 0) g_tas_state = 1;
    return g_tas_num;
}

/* ------------------------------------------------------------------ */
static SOCKET tcp_connect(const char *host, int port, int timeout_ms,
                          char *err, int errlen)
{
    struct hostent *he;
    unsigned long addr;
    SOCKET s;
    struct sockaddr_in sa;
    u_long nb = 1;
    int sel;
    fd_set wf, ef;
    struct timeval tv;

    addr = inet_addr(host);
    if (addr == INADDR_NONE) {
        he = gethostbyname(host);
        if (!he || he->h_addrtype != AF_INET || !he->h_addr_list[0]) {
            _snprintf(err, errlen, "无法解析服务器地址“%s”。\r\n\r\n"
                      "请检查：\r\n"
                      "  1. 网络是否已经连通（可以用浏览器打开任意网页确认）；\r\n"
                      "  2. 服务器地址是否输入正确；\r\n"
                      "  3. 域名服务器(DNS)设置是否正确。", host);
            err[errlen - 1] = 0;
            return INVALID_SOCKET;
        }
        memcpy(&addr, he->h_addr_list[0], 4);
    }

    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        _snprintf(err, errlen, "无法创建网络套接字（错误码 %d）。", WSAGetLastError());
        err[errlen - 1] = 0;
        return INVALID_SOCKET;
    }

    ZeroMemory(&sa, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((unsigned short)port);
    sa.sin_addr.s_addr = addr;

    ioctlsocket(s, FIONBIO, &nb);
    if (connect(s, (struct sockaddr *)&sa, sizeof(sa)) == SOCKET_ERROR) {
        int e = WSAGetLastError();
        if (e != WSAEWOULDBLOCK) {
            _snprintf(err, errlen, "无法连接到服务器（错误码 %d）。", e);
            err[errlen - 1] = 0;
            closesocket(s);
            return INVALID_SOCKET;
        }
        FD_ZERO(&wf); FD_SET(s, &wf);
        FD_ZERO(&ef); FD_SET(s, &ef);
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        sel = select(0, NULL, &wf, &ef, &tv);
        if (sel <= 0) {
            _snprintf(err, errlen, "连接服务器超时（%d 秒）。\r\n\r\n"
                      "本机可能位于防火墙或代理服务器之后，请检查网络设置。",
                      timeout_ms / 1000);
            err[errlen - 1] = 0;
            closesocket(s);
            return INVALID_SOCKET;
        }
    }
    nb = 0;
    ioctlsocket(s, FIONBIO, &nb);
    {
        int tv2 = timeout_ms;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv2, sizeof(tv2));
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv2, sizeof(tv2));
    }
    return s;
}

/* ---------------- 连接对象 ---------------- */
typedef struct {
    SOCKET                  s;
    int                     is_tls;
    br_ssl_client_context  *cc;
    br_x509_minimal_context *xc;
    br_sslio_context        ioc;
    unsigned char          *iobuf;
    volatile LONG          *cancel;
} Conn;

static int low_read(void *ctx, unsigned char *buf, size_t len)
{
    Conn *c = (Conn *)ctx;
    int n;
    if (c->cancel && *c->cancel) return -1;
    n = recv(c->s, (char *)buf, (int)len, 0);
    if (n == SOCKET_ERROR) return -1;
    return n;
}

static int low_write(void *ctx, const unsigned char *buf, size_t len)
{
    Conn *c = (Conn *)ctx;
    int n;
    if (c->cancel && *c->cancel) return -1;
    n = send(c->s, (const char *)buf, (int)len, 0);
    if (n == SOCKET_ERROR) return -1;
    return n;
}

static int conn_write_all(Conn *c, const void *data, size_t len)
{
    const char *p = (const char *)data;
    if (c->is_tls) return br_sslio_write_all(&c->ioc, data, len) < 0 ? -1 : 0;
    while (len > 0) {
        int n = send(c->s, p, (int)len, 0);
        if (n <= 0) return -1;
        p += n; len -= (size_t)n;
    }
    return 0;
}

static int conn_flush(Conn *c)
{
    if (!c->is_tls) return 0;
    return br_sslio_flush(&c->ioc) < 0 ? -1 : 0;
}

/* >0: 字节数, 0: 正常结束, -1: 出错 */
static int conn_read(Conn *c, void *buf, size_t len)
{
    if (c->is_tls) {
        int n = br_sslio_read(&c->ioc, buf, len);
        if (n >= 0) return n;
        if (br_ssl_engine_last_error(&c->cc->eng) == 0) {
            if (br_ssl_engine_current_state(&c->cc->eng) & BR_SSL_CLOSED) return 0;
            return 0;
        }
        return -1;
    } else {
        int n = recv(c->s, (char *)buf, (int)len, 0);
        if (n < 0) return -1;
        return n;
    }
}

static const char *tls_err_text(unsigned e, char *buf, int len)
{
    const char *m;
    switch (e) {
    case 0:  m = "无错误"; break;
    case BR_ERR_BAD_PARAM:          m = "参数错误"; break;
    case BR_ERR_BAD_STATE:          m = "状态错误"; break;
    case BR_ERR_UNSUPPORTED_VERSION: m = "服务器要求的 TLS 版本本程序不支持"; break;
    case BR_ERR_BAD_VERSION:        m = "TLS 版本不匹配"; break;
    case BR_ERR_BAD_LENGTH:         m = "握手数据长度错误"; break;
    case BR_ERR_TOO_LARGE:          m = "握手数据过大"; break;
    case BR_ERR_BAD_MAC:            m = "消息校验码错误（数据被篡改）"; break;
    case BR_ERR_NO_RANDOM:          m = "随机数不足"; break;
    case BR_ERR_UNKNOWN_TYPE:       m = "未知的握手消息"; break;
    case BR_ERR_UNEXPECTED:         m = "收到意外的握手消息"; break;
    case BR_ERR_BAD_CCS:            m = "ChangeCipherSpec 错误"; break;
    case BR_ERR_BAD_ALERT:          m = "服务器返回致命警报（可能不支持 TLS 1.2 或协议不符）"; break;
    case BR_ERR_BAD_HANDSHAKE:      m = "握手消息格式错误"; break;
    case BR_ERR_OVERSIZED_ID:       m = "会话标识过长"; break;
    case BR_ERR_BAD_CIPHER_SUITE:   m = "没有双方都支持的加密套件（服务器可能要求 TLS 1.3）"; break;
    case BR_ERR_BAD_COMPRESSION:    m = "压缩方式不支持"; break;
    case BR_ERR_BAD_FRAGLEN:        m = "分片长度错误"; break;
    case BR_ERR_BAD_SECRENEG:       m = "会话重协商错误"; break;
    case BR_ERR_EXTRA_EXTENSION:    m = "服务器返回了未知的扩展"; break;
    case BR_ERR_BAD_SNI:            m = "服务器名称(SNI)错误"; break;
    case BR_ERR_BAD_HELLO_DONE:     m = "ServerHelloDone 错误"; break;
    case BR_ERR_LIMIT_EXCEEDED:     m = "超出协议限制"; break;
    case BR_ERR_BAD_FINISHED:       m = "Finished 校验失败"; break;
    case BR_ERR_RESUME_MISMATCH:    m = "会话恢复不匹配"; break;
    case BR_ERR_INVALID_ALGORITHM:  m = "非法的算法"; break;
    case BR_ERR_BAD_SIGNATURE:      m = "服务器签名校验失败"; break;
    case BR_ERR_WRONG_KEY_USAGE:    m = "服务器密钥用途不符"; break;
    case BR_ERR_NO_CLIENT_AUTH:     m = "服务器要求客户端证书"; break;
    case BR_ERR_IO:                 m = "网络读写失败（连接被中断）"; break;
    case BR_ERR_X509_EXPIRED:       m = "服务器证书已过期或尚未生效（本机系统时间可能不正确）"; break;
    case BR_ERR_X509_NOT_TRUSTED:   m = "服务器证书不受信任（缺少根证书，请运行『证书更新程序』）"; break;
    case BR_ERR_X509_BAD_SERVER_NAME: m = "服务器证书中的域名与访问的域名不一致"; break;
    case BR_ERR_X509_TIME_UNKNOWN:  m = "无法确定当前时间，证书日期无法校验"; break;
#ifdef BR_ERR_X509_SIG
    case BR_ERR_X509_SIG:           m = "证书签名无效"; break;
#endif
    default: m = NULL; break;
    }
    if (m) _snprintf(buf, (size_t)len, "%s（代码 %u）", m, e);
    else _snprintf(buf, (size_t)len, "安全连接层错误（代码 %u）", e);
    buf[len - 1] = 0;
    return buf;
}

static void set_x509_time(br_x509_minimal_context *xc, long long override_sec)
{
    time_t t;
    unsigned long days, secs;
    t = override_sec > 0 ? (time_t)override_sec : time(NULL);
    /* br_x509_minimal 的 days 自公元 1 年 1 月 1 日起算（UNIX 纪元 = 719528） */
    days = (unsigned long)(t / 86400) + 719528UL;
    secs = (unsigned long)(t % 86400);
    br_x509_minimal_set_time(xc, days, secs);
}

/* 用明文 HTTP 的 Date 响应头估算当前时间（CMOS 电池没电时救命用） */
static long long http_date_probe(const char *host, int timeoutSec, volatile LONG *cancel)
{
    char req[512];
    char buf[2048];
    char *p;
    SOCKET s;
    char err[256];
    int n, total = 0;
    struct tm tm;

    s = tcp_connect(host, 80, timeoutSec * 1000, err, sizeof(err));
    if (s == INVALID_SOCKET) return 0;
    _snprintf(req, sizeof(req),
              "HEAD / HTTP/1.0\r\nHost: %s\r\nUser-Agent: NeuroBench/%s\r\n\r\n",
              host, SM_VERSION_STR);
    if (send(s, req, (int)strlen(req), 0) == SOCKET_ERROR) { closesocket(s); return 0; }
    for (;;) {
        n = recv(s, buf + total, (int)sizeof(buf) - 1 - total, 0);
        if (n <= 0) break;
        total += n;
        buf[total] = 0;
        if (strstr(buf, "\r\n\r\n") || total >= (int)sizeof(buf) - 2) break;
    }
    closesocket(s);
    buf[total] = 0;
    if (cancel && *cancel) return 0;

    p = (char *)sm_stristr(buf, "\r\nDate:");
    if (!p) return 0;
    p += 7;
    while (*p == ' ') p++;
    if (!*p) return 0;
    return parse_http_date(p);
    return 0;
}

static int url_split(const char *url, char *host, int hostlen, int *port,
                     char *path, int pathlen, int *is_tls)
{
    const char *p;
    *is_tls = 0;
    host[0] = 0; path[0] = 0; *port = 80;
    if (!url) return 0;
    if (!_strnicmp(url, "https://", 8)) { *is_tls = 1; *port = 443; p = url + 8; }
    else if (!_strnicmp(url, "http://", 7)) { *is_tls = 0; *port = 80; p = url + 7; }
    else return 0;

    {
        const char *e = p;
        while (*e && *e != '/' && *e != ':' && *e != '?') e++;
        if ((int)(e - p) >= hostlen) return 0;
        memcpy(host, p, (size_t)(e - p));
        host[e - p] = 0;
        if (*e == ':') {
            *port = atoi(e + 1);
            while (*e && *e != '/' && *e != '?') e++;
        }
        if (*e != '/') { path[0] = '/'; path[1] = 0; }
        else {
            int n = 0;
            while (*e && n < pathlen - 1) path[n++] = *e++;
            path[n] = 0;
        }
    }
    return host[0] != 0;
}

typedef struct {
    const char *host;
    int         port;
    const char *path;
    const char *apikey;
    const char *extra;
    const char *body;
    int         bodylen;
    int         timeoutSec;
    volatile LONG *cancel;
} Req;

static void conn_close(Conn *c)
{
    if (!c) return;
    if (c->is_tls) {
        if (c->cc) {
            br_ssl_engine_close(&c->cc->eng);
            sm_free(c->cc);
        }
        if (c->xc) sm_free(c->xc);
        if (c->iobuf) sm_free(c->iobuf);
        c->cc = NULL; c->xc = NULL; c->iobuf = NULL;
    }
    if (c->s != INVALID_SOCKET) { closesocket(c->s); c->s = INVALID_SOCKET; }
}

/* 返回: 1=成功, 0=失败, -1=握手失败（可能是证书日期问题） */
static int do_request(const Req *rq, int is_tls, SmHttpResult *res,
                      long long time_override)
{
    Conn *c = NULL;
    Sb reqhdr, raw;
    char err[600];
    int rc = 0;

    err[0] = 0;
    c = (Conn *)sm_calloc(sizeof(Conn));
    c->cancel = rq->cancel;
    c->s = tcp_connect(rq->host, rq->port, rq->timeoutSec * 1000, err, sizeof(err));
    if (c->s == INVALID_SOCKET) {
        strncpy(res->errmsg, err, sizeof(res->errmsg) - 1);
        sm_free(c);
        return 0;
    }

    if (is_tls) {
        size_t num = ca_load();
        unsigned char seed[32];
        br_ssl_client_context *cc;
        br_x509_minimal_context *xc;

        if (num == 0) {
            _snprintf(res->errmsg, sizeof(res->errmsg),
                      "找不到根证书（CERTS 文件夹为空或缺失）。\r\n\r\n"
                      "请运行安装目录中的『根证书更新程序』(CERTUPD.EXE)，"
                      "或重新安装本软件。");
            conn_close(c); sm_free(c);
            return 0;
        }

        cc = (br_ssl_client_context *)sm_calloc(sizeof(br_ssl_client_context));
        xc = (br_x509_minimal_context *)sm_calloc(sizeof(br_x509_minimal_context));
        c->iobuf = (unsigned char *)sm_alloc(BR_SSL_BUFSIZE_BIDI);
        c->cc = cc; c->xc = xc; c->is_tls = 1;

        br_ssl_client_init_full(cc, xc, g_tas, num);
        br_x509_minimal_set_hash(xc, br_sha1_ID, &br_sha1_vtable);
        br_x509_minimal_set_hash(xc, br_sha256_ID, &br_sha256_vtable);
        br_x509_minimal_set_hash(xc, br_sha384_ID, &br_sha384_vtable);
        br_x509_minimal_set_rsa(xc, br_rsa_i31_pkcs1_vrfy);
        br_x509_minimal_set_ecdsa(xc, &br_ec_prime_i31, br_ecdsa_i31_vrfy_asn1);
        set_x509_time(xc, time_override);

        sm_random_bytes(seed, sizeof(seed));
        br_ssl_engine_inject_entropy(&cc->eng, seed, sizeof(seed));
        br_ssl_engine_set_buffer(&cc->eng, c->iobuf, BR_SSL_BUFSIZE_BIDI, 1);
        br_ssl_client_reset(cc, rq->host, 0);
        br_sslio_init(&c->ioc, &cc->eng, low_read, c, low_write, c);
        strcpy(res->proto, "TLS 1.2");
    } else {
        c->is_tls = 0;
        strcpy(res->proto, "HTTP 明文");
    }

    /* 构造请求 */
    sb_init(&reqhdr);
    sb_printf(&reqhdr, "POST %s HTTP/1.1\r\n", rq->path);
    sb_printf(&reqhdr, "Host: %s\r\n", rq->host);
    sb_printf(&reqhdr, "User-Agent: NeuroBench/%s (Windows; 32-bit)\r\n", SM_VERSION_STR);
    sb_puts(&reqhdr, "Accept: */*\r\n");
    sb_puts(&reqhdr, "Accept-Encoding: identity\r\n");
    sb_puts(&reqhdr, "Connection: close\r\n");
    sb_printf(&reqhdr, "Content-Type: application/json; charset=utf-8\r\n");
    sb_printf(&reqhdr, "Content-Length: %d\r\n", rq->bodylen);
    if (rq->apikey && rq->apikey[0])
        sb_printf(&reqhdr, "Authorization: Bearer %s\r\n", rq->apikey);
    if (rq->extra && rq->extra[0]) sb_puts(&reqhdr, rq->extra);
    sb_puts(&reqhdr, "\r\n");

    if (conn_write_all(c, sb_str(&reqhdr), reqhdr.len) < 0 ||
        (rq->bodylen > 0 && conn_write_all(c, rq->body, (size_t)rq->bodylen) < 0) ||
        conn_flush(c) < 0) {
        unsigned e = is_tls ? br_ssl_engine_last_error(&c->cc->eng) : 0;
        char tb[256];
        sb_free(&reqhdr);
        if (is_tls && e != 0) {
            _snprintf(res->errmsg, sizeof(res->errmsg),
                      "与服务器建立安全连接失败：\r\n\r\n%s\r\n\r\n"
                      "可能原因：服务器地址不正确、本机系统时间错误，"
                      "或网络中的防火墙拦截了 443 端口。\r\n\r\n"
                      "（NeuroBench 使用内置的 SSL/TLS 1.2 组件进行加密通信）",
                      tls_err_text(e, tb, sizeof(tb)));
            res->errmsg[sizeof(res->errmsg) - 1] = 0;
            if (e == BR_ERR_X509_EXPIRED || e == BR_ERR_X509_TIME_UNKNOWN) rc = -1;
        } else {
            strcpy(res->errmsg, "向服务器发送请求失败（连接可能已中断）。");
        }
        conn_close(c);
        sm_free(c);
        return rc;
    }
    sb_free(&reqhdr);

    /* 读响应 */
    sb_init(&raw);
    for (;;) {
        char buf[4096];
        int n;
        if (rq->cancel && *rq->cancel) {
            strcpy(res->errmsg, "操作已被用户中止。");
            sb_free(&raw);
            conn_close(c); sm_free(c);
            return 0;
        }
        n = conn_read(c, buf, sizeof(buf));
        if (n < 0) {
            unsigned e = is_tls ? br_ssl_engine_last_error(&c->cc->eng) : 0;
            char tb[256];
            if (is_tls && e != 0) {
                _snprintf(res->errmsg, sizeof(res->errmsg),
                          "接收服务器数据时出错：\r\n%s", tls_err_text(e, tb, sizeof(tb)));
                res->errmsg[sizeof(res->errmsg) - 1] = 0;
            } else if (raw.len == 0) {
                strcpy(res->errmsg, "服务器没有返回任何数据。");
            }
            sb_free(&raw);
            conn_close(c); sm_free(c);
            return 0;
        }
        if (n == 0) break;
        sb_putn(&raw, buf, (size_t)n);
        if (raw.len > 32 * 1024 * 1024) break;
    }
    conn_close(c);
    sm_free(c);

    if (raw.len == 0) {
        if (res->errmsg[0] == 0) strcpy(res->errmsg, "服务器没有返回任何数据。");
        sb_free(&raw);
        return 0;
    }

    /* 解析响应 */
    {
        char *p = sb_str(&raw);
        char *hend = strstr(p, "\r\n\r\n");
        char *body;
        int hlen;
        res->status = 0;
        if (!strncmp(p, "HTTP/", 5)) {
            char *sp = strchr(p, ' ');
            if (sp) res->status = atoi(sp + 1);
        }
        if (hend) { hlen = (int)(hend - p) + 4; body = hend + 4; }
        else { hlen = (int)raw.len; body = p + raw.len; }

        if (sm_stristr(p, "Transfer-Encoding: chunked")) {
            Sb dec;
            char *q = body;
            char *rend = sb_str(&raw) + raw.len;
            sb_init(&dec);
            for (;;) {
                long clen;
                char *nl;
                if (q >= rend) break;
                clen = strtol(q, &nl, 16);
                if (clen <= 0) break;
                if (nl[0] == '\r' && nl[1] == '\n') nl += 2;
                else if (nl[0] == '\n') nl += 1;
                if (nl + clen > rend) { sb_putn(&dec, nl, (size_t)(rend - nl)); break; }
                sb_putn(&dec, nl, (size_t)clen);
                q = nl + clen;
                if (q[0] == '\r' && q[1] == '\n') q += 2;
                else if (q[0] == '\n') q += 1;
            }
            {
                long dl = (long)dec.len;
                res->body = sb_take(&dec);
                res->bodylen = (int)dl;
            }
            if (dec.p) free(dec.p);
        } else {
            long cl = -1;
            char *cp = (char *)sm_stristr(p, "\r\nContent-Length:");
            if (cp && cp < p + hlen) cl = strtol(cp + 17, NULL, 10);
            res->bodylen = (int)(sb_str(&raw) + raw.len - body);
            if (cl >= 0 && cl < res->bodylen) res->bodylen = (int)cl;
            res->body = (char *)sm_alloc((size_t)res->bodylen + 1);
            memcpy(res->body, body, (size_t)res->bodylen);
            res->body[res->bodylen] = 0;
        }
        (void)hlen;
    }
    sb_free(&raw);
    return 1;
}

/* ------------------------------------------------------------------ */
int sm_https_post(const SmHttpArgs *a, SmHttpResult *res)
{
    Req rq;
    char host[260], path[1024];
    int port, is_tls, r;

    ZeroMemory(res, sizeof(*res));
    if (!net_init()) {
        strcpy(res->errmsg, "网络子系统初始化失败（无法加载 Winsock）。");
        return 0;
    }
    if (!url_split(a->endpoint, host, sizeof(host), &port, path, sizeof(path), &is_tls)) {
        _snprintf(res->errmsg, sizeof(res->errmsg),
                  "服务器地址格式不正确：\r\n%s\r\n\r\n"
                  "正确格式示例：https://api.example.com/v1/chat/completions",
                  a->endpoint ? a->endpoint : "(空)");
        res->errmsg[sizeof(res->errmsg) - 1] = 0;
        return 0;
    }
    ZeroMemory(&rq, sizeof(rq));
    rq.host = host; rq.port = port; rq.path = path;
    rq.apikey = a->apikey; rq.extra = a->extra_headers;
    rq.body = a->body; rq.bodylen = a->bodylen;
    rq.timeoutSec = a->timeoutSec > 0 ? a->timeoutSec : 60;
    rq.cancel = a->cancel;

    r = do_request(&rq, is_tls, res, 0);

    if (r == -1) {
        if (a->ignoreCertDate) {
            long long t = http_date_probe(host, rq.timeoutSec, a->cancel);
            if (t > 0) return do_request(&rq, is_tls, res, t) == 1;
        }
        strncat(res->errmsg,
                "\r\n\r\n提示：请检查本机“日期/时间”设置是否正确。如果 CMOS 电池已耗尽，"
                "可在『设置』中勾选“忽略证书日期错误”，程序会尝试用网络时间校正。",
                sizeof(res->errmsg) - strlen(res->errmsg) - 1);
        return 0;
    }
    return r == 1;
}

void sm_http_result_free(SmHttpResult *res)
{
    if (res->body) { sm_free(res->body); res->body = NULL; }
    res->bodylen = 0;
}

int sm_tls_test(const char *endpoint, const char *apikey, int timeoutSec,
                int ignoreCertDate, char *report, int len)
{
    SmHttpArgs a;
    SmHttpResult res;
    Sb b;
    int ok;

    ZeroMemory(&a, sizeof(a));
    a.endpoint = endpoint;
    a.apikey = apikey;
    a.body = "{\"model\":\"probe\",\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}],\"max_tokens\":1}";
    a.bodylen = (int)strlen(a.body);
    a.timeoutSec = timeoutSec;
    a.ignoreCertDate = ignoreCertDate;

    ok = sm_https_post(&a, &res);
    sb_init(&b);
    if (!ok) {
        sb_puts(&b, res.errmsg);
    } else {
        sb_printf(&b, "已成功与服务器建立安全连接。\r\n\r\n");
        sb_printf(&b, "  加密协议：%s\r\n", res.proto);
        sb_printf(&b, "  HTTP 状态：%d\r\n", res.status);
        sb_printf(&b, "  返回长度：%d 字节\r\n\r\n", res.bodylen);
        if (res.status == 401 || res.status == 403)
            sb_puts(&b, "网络与加密连接正常，但“密钥”被服务器拒绝，请检查 API 密钥。");
        else if (res.status == 404)
            sb_puts(&b, "网络与加密连接正常，但接口路径不存在，请检查服务器地址。");
        else if (res.status == 400 || res.status == 422)
            sb_puts(&b, "网络与加密连接正常，服务器返回参数错误（通常是型号名称不对）。");
        else if (res.status >= 200 && res.status < 300)
            sb_puts(&b, "网络、加密与鉴权全部正常，可以开始使用。");
        else
            sb_puts(&b, "服务器有响应，但返回了错误，请检查服务器地址与型号名称。");
    }
    _snprintf(report, (size_t)len, "%s", sb_str(&b));
    report[len - 1] = 0;
    sb_free(&b);
    sm_http_result_free(&res);
    return ok;
}
