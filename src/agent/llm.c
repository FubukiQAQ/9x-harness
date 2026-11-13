/* NeuroBench 2003 - 与云端大模型通信（含工具调用循环）
 *
 * 工作线程做全部网络和解析工作，界面线程只负责显示，
 * 事件通过 PostMessage(WM_SM_EVENT) 回传。 */
#include "agent.h"

#define MAX_TOOL_ROUNDS_DEFAULT 8

/* ------------------------------------------------------------------ */
void llm_post_event(int kind, const char *text, int flag)
{
    SmEvent *e = (SmEvent *)sm_calloc(sizeof(SmEvent));
    e->kind = kind;
    e->text = text ? sm_strdup(text) : NULL;
    e->flag = flag;
    PostMessageA(g.hFrame, WM_SM_EVENT, 0, (LPARAM)e);
}

static void post_notice(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = 0;
    llm_post_event(EVT_NOTICE, buf, 0);
}

/* ------------------------------------------------------------------ */
static char *build_request(void)
{
    Sb b;
    char *sys;

    sys = prompt_system();          /* ANSI */
    sb_init(&b);
    sb_puts(&b, "{\"model\":");
    sb_put_json_str(&b, g.cfg.model[0] ? g.cfg.model : "gpt-3.5-turbo");
    sb_puts(&b, ",\"messages\":[{\"role\":\"system\",\"content\":");
    sb_put_json_str(&b, sys);
    sb_putc(&b, '}');
    if (g.history.len) {
        sb_putc(&b, ',');
        sb_putn(&b, sb_str(&g.history), g.history.len);
    }
    /* 读图：screenshot 工具抓到画面后，随下一次请求附一张图。
     * 只有当前供应商开了 vision 才发；纯文本模型收到数组会报错。 */
    {
        int vision = 0;
        if (g.prov.count > 0 && g.prov.active >= 0 && g.prov.active < g.prov.count)
            vision = g.prov.list[g.prov.active].vision;
        if (vision && g_shot_png && g_shot_len > 0) {
            char *b64 = (char *)sm_alloc((size_t)g_shot_len * 2 + 32);
            if (b64) {
                sm_b64_encode(g_shot_png, g_shot_len, b64, 0);
                sb_putc(&b, ',');
                sb_puts(&b, "{\"role\":\"user\",\"content\":["
                            "{\"type\":\"text\",\"text\":\"[\u5c4f\u5e55\u622a\u56fe]\"},"
                            "{\"type\":\"image_url\",\"image_url\":{"
                            "\"url\":\"data:image/png;base64,");
                sb_puts(&b, b64);
                sb_puts(&b, "\"}}]}");
                sm_free(b64);
            }
        }
    }
    sb_puts(&b, "],\"stream\":false");
    sb_printf(&b, ",\"temperature\":%d.%d", g.cfg.temperature10 / 10,
              g.cfg.temperature10 % 10);
    sb_printf(&b, ",\"max_tokens\":%d", g.cfg.maxtokens > 0 ? g.cfg.maxtokens : 1024);
    /* 深度思考开关与挡位。
     * 不同网关用的字段名不一样，默认两种都发（多数字段不认识就忽略）：
     *   reasoning_effort : low / medium / high   （OpenAI 风格）
     *   enable_thinking  : true / false          （Qwen、混元等国内接口）
     * 用 thinkstyle 可以只发其中一种。 */
    {
        const char *lv = "medium";
        if (g.cfg.thinkLevel <= 0) lv = "low";
        else if (g.cfg.thinkLevel >= 2) lv = "high";
        if (g.cfg.think) {
            if (g.cfg.thinkStyle != 2)
                sb_printf(&b, ",\"reasoning_effort\":\"%s\"", lv);
            if (g.cfg.thinkStyle != 1)
                sb_puts(&b, ",\"enable_thinking\":true");
        } else {
            if (g.cfg.thinkStyle != 1)
                sb_puts(&b, ",\"enable_thinking\":false");
        }
    }
    if (g.cfg.useTools) {
        char *spec = tools_spec_json();   /* UTF-8 */
        sb_puts(&b, ",\"tools\":");
        sb_puts(&b, spec);
        sb_puts(&b, ",\"tool_choice\":\"auto\"");
        sm_free(spec);
    }
    sb_putc(&b, '}');
    sm_free(sys);
    /* 截图是一次性的：发出去就释放，避免下次重复附带旧画面 */
    if (g_shot_png) { sm_free(g_shot_png); g_shot_png = NULL; g_shot_len = 0; }
    return sb_take(&b);
}

char *llm_build_request(int *outLen)
{
    char *r = build_request();
    if (outLen) *outLen = (int)strlen(r);
    return r;
}

/* 把解析出的 assistant message 重新序列化成发给下一轮的历史节点 */
static void serialize_assistant_msg(Sb *out, Js *msg)
{
    Js *content = js_obj_get(msg, "content");
    Js *tc = js_obj_get(msg, "tool_calls");
    int n, i;

    sb_puts(out, "{\"role\":\"assistant\",\"content\":");
    if (content && content->t == JS_STR && content->str) {
        sb_put_json_raw(out, content->str);
    } else {
        sb_puts(out, "null");
    }
    if (tc && tc->t == JS_ARR) {
        n = js_arr_count(tc);
        if (n > 0) {
            sb_puts(out, ",\"tool_calls\":[");
            for (i = 0; i < n; i++) {
                Js *c = js_arr_get(tc, i);
                Js *fn = js_obj_get(c, "function");
                const char *id = js_str(js_obj_get(c, "id"), "");
                const char *nm = js_str(js_obj_get(fn, "name"), "");
                const char *ar = js_str(js_obj_get(fn, "arguments"), "");
                if (i) sb_putc(out, ',');
                sb_puts(out, "{\"id\":");
                sb_put_json_raw(out, id);
                sb_puts(out, ",\"type\":\"function\",\"function\":{\"name\":");
                sb_put_json_raw(out, nm);
                sb_puts(out, ",\"arguments\":");
                sb_put_json_raw(out, ar);
                sb_puts(out, "}}");
            }
            sb_puts(out, "]");
        }
    }
    sb_putc(out, '}');
}

/* ------------------------------------------------------------------ */
#define MAX_ARG_LOG 120

static DWORD WINAPI llm_thread(LPVOID param)
{
    char *userText = (char *)param;
    int round = 0;
    int maxRounds = g.cfg.toolRounds;
    chat_history_append_user(userText);
    sm_free(userText);

    if (maxRounds < 1) maxRounds = 1;
    if (maxRounds > 50) maxRounds = 50;
    for (round = 0; round < maxRounds; round++) {
        char *body;
        SmHttpArgs a;
        SmHttpResult res;
        Js *root = NULL;
        Js *choices, *msg, *tc;
        int ok;
        if (g.cancel) { llm_post_event(EVT_NOTICE, L(STR_MSG_STOPPED), 0); break; }

        body = build_request();
        ZeroMemory(&a, sizeof(a));
        a.endpoint = g.cfg.endpoint;
        a.apikey = g.cfg.apikey;
        a.body = body;
        a.bodylen = (int)strlen(body);
        a.timeoutSec = g.cfg.timeoutSec > 0 ? g.cfg.timeoutSec : 60;
        a.ignoreCertDate = g.cfg.ignoreCertDate;
        a.cancel = &g.cancel;

        llm_post_event(EVT_STATE, L(STR_STATUS_CONNECTING), 0);
        ok = sm_https_post(&a, &res);
        sm_free(body);

        if (!ok) {
            char m[1200];
            _snprintf(m, sizeof(m), "%s", res.errmsg[0] ? res.errmsg : "网络请求失败。");
            m[sizeof(m) - 1] = 0;
            llm_post_event(EVT_ERROR, m, 0);
            sm_http_result_free(&res);
            break;
        }

        llm_post_event(EVT_STATE, L(STR_STATUS_RECEIVING), 0);

        if (res.status != 200) {
            char m[1200];
            const char *detail = NULL;
            Js *r2 = js_parse(res.body, NULL);
            if (r2) {
                Js *e = js_obj_get(r2, "error");
                if (e) detail = js_str(js_obj_get(e, "message"), NULL);
                if (!detail) detail = js_str(js_obj_get(r2, "message"), NULL);
            }
            if (res.status == 401 || res.status == 403)
                _snprintf(m, sizeof(m), "服务器拒绝了请求（HTTP %d）：API 密钥无效或没有权限。\r\n%s",
                          res.status, detail ? detail : "");
            else if (res.status == 404)
                _snprintf(m, sizeof(m), "服务器返回 HTTP 404：接口地址不存在。\r\n"
                                        "请在『工具 - 设置』中检查服务器地址。");
            else if (res.status == 429)
                _snprintf(m, sizeof(m), "服务器返回 HTTP 429：请求太频繁或额度已用完。\r\n%s",
                          detail ? detail : "");
            else
                _snprintf(m, sizeof(m), "服务器返回 HTTP %d。\r\n%s",
                          res.status, detail ? detail : (res.body ? res.body : ""));
            m[sizeof(m) - 1] = 0;
            if (strlen(m) > 1200) m[1100] = 0;
            llm_post_event(EVT_ERROR, m, 0);
            if (r2) js_free(r2);
            sm_http_result_free(&res);
            break;
        }

        root = js_parse(res.body, NULL);
        sm_http_result_free(&res);
        if (!root) {
            llm_post_event(EVT_ERROR,
                "The server response could not be parsed as JSON.\r\n"
                "Please make sure the endpoint is an OpenAI-compatible chat/completions API.", 0);
            break;
        }

        choices = js_obj_get(root, "choices");
        msg = js_obj_get(js_arr_get(choices, 0), "message");
        if (!msg) {
            Js *e = js_obj_get(root, "error");
            const char *d = e ? js_str(js_obj_get(e, "message"), NULL) : NULL;
            char m[900];
            _snprintf(m, sizeof(m), "服务器没有返回有效的回复。\r\n%s", d ? d : "");
            m[sizeof(m) - 1] = 0;
            llm_post_event(EVT_ERROR, m, 0);
            js_free(root);
            break;
        }

        /* 记录历史 */
        {
            Sb aj;
            sb_init(&aj);
            serialize_assistant_msg(&aj, msg);
            chat_history_append_json(sb_str(&aj));
            sb_free(&aj);
        }

        /* 思考过程（DeepSeek-R1 / 混元等把思维链放在 reasoning_content） */
        {
            const char *rc = js_str(js_obj_get(msg, "reasoning_content"), NULL);
            if (!rc || !rc[0]) rc = js_str(js_obj_get(msg, "reasoning"), NULL);
            if (rc && rc[0]) {
                char *ansi = sm_utf8_to_ansi(rc);
                if (ansi && ansi[0]) {
                    /* 老机器内存小，太长就截断，只作参考 */
                    if (strlen(ansi) > 4000) {
                        ansi[4000] = 0;
                        strcat(ansi, "…");
                    }
                    llm_post_event(EVT_REASONING, ansi, 0);
                }
                sm_free(ansi);
            }
        }

        /* 显示正文 */
        {
            const char *c = js_str(js_obj_get(msg, "content"), NULL);
            if (c && c[0]) {
                char *ansi = sm_utf8_to_ansi(c);
                if (ansi && ansi[0]) llm_post_event(EVT_ASSISTANT, ansi, 0);
                sm_free(ansi);
            }
        }

        tc = js_obj_get(msg, "tool_calls");
        if (!tc || js_arr_count(tc) == 0) { js_free(root); break; }

        /* 执行工具 */
        {
            int n = js_arr_count(tc), i;
            for (i = 0; i < n; i++) {
                Js *c = js_arr_get(tc, i);
                Js *fn = js_obj_get(c, "function");
                const char *id = js_str(js_obj_get(c, "id"), "");
                const char *nm = js_str(js_obj_get(fn, "name"), "");
                const char *ar = js_str(js_obj_get(fn, "arguments"), "{}");
                char *ansiName, *ansiArgs, *result;
                char *argPreview;
                int isErr = 0;

                if (g.cancel) { js_free(root); goto done; }

                ansiName = sm_utf8_to_ansi(nm);
                ansiArgs = sm_utf8_to_ansi(ar);
                argPreview = sm_strdup(ansiArgs);
                if (strlen(argPreview) > MAX_ARG_LOG) {
                    argPreview[MAX_ARG_LOG] = 0;
                    strcat(argPreview, "...");
                }
                {
                    char fmt[200];
                    char line[700];
                    strncpy(fmt, L(STR_MSG_TOOLCALL), sizeof(fmt) - 1);
                    fmt[sizeof(fmt) - 1] = 0;
                    _snprintf(line, sizeof(line), fmt, ansiName, argPreview);
                    line[sizeof(line) - 1] = 0;
                    post_notice("  -> %s", line);
                }
                sm_free(argPreview);

                llm_post_event(EVT_STATE, L(STR_STATUS_TOOL), 0);
                result = tools_execute(ansiName, ar, &isErr);

                /* 工具结果写入历史（ANSI -> UTF-8 转义） */
                {
                    Sb tm;
                    sb_init(&tm);
                    sb_puts(&tm, "{\"role\":\"tool\",\"tool_call_id\":");
                    sb_put_json_raw(&tm, id);
                    sb_puts(&tm, ",\"content\":");
                    sb_put_json_str(&tm, result ? result : "");
                    sb_putc(&tm, '}');
                    chat_history_append_json(sb_str(&tm));
                    sb_free(&tm);
                }

                /* 界面上显示结果摘要（前几行） */
                if (result && result[0]) {
                    char *snip = sm_strdup(result);
                    int lines = 0;
                    char *p;
                    for (p = snip; *p; p++) {
                        if (*p == '\r' || *p == '\n') {
                            if (++lines >= 3) { *p = 0; break; }
                        }
                    }
                    if (strlen(snip) > 400) snip[400] = 0;
                    post_notice("     <- %s%s%s", isErr ? "[failed] " : "",
                                snip, strlen(result) > strlen(snip) ? " ..." : "");
                    sm_free(snip);
                }
                sm_free(result);
                sm_free(ansiName);
                sm_free(ansiArgs);
            }
        }
        js_free(root);

        if (round == maxRounds - 1)
            llm_post_event(EVT_ERROR, L(STR_MSG_MAXROUND), 0);
    }

done:
    chat_history_trim();
    llm_post_event(EVT_DONE, NULL, 0);
    return 0;
}

/* ------------------------------------------------------------------ */
void llm_start(const char *userText)
{
    HANDLE th;
    DWORD tid;
    if (g.busy) return;
    InterlockedExchange(&g.cancel, 0);
    InterlockedExchange(&g.busy, 1);
    SendMessageA(g.hToolbar, TB_ENABLEBUTTON, IDM_CHAT_SEND, MAKELONG(0, 0));
    SendMessageA(g.hToolbar, TB_ENABLEBUTTON, IDM_CHAT_STOP, MAKELONG(1, 0));
    EnableWindow(g.hBtnSend, FALSE);
    EnableWindow(g.hBtnStop, TRUE);
    sm_status_set(0, "正在连接服务器...");
    dlg_connecting(g.hFrame);

    th = CreateThread(NULL, 0, llm_thread, sm_strdup(userText), 0, &tid);
    if (th) CloseHandle(th);
}

void llm_cancel(void)
{
    InterlockedExchange(&g.cancel, 1);
    dlg_connecting_set("正在停止...");
}

int llm_is_busy(void)
{
    return (int)g.busy;
}
