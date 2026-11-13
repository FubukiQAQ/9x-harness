/* TLS / JSON / 授权码 自检程序（开发用，控制台） */
#include "../common/sm.h"

int main(int argc, char **argv)
{
    const char *ep = argc > 1 ? argv[1] : "https://www.baidu.com/robots.txt";
    char rep[4096];
    int i;

    printf("=== " SM_PRODUCT_NAME " %s 自检 ===
", SM_VERSION_STR);
    printf("OS: %s\n", sm_get_os_name());
    printf("CP: %u\n", sm_system_codepage());

    printf("\n--- CDK ---\n");
    {
        int ed, se, dy;
        unsigned int sn;
        char k[40];
        for (i = 0; i < 3; i++) {
            sm_cdk_generate(1 + i % 3, 1 + i * 7, i * 100, (unsigned int)(0x123456 + i * 37), k);
            {
                int ok = sm_cdk_verify(k, &ed, &se, &dy, &sn);
                printf("gen=%-26s verify=%d ed=%d seats=%d days=%d serial=0x%X\n",
                       k, ok, ed, se, dy, sn);
            }
        }
        printf("bad key verify=%d (expect 0)\n", sm_cdk_verify("AAAA-AAAA-AAAA-AAAA-AAAA", NULL, NULL, NULL, NULL));
    }

    printf("\n--- JSON ---\n");
    {
        const char *txt = "{\"id\":\"x\",\"choices\":[{\"message\":{\"role\":\"assistant\","
                          "\"content\":\"\\u4f60\\u597d\\n\\\"q\\\"\"},\"finish_reason\":\"stop\"}],"
                          "\"usage\":{\"total_tokens\":12}}";
        const char *err = NULL;
        Js *v = js_parse(txt, &err);
        if (!v) { printf("JSON FAIL: %s\n", err ? err : "?"); }
        else {
            Js *ch = js_obj_get(v, "choices");
            Js *m = js_obj_get(js_arr_get(ch, 0), "message");
            char *a = sm_utf8_to_ansi(js_str(js_obj_get(m, "content"), ""));
            printf("content(ANSI)=[%s]\n", a);
            printf("total_tokens=%d\n", (int)js_num(js_obj_get(js_obj_get(v, "usage"), "total_tokens"), -1));
            sm_free(a);
            js_free(v);
        }
    }

    printf("\n--- INI ---\n");
    {
        char tmp[MAX_PATH], buf[256];
        GetTempPathA(MAX_PATH, tmp);
        strcat(tmp, "smtest.ini");
        DeleteFileA(tmp);
        SmConfig c;
        sm_config_defaults(&c);
        strcpy(c.apikey, "sk-test-1234567890");
        strcpy(c.model, "gpt-x");
        sm_config_save(&c);
        printf("ini=%s\n", tmp);
        (void)buf;
    }

    printf("\n--- TLS -> %s ---\n", ep);
    if (sm_tls_test(ep, "sk-probe", 30, 1, rep, sizeof(rep))) printf("RESULT OK:\n%s\n", rep);
    else printf("RESULT FAIL:\n%s\n", rep);

    /* 直接调用一次，检查响应体提取 */
    {
        SmHttpArgs a;
        SmHttpResult r;
        ZeroMemory(&a, sizeof(a));
        a.endpoint = "https://www.baidu.com/robots.txt";
        a.body = "{\"x\":1}";
        a.bodylen = (int)strlen(a.body);
        a.timeoutSec = 30;
        if (sm_https_post(&a, &r)) {
            printf("\n--- BODY CHK: status=%d len=%d ---\n", r.status, r.bodylen);
            if (r.body) {
                int n = r.bodylen > 200 ? 200 : r.bodylen;
                fwrite(r.body, 1, (size_t)n, stdout);
                printf("\n");
            }
        } else printf("\n--- BODY CHK FAIL: %s\n", r.errmsg);
        sm_http_result_free(&r);
    }
    return 0;
}
