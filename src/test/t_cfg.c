/* 配置读取自检（开发用） */
#include "../common/sm.h"

int main(int argc, char **argv)
{
    SmConfig c;
    const char *ini = argc > 1 ? argv[1] : "NEBENCH.INI";
    char *v;
    int i;

    printf("ini=%s exists=%d\n", ini, sm_file_exists(ini));
    v = sm_ini_get(ini, "connection", "endpoint", "(def)");
    printf("raw endpoint = [%s] len=%d bytes:", v, (int)strlen(v));
    for (i = 0; v[i]; i++) printf(" %02X", (unsigned char)v[i]);
    printf("\n");
    sm_free(v);

    sm_config_load(&c);
    printf("cfg.endpoint = [%s] len=%d\n", c.endpoint, (int)strlen(c.endpoint));
    printf("cfg.model    = [%s]\n", c.model);
    printf("cfg.apikey   = [%s]\n", c.apikey);
    printf("cfg.timeout  = %d  maxtokens=%d  useTools=%d\n",
           c.timeoutSec, c.maxtokens, c.useTools);
    return 0;
}
