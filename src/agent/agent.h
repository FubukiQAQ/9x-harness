/* Axiom NeuroBench 2003 - 主程序内部头文件 */
#ifndef NB_AGENT_H
#define NB_AGENT_H

#include "../common/sm.h"
#include "../common/nb_lang.h"
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <shellapi.h>

/* htmlhelp 常量（mingw 没有 htmlhelp.h） */
#ifndef HH_DISPLAY_TOPIC
#define HH_DISPLAY_TOPIC 0x0000
#endif

/* 对话区文字种类 */
#define CT_SYS    0
#define CT_INFO   1
#define CT_ASSIST 2
#define CT_USER   3
#define CT_ERR    4
#define CT_THINK  5      /* 思考过程：灰字斜体 */

/* ---------------- 控件 ID ---------------- */
#define IDC_TRANSCRIPT    1001
#define IDC_INPUT         1002
#define IDC_BTN_SEND      1004
#define IDC_BTN_STOP      1005
#define IDC_TOOLBAR       1006
#define IDC_STATUSBAR     1007
#define IDC_CHK_TOOLS     1008
#define IDC_WS_SEARCH     1010
#define IDC_WS_LIST       1011
#define IDC_FILE_SEARCH   1012
#define IDC_TREE          1013
#define IDC_SPLIT1        1014
#define IDC_SPLIT2        1015
#define IDC_LBL_WS        1016
#define IDC_LBL_TREE      1017
#define IDC_LBL_MSG       1018
#define IDC_LBL_INPUT     1019
#define IDC_LBL_WSSEARCH  1020
#define IDC_LBL_FILESEARCH 1021

/* ---------------- 命令 ID ---------------- */
#define IDM_FILE_NEW        101
#define IDM_FILE_OPEN       102
#define IDM_FILE_SAVEAS     103
#define IDM_FILE_PRINT      104
#define IDM_FILE_EXIT       105
#define IDM_FILE_EDITOR     106
#define IDM_FILE_SAVE       107
#define IDM_FILE_OPENCONV   108

#define IDM_EDIT_UNDO       201
#define IDM_EDIT_CUT        202
#define IDM_EDIT_COPY       203
#define IDM_EDIT_PASTE      204
#define IDM_EDIT_SELALL     205
#define IDM_EDIT_FIND       206
#define IDM_EDIT_TIME       207
#define IDM_EDIT_WORDWRAP   208
#define IDM_EDIT_FONT       209

#define IDM_CHAT_SEND       301
#define IDM_CHAT_STOP       302
#define IDM_CHAT_CLEAR      303
#define IDM_CHAT_COPYALL    304
#define IDM_CHAT_SYSINFO    305
#define IDM_CHAT_ATTACH     310
#define IDM_CHAT_REFLOW     311   /* 自动合并模型硬折行 */
#define IDM_CHAT_THINK      306
#define IDM_CHAT_THINK_LOW  307
#define IDM_CHAT_THINK_MID  308
#define IDM_CHAT_THINK_HIGH 309

#define IDM_TOOL_SETTINGS   401
#define IDM_TOOL_ACTIVATE   402
#define IDM_TOOL_CERTUPD    403
#define IDM_TOOL_ABOUTSMATE 404
#define IDM_TOOL_LANG_ZH    405
#define IDM_TOOL_LANG_EN    406

#define IDM_WS_NEW          601
#define IDM_WS_RENAME       602
#define IDM_WS_DELETE       603
#define IDM_WS_OPENFOLDER   604
#define IDM_WS_REFRESH      605
#define IDM_WS_NEWFILE      606
#define IDM_WS_ADDFILE      607

#define IDM_HELP_TOPICS     501
#define IDM_HELP_QUICK      502
#define IDM_HELP_README     503
#define IDM_HELP_ABOUT      504

/* ---------------- 事件（工作线程 -> 界面线程） ---------------- */
#define WM_SM_EVENT (WM_APP + 10)

#define EVT_USER        1
#define EVT_ASSISTANT   2
#define EVT_NOTICE      3
#define EVT_ERROR       4
#define EVT_DONE        5
#define EVT_STATE       6
#define EVT_REASONING   7

typedef struct {
    int   kind;
    char *text;
    int   flag;
} SmEvent;

/* ---------------- 全局状态 ---------------- */
typedef struct {
    HINSTANCE  hInst;
    HWND       hFrame;
    HWND       hToolbar;
    HWND       hStatus;

    /* 右窗格：对话 + 输入 */
    HWND       hTranscript;
    HWND       hInput;
    HWND       hBtnSend;
    HWND       hBtnStop;
    HWND       hChkTools;
    HWND       hLblMsg;
    HWND       hLblInput;

    /* 左窗格：工作区 */
    HWND       hLblWsSearch;
    HWND       hWsSearch;
    HWND       hWsList;
    HWND       hLblWs;

    /* 中窗格：文件树 */
    HWND       hLblFileSearch;
    HWND       hFileSearch;
    HWND       hTree;
    HWND       hLblTree;

    /* 分隔条 */
    HWND       hSplit1;
    HWND       hSplit2;

    HFONT      hFontUI;
    HFONT      hFontDlg;      /* 对话框用，比主界面略大 */
    HFONT      hFontBold;
    HFONT      hFontMono;
    HFONT      hFontTitle;
    HFONT      hFontSmall;
    HBRUSH     hbrBk;

    SmConfig   cfg;
    SmProviders prov;      /* 多供应商表 */
    /* 下一次请求要附带的截图（PNG） */
    SmLicense  lic;

    volatile LONG cancel;
    volatile LONG busy;

    Sb         history;
    int        turnCount;
    int        dirty;

    int        paneL;        /* 左窗格宽度 */
    int        paneM;        /* 中窗格宽度 */
    int        inputH;       /* 输入框高度 */
    UINT       findMsg;
    char       lastFile[MAX_PATH];
} App;

extern App g;

/* ---------------- chat.c ---------------- */
void  chat_init(HWND hwndFrame);
void  chat_append(int kind, const char *text);
void  chat_clear(void);
char *chat_get_input(void);
void  chat_set_input(const char *s);
void  chat_set_raw(const char *text);

/* 待发送的屏幕截图（PNG），由 tools.c 的 screenshot 工具填充 */
extern unsigned char *g_shot_png;
extern int            g_shot_len;
extern int            g_shot_w, g_shot_h;
void  chat_zoom(int delta);          /* Ctrl+滚轮：调整聊天字号 */
int   chat_font_px(void);
void  chat_focus_input(void);
char *chat_transcript_text(void);
void  chat_history_reset(void);
void  chat_history_append_user(const char *ansi);
void  chat_history_append_json(const char *json);
void  chat_history_trim(void);
void  chat_find(HWND owner);
int   chat_is_find_msg(MSG *msg);

/* ---------------- llm.c ---------------- */
void  llm_start(const char *userText);
void  llm_cancel(void);
int   llm_is_busy(void);
char *llm_build_request(int *outLen);
void  llm_post_event(int kind, const char *text, int flag);

/* ---------------- tools.c ---------------- */
char *tools_spec_json(void);
char *tools_execute(const char *name, const char *argsJson, int *isError);
int   tools_confirm(const char *what);
void  tools_sysinfo_text(Sb *out);

/* ---------------- prompt.c ---------------- */
char *prompt_system(void);
int  dlg_confirm(HWND owner, const char *text);   /* 工具执行确认，1=允许 */
void tools_confirm_show(void);   /* 主线程收到确认请求时调用 */
#define WM_NB_CONFIRM (WM_APP + 71)
char *prompt_default_template(void);   /* 默认系统提示词模板（含占位符） */

/* ---------------- dlgs.c ---------------- */
int   dlg_pick_image(char *outPath, int outLen);
void  dlg_settings(HWND owner);
void  dlg_activate(HWND owner);
void  dlg_about(HWND owner);
void  dlg_sysinfo(HWND owner);
void  dlg_connecting(HWND owner);
void  dlg_connecting_close(void);
void  dlg_connecting_set(const char *text);
int   dlg_input(HWND owner, const char *prompt, const char *title,
                char *buf, int buflen, int selectAll);

/* ---------------- editor.c ---------------- */
void  editor_open(const char *path);
HWND  editor_last_window(void);
void  editor_refresh_strings(void);

/* ---------------- workspace.c ---------------- */
char *ws_root(void);
char *ws_current_dir(void);
const char *ws_current(void);
void  ws_init(void);
void  ws_populate_list(void);
void  ws_set_filter(const char *text);
void  ws_set_file_filter(const char *text);
void  ws_switch_to(const char *name);
int   ws_create(const char *name);
int   ws_rename(const char *oldn, const char *newn);
int   ws_delete(const char *name);
void  ws_tree_refresh(void);
char *tree_selected_path(void);
void  ws_open_path(const char *path);
int   path_ext_supported(const char *path);
int   path_is_conv(const char *path);
void  conv_new(void);
int   conv_save(void);
int   conv_save_as(const char *path);
void  conv_load(const char *path);
void  conv_append_text(const char *text);
int   conv_is_open(void);
const char *conv_path(void);
int   conv_dirty(void);
void  conv_mark_dirty(void);
void  conv_clear_dirty(void);
void  conv_set_path(const char *p);

/* ---------------- 公共 UI 小工具 ---------------- */
HFONT sm_ui_font(int px, int bold, int mono);
void  sm_set_dlg_font(HWND hdlg);
void  sm_open_help(HWND owner, const char *topic);
void  sm_open_help_file(HWND owner);
void  sm_error(HWND owner, const char *fmt, ...);
void  sm_info(HWND owner, const char *fmt, ...);
int   sm_ask(HWND owner, const char *fmt, ...);
void  sm_splash_show(void);
void  sm_splash_hide(void);
void  sm_status_set(int pane, const char *text);
void  sm_update_title(void);
void  sm_update_license_ui(void);
void  sm_rebuild_ui(void);      /* 切换语言后重建菜单与控件文字 */

#endif
