/* Axiom NeuroBench 2003 - 界面字符串表（简体中文 / English） */
#include "nb_lang.h"

static int g_lang = LANG_ZH;

/* { 中文, English } —— 顺序与 lang.h 中的 StrId 完全一致 */
static const char *T[S__COUNT][2] = {
/* STR_OK */            { "确定", "OK" },
/* STR_CANCEL */        { "取消", "Cancel" },
/* STR_CLOSE */         { "关闭", "Close" },
/* STR_YES */           { "是", "Yes" },
/* STR_NO */            { "否", "No" },
/* STR_BROWSE */        { "浏览(&B)...", "Browse(&B)..." },
/* STR_REFRESH */       { "刷新(&R)", "Refresh(&R)" },
/* STR_APPLY */         { "应用(&A)", "Apply(&A)" },
/* STR_SAVE */          { "保存(&S)", "Save(&S)" },
/* STR_OPEN */          { "打开(&O)", "Open(&O)" },
/* STR_DELETE */        { "删除(&D)", "Delete(&D)" },
/* STR_RENAME */        { "重命名(&M)", "Rename(&M)" },
/* STR_NEW */           { "新建(&N)", "New(&N)" },
/* STR_ERROR */         { "错误", "Error" },
/* STR_WARNING */       { "注意", "Warning" },
/* STR_NOTICE */        { "提示", "Notice" },
/* STR_UNTITLED */      { "无标题", "Untitled" },
/* STR_NONE */          { "(无)", "(none)" },
/* STR_ALLFILES */      { "所有文件 (*.*)", "All files (*.*)" },
/* STR_TEXTFILES */     { "文本文件 (*.txt)", "Text files (*.txt)" },

/* STR_APP_TITLE */     { SM_PRODUCT_NAME, SM_PRODUCT_NAME_EN },
/* STR_STATUS_READY */  { "就绪", "Ready" },
/* STR_STATUS_CONNECTING */ { "正在连接服务器...", "Connecting to server..." },
/* STR_STATUS_RECEIVING */  { "正在接收应答...", "Receiving reply..." },
/* STR_STATUS_TOOL */   { "正在执行工具...", "Running tool..." },
/* STR_STATUS_STOPPED */{ "已停止", "Stopped" },
/* STR_STATUS_DONE */   { "完成", "Done" },
/* STR_STATUS_SAVED */  { "已保存", "Saved" },
/* STR_STATUS_NOCONV */ { "未打开会话", "No conversation" },
/* STR_STATUS_TRIAL */  { "试用版", "Trial" },
/* STR_STATUS_LICENSED */ { "已授权", "Licensed" },
/* STR_STATUS_EXPIRED */{ "试用到期", "Trial expired" },
/* STR_STATUS_WORKSPACE */ { "工作区", "Workspace" },
/* STR_STATUS_MODEL */  { "型号", "Model" },
/* STR_STATUS_LANG */   { "语言", "Language" },

/* STR_M_FILE */        { "文件(&F)", "&File" },
/* STR_M_FILE_NEWCONV */ { "新建会话(&N)\tCtrl+N", "&New Conversation\tCtrl+N" },
/* STR_M_FILE_OPENCONV */{ "打开会话(&O)...\tCtrl+O", "&Open Conversation...\tCtrl+O" },
/* STR_M_FILE_SAVE */   { "保存会话(&S)\tCtrl+S", "&Save Conversation\tCtrl+S" },
/* STR_M_FILE_SAVEAS */ { "会话另存为(&A)...", "Save Conversation &As..." },
/* STR_M_FILE_EXPORT */ { "导出为纯文本(&E)...", "&Export as Text..." },
/* STR_M_FILE_EDITOR */ { "文本编辑器(&T)", "Text Edi&tor" },
/* STR_M_FILE_EXIT */   { "退出(&X)", "E&xit" },

/* STR_M_EDIT */        { "编辑(&E)", "&Edit" },
/* STR_M_EDIT_UNDO */   { "撤消(&U)\tCtrl+Z", "&Undo\tCtrl+Z" },
/* STR_M_EDIT_CUT */    { "剪切(&T)\tCtrl+X", "Cu&t\tCtrl+X" },
/* STR_M_EDIT_COPY */   { "复制(&C)\tCtrl+C", "&Copy\tCtrl+C" },
/* STR_M_EDIT_PASTE */  { "粘贴(&P)\tCtrl+V", "&Paste\tCtrl+V" },
/* STR_M_EDIT_SELALL */ { "全选(&A)\tCtrl+A", "Select &All\tCtrl+A" },
/* STR_M_EDIT_FIND */   { "查找(&F)...\tCtrl+F", "&Find...\tCtrl+F" },
/* STR_M_EDIT_TIME */   { "插入日期时间(&D)", "Insert Date/&Time" },

/* STR_M_WS */          { "工作区(&W)", "&Workspace" },
/* STR_M_WS_NEW */      { "新建工作区(&N)...", "&New Workspace..." },
/* STR_M_WS_RENAME */   { "重命名工作区(&M)...", "Re&name Workspace..." },
/* STR_M_WS_DELETE */   { "删除工作区(&D)", "&Delete Workspace" },
/* STR_M_WS_OPENFOLDER */ { "打开工作区文件夹(&O)", "&Open Workspace Folder" },
/* STR_M_WS_REFRESH */  { "刷新(&R)\tF5", "&Refresh\tF5" },
/* STR_M_WS_NEWFILE */  { "在工作区中新建文本文件(&F)...", "New Text &File in Workspace..." },
/* STR_M_WS_IMPORT */   { "把文件加入工作区(&A)...", "&Add File to Workspace..." },

/* STR_M_CONV */        { "会话(&C)", "&Conversation" },
/* STR_M_CONV_SEND */   { "发送(&S)\tCtrl+Enter", "&Send\tCtrl+Enter" },
/* STR_M_CONV_STOP */   { "停止(&E)\tEsc", "Stop R&equest\tEsc" },
/* STR_M_CONV_CLEAR */  { "清空当前会话(&C)", "&Clear Conversation" },
/* STR_M_CONV_COPYALL */{ "复制整个对话(&Y)", "Cop&y Whole Transcript" },
/* STR_M_CONV_SYSINFO */{ "采集系统信息并填入(&I)", "&Insert System Information" },
/* STR_M_CONV_ATTACH */   { "附加图片(&P)...", "Attach &Picture..." },
/* STR_M_CONV_THINK */    { "深度思考(&T)", "Deep &Thinking" },
/* STR_M_CONV_THINKLEVEL */ { "思考挡位(&L)", "Thinking &Level" },
/* STR_THINK_LOW */       { "低（快）", "Low (fast)" },
/* STR_THINK_MID */       { "中（默认）", "Medium (default)" },
/* STR_THINK_HIGH */      { "高（慢）", "High (slow)" },
/* STR_MSG_THINK_ON */    { "已开启深度思考：回答前会先推演一遍，速度会变慢。", "Deep thinking enabled. The assistant reasons before answering; responses are slower." },
/* STR_MSG_THINK_OFF */   { "已关闭深度思考，直接作答。", "Deep thinking disabled; the assistant answers directly." },
/* STR_THINK_PREFIX */    { "思考", "Thinking" },
/* STR_MSG_ATTACHED */    { "已附加图片，下一条消息会一起发送。", "Picture attached; it will be sent with your next message." },
/* STR_MSG_ATTACHED_NOIMG */ { "图片已附加，但当前模型没有开“读图”，对方看不到。请在设置里给这个模型勾上“视觉”。", "A picture is attached, but this model does not have vision enabled, so the server will ignore it." },
/* STR_ATTACH_FILTER */   { "图片文件 (*.png;*.bmp)\0*.png;*.bmp\0所有文件 (*.*)\0*.*\0", "Pictures (*.png;*.bmp)\0*.png;*.bmp\0All files (*.*)\0*.*\0" },

/* STR_M_TOOL */        { "工具(&T)", "&Tools" },
/* STR_M_TOOL_SETTINGS */ { "设置(&O)...", "&Options..." },
/* STR_M_TOOL_ACTIVATE */ { "激活产品(&A)...", "&Activate Product..." },
/* STR_M_TOOL_CERTUPD */  { "根证书更新(&C)...", "&Root Certificate Update..." },
/* STR_M_TOOL_SYSINFO */  { "系统信息(&I)...", "System &Information..." },
/* STR_M_TOOL_LANG */     { "语言(&L)", "&Language" },
/* STR_M_TOOL_LANG_ZH */  { "简体中文(&C)", "Chinese (Simplified)(&C)" },
/* STR_M_TOOL_LANG_EN */  { "English(&E)", "English(&E)" },

/* STR_M_HELP */        { "帮助(&H)", "&Help" },
/* STR_M_HELP_TOPICS */ { "帮助目录(&H)\tF1", "Help &Topics\tF1" },
/* STR_M_HELP_QUICK */  { "快速入门(&Q)", "&Quick Start" },
/* STR_M_HELP_README */ { "自述文件(&R)", "&Read Me" },
/* STR_M_HELP_ABOUT */  { "关于 " SM_SHORT_NAME "(&A)...", "&About " SM_SHORT_NAME_EN "..." },

/* STR_TIP_NEWCONV */   { "新建会话 (Ctrl+N)", "New conversation (Ctrl+N)" },
/* STR_TIP_SAVE */      { "保存会话 (Ctrl+S)", "Save conversation (Ctrl+S)" },
/* STR_TIP_EDITOR */    { "打开内置文本编辑器", "Open the built-in text editor" },
/* STR_TIP_SEND */      { "发送 (Ctrl+Enter)", "Send (Ctrl+Enter)" },
/* STR_TIP_STOP */      { "停止当前请求 (Esc)", "Stop the current request (Esc)" },
/* STR_TIP_WS */        { "新建工作区", "Create a new workspace" },
/* STR_TIP_SETTINGS */  { "设置服务器与选项", "Server and option settings" },
/* STR_TIP_ACTIVATE */  { "输入产品序列号激活", "Enter a product key to activate" },
/* STR_TIP_HELP */      { "打开帮助 (F1)", "Open help (F1)" },

/* STR_PANE_WS_SEARCH */ { "工作区搜索", "Workspace search" },
/* STR_PANE_WS_LIST */   { "工作区列表", "Workspaces" },
/* STR_PANE_FILE_SEARCH */ { "文件搜索", "File search" },
/* STR_PANE_TREE */      { "当前工作区内的文件", "Files in workspace" },
/* STR_PANE_MSG */       { "对话消息", "Conversation" },
/* STR_PANE_INPUT */     { "输入", "Input" },
/* STR_PANE_OTHER */     { "其他信息", "Status" },
/* STR_PANE_NOWORKSPACE */ { "(没有工作区)", "(no workspace)" },

/* STR_SPEAKER_YOU */   { "你", "You" },
/* STR_SPEAKER_BOT */   { SM_SHORT_NAME, SM_SHORT_NAME_EN },
/* STR_SPEAKER_NOTICE */{ "提示", "Notice" },
/* STR_MSG_WELCOME */   { "欢迎使用 " SM_PRODUCT_NAME "。", "Welcome to " SM_PRODUCT_NAME_EN "." },
/* STR_MSG_NOKEY */     { "尚未配置服务器与 API 密钥。请使用『工具 - 设置』进行配置，配置完成后点击『发送』开始对话。",
                        "No server or API key configured yet. Use Tools - Options to set them up, then press Send." },
/* STR_MSG_TRIAL */     { "当前为试用版，请通过『工具 - 激活产品』输入产品序列号完成激活。",
                        "This is the trial edition. Use Tools - Activate Product to enter your product key." },
/* STR_MSG_EXPIRED */   { "试用期已结束。请使用『工具 - 激活产品』输入产品序列号后继续使用。",
                        "The trial period has ended. Use Tools - Activate Product to continue." },
/* STR_MSG_LOADED_CONV */ { "已载入会话文件：%s\r\n（此前的对话上下文不会被带入，请重新说明需求。）",
                          "Loaded conversation file: %s\r\n(Earlier context is not restored; please restate your request.)" },
/* STR_MSG_NEW_CONV */  { "已开始新的会话，保存位置：%s", "New conversation started. It will be saved to: %s" },
/* STR_MSG_TOOLCALL */  { "调用工具 %s(%s)", "Calling tool %s(%s)" },
/* STR_MSG_TOOLRESULT */{ "结果：%s", "Result: %s" },
/* STR_MSG_STOPPED */   { "（已停止）", "(stopped)" },
/* STR_MSG_MAXROUND */  { "本轮工具调用次数已达到上限（8 次），已停止。如果任务还没完成，请把需求拆小一点再试。",
                        "Tool calls for this turn reached the limit (8). If the task is unfinished, please split it into smaller steps." },

/* STR_TREE_EMPTY */    { "(工作区文件夹是空的)", "(the workspace folder is empty)" },
/* STR_TREE_NOTSUPPORTED */ { "这个文件类型不能用内置编辑器打开：\r\n%s\r\n\r\n要用系统默认程序打开吗？",
                            "The built-in editor cannot open this file type:\r\n%s\r\n\r\nOpen it with the default program instead?" },
/* STR_TREE_OPENWITH */ { "打开方式", "Open with" },
/* STR_TREE_DELCONFIRM */ { "确实要删除这个文件吗？\r\n\r\n%s\r\n\r\n删除后无法从回收站恢复。",
                         "Delete this file?\r\n\r\n%s\r\n\r\nIt will not go to the Recycle Bin." },
/* STR_TREE_NEWFILE_PROMPT */ { "请输入新文件的名称（例如 笔记.txt）：", "Enter the new file name (for example notes.txt):" },
/* STR_TREE_LOADCONV */ { "载入会话", "Load conversation" },
/* STR_TREE_SAVEFIRST */ { "当前会话尚未保存，要保存吗？", "The current conversation has not been saved. Save it now?" },
/* STR_TREE_NAMEBAD */  { "文件名不能为空，也不能包含下列字符：\\ / : * ? \" < > |",
                        "The file name cannot be empty and cannot contain: \\ / : * ? \" < > |" },
/* STR_TREE_EXISTS */   { "同名的文件已经存在，请换一个名称。", "A file with that name already exists. Please choose another name." },

/* STR_WS_NEW_PROMPT */ { "请输入新工作区的名称：", "Enter a name for the new workspace:" },
/* STR_WS_RENAME_PROMPT */ { "请输入新的工作区名称：", "Enter the new workspace name:" },
/* STR_WS_DELCONFIRM */ { "确实要删除工作区“%s”及其中的全部会话文件吗？", "Delete workspace \"%s\" and all conversation files in it?" },
/* STR_WS_DEFAULT */    { "默认工作区", "Default" },
/* STR_WS_NOFILES */    { "工作区中没有会话文件", "No conversation files in this workspace" },
/* STR_WS_CREATED */    { "工作区已创建：%s", "Workspace created: %s" },
/* STR_WS_DELETED */    { "工作区已删除。", "Workspace deleted." },

/* STR_SET_TITLE */     { "设置", "Options" },
/* STR_SET_GROUP_SRV */ { "服务器(&S)", "&Server" },
/* STR_SET_URL */       { "服务器地址(&U)：", "Server &URL:" },
/* STR_SET_MODEL */     { "型号名称(&M)：", "&Model name:" },
/* STR_SET_KEY */       { "API 密钥(&K)：", "API &key:" },
/* STR_SET_TIMEOUT */   { "超时(秒)：", "Timeout (s):" },
/* STR_SET_TOKENS */    { "最长输出：", "Max output:" },
/* STR_SET_TOKENS_HINT */ { "（令牌数）", "(tokens)" },
/* STR_SET_GROUP_OPT */ { "选项(&O)", "&Options" },
/* STR_SET_TOOLS */     { "允许智能助手调用本机工具(&T)", "Allow the assistant to use local &tools" },
/* STR_SET_CONFIRM */   { "写文件、执行命令前先征求我的同意(&C)", "Ask for my &confirmation before writing files or running commands" },
/* STR_SET_CERTDATE */  { "忽略证书日期错误（本机时间不准确时使用）(&D)", "Ignore certificate &date errors (use when the clock is wrong)" },
/* STR_SET_TEST */      { "测试连接(&T)", "Tes&t connection" },
/* STR_SET_TESTING */   { "正在测试连接...", "Testing connection..." },
/* STR_SET_URL_EMPTY */ { "服务器地址不能为空。\r\n\r\n示例：\r\nhttps://api.example.com/v1/chat/completions",
                        "The server URL cannot be empty.\r\n\r\nExample:\r\nhttps://api.example.com/v1/chat/completions" },
/* STR_SET_URL_BAD */   { "服务器地址必须以 http:// 或 https:// 开头。", "The server URL must start with http:// or https://." },
/* STR_SET_TEST_TITLE */{ "连接测试", "Connection test" },
/* STR_SET_SAVED */     { "设置已保存。", "Options saved." },

/* STR_ACT_TITLE */     { "激活产品", "Activate Product" },
/* STR_ACT_PRODUCT */   { "产品(&P)：" SM_PRODUCT_NAME, "&Product: " SM_PRODUCT_NAME_EN },
/* STR_ACT_PROMPT */    { "请输入产品序列号（CDK）：", "Enter your product key (CDK):" },
/* STR_ACT_FORMAT */    { "序列号由 5 组、每组 4 位字符组成，例如：", "The key has 5 groups of 4 characters, for example:" },
/* STR_ACT_EXAMPLE */   { "    2K7M-4XQ9-BT3P-DE8N-VW6C", "    2K7M-4XQ9-BT3P-DE8N-VW6C" },
/* STR_ACT_GROUP */     { "当前授权状态(&S)", "Current &license status" },
/* STR_ACT_EDITION */   { "产品版本：%s", "Edition: %s" },
/* STR_ACT_STATE */     { "授权状态：%s", "Status: %s" },
/* STR_ACT_INSTALLDATE */ { "安装日期：%s", "Installed: %s" },
/* STR_ACT_TRIALLEFT */ { "试用期限：剩余 %d 天 / %d 次会话", "Trial: %d days / %d conversations left" },
/* STR_ACT_REGKEY */    { "已登记的序列号：%s", "Registered key: %s" },
/* STR_ACT_WARN */      { "警告：检测到授权信息被修改，本程序可能无法正常工作。",
                        "Warning: the license information has been modified; the program may not work correctly." },
/* STR_ACT_GO */        { "激活(&A)", "&Activate" },
/* STR_ACT_LATER */     { "以后再说(&L)", "&Later" },
/* STR_ACT_OKMSG */     { "激活成功！\r\n\r\n本产品已获得合法授权，感谢您使用 " SM_COMPANY " 的产品。",
                        "Activation successful!\r\n\r\nThank you for using a " SM_COMPANY_EN " product." },
/* STR_ACT_BADKEY */    { "产品序列号无效。请检查输入是否正确，或与经销商联系更换。",
                        "Invalid product key. Please check the characters, or contact your dealer." },
/* STR_ACT_EMPTY */     { "请输入产品序列号（CDK）。", "Please enter your product key (CDK)." },

/* STR_ABOUT_TITLE */   { "关于 " SM_PRODUCT_NAME, "About " SM_PRODUCT_NAME_EN },
/* STR_ABOUT_TAGLINE */ { "老机器上的桌面智能助手", "A desktop assistant for old machines" },
/* STR_ABOUT_TLS1 */    { "本产品通过内置的 SSL/TLS 1.2 组件与云端服务器通信，",
                        "This product talks to cloud servers through a built-in SSL/TLS 1.2 stack," },
/* STR_ABOUT_TLS2 */    { "不依赖操作系统的加密服务，因此在 Windows 95/98/2000/XP",
                        "independently of the operating system crypto services, so it also works" },
/* STR_ABOUT_TLS3 */    { "上也可以正常使用现代安全连接。", "on Windows 95/98/2000/XP with modern secure connections." },
/* STR_ABOUT_LICSTATE */{ "本机授权状态：%s", "License status: %s" },
/* STR_ABOUT_SYSINFO */ { "系统信息(&I)...", "System &Information..." },

/* STR_SI_TITLE */      { "系统信息", "System Information" },
/* STR_SI_COPY */       { "复制到剪贴板(&C)", "&Copy to clipboard" },
/* STR_SI_SAVEAS */     { "另存为(&S)...", "Save &As..." },
/* STR_SI_HEADER */     { "【系统信息】", "[System information]" },

/* STR_CONNECTING */    { "正在连接服务器...", "Connecting to server..." },
/* STR_CONNECTING_STOP */ { "正在停止...", "Stopping..." },
/* STR_PLEASE_WAIT */   { "请稍候", "Please wait" },
/* STR_HTTP_ERR */      { "服务器返回 HTTP %d。", "The server returned HTTP %d." },
/* STR_EDITOR_TITLE */  { SM_SHORT_NAME " 编辑器", SM_SHORT_NAME_EN " Editor" },
/* STR_EDITOR_UNTITLED */ { "无标题", "Untitled" },
/* STR_EDITOR_SAVEPROMPT */ { "文件已经被修改，是否保存？", "The file has been modified. Save it?" },
/* STR_BTN_SEND */   { "发送(&S)", "&Send" },
/* STR_BTN_STOP */   { "停止(&E)", "St&op" },
/* STR_BTN_NEWWS */  { "新建(&N)...", "&New..." },
/* STR_LIC_TRIAL_FMT */ { "试用版 剩余%d天/%d次", "Trial - %d days / %d left" },
/* STR_LIC_LICENSED_FMT */ { "已授权 (%s)", "Licensed (%s)" },
/* STR_LIC_EXPIRED */ { "试用已到期", "Trial expired" },
/* STR_LIC_TAMPERED */ { "授权已损坏", "License corrupted" },
/* STR_ED_STD */     { "标准版", "Standard" },
/* STR_ED_PRO */     { "专业版", "Professional" },
/* STR_ED_ENT */     { "企业版", "Enterprise" },
/* STR_MSG_CTXNOTE */ { "（此前的对话上下文不会被带入，请重新说明需求。）",
                        "(Earlier context is not restored; please restate your request.)" },
};

const char *L(StrId id)
{
    if ((int)id < 0 || (int)id >= (int)S__COUNT) return "";
    return T[id][g_lang] ? T[id][g_lang] : T[id][0];
}

const char *TS(const char *zh, const char *en)
{
    return (g_lang == LANG_EN) ? en : zh;
}

void nb_lang_init(void)
{
    char *ini = sm_ini_path();
    char *lv = sm_ini_get(ini, "display", "language", "");
    if (lv[0]) g_lang = (sm_stricmp(lv, "en") == 0) ? LANG_EN : LANG_ZH;
    else g_lang = nb_lang_from_system();
    sm_free(lv);
    sm_free(ini);
}

int nb_lang_from_system(void)
{
    /* 安装程序等独立程序用它决定初始语言：中文系统用中文，其余用英文 */
    /* GetSystemDefaultUILanguage 是 NT 专有导出；Windows 98/ME 上没有，
     * 静态导入会让整个 EXE 加载失败，所以按需取，取不到退回 GetUserDefaultLangID。 */
    typedef LANGID (WINAPI *PFN_GSDUIL)(void);
    static PFN_GSDUIL pfn = NULL;
    static int tried = 0;
    LANGID lid;
    if (!tried) {
        HMODULE k = GetModuleHandleA("kernel32.dll");
        if (!k) k = LoadLibraryA("kernel32.dll");
        if (k) pfn = (PFN_GSDUIL)GetProcAddress(k, "GetSystemDefaultUILanguage");
        tried = 1;
    }
    lid = pfn ? pfn() : GetUserDefaultLangID();
    return (PRIMARYLANGID(lid) == LANG_CHINESE) ? LANG_ZH : LANG_EN;
}

const char *P_NAME(void) { return SM_PRODUCT_NAME; }
const char *P_COMPANY(void) { return SM_COMPANY; }

int lang_get(void) { return g_lang; }

void lang_set(int lang)
{
    g_lang = (lang == LANG_EN) ? LANG_EN : LANG_ZH;
}

void lang_load(void)
{
    char *ini = sm_ini_path();
    char *v = sm_ini_get(ini, "display", "language", "zh");
    if (!sm_stricmp(v, "en") || !sm_stricmp(v, "english")) g_lang = LANG_EN;
    else g_lang = LANG_ZH;
    sm_free(v);
    sm_free(ini);
}

const char *nb_edition_name(int edition)
{
    switch (edition) {
    case 2:  return L(STR_ED_PRO);
    case 3:  return L(STR_ED_ENT);
    default: return L(STR_ED_STD);
    }
}


const char *nb_now_text(char *buf, int len)
{
    SYSTEMTIME st;
    static const char *wd_en[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static const char *wd_zh[] = { "日", "一", "二", "三", "四", "五", "六" };
    GetLocalTime(&st);
    if (!buf || len <= 0) return "";
    if (g_lang == LANG_EN)
        _snprintf(buf, (size_t)len, "%s %04d-%02d-%02d %02d:%02d",
                  wd_en[st.wDayOfWeek % 7], st.wYear, st.wMonth, st.wDay,
                  st.wHour, st.wMinute);
    else
        _snprintf(buf, (size_t)len, "%d年%d月%d日 星期%s %02d:%02d",
                  st.wYear, st.wMonth, st.wDay,
                  wd_zh[st.wDayOfWeek % 7], st.wHour, st.wMinute);
    buf[len - 1] = 0;
    return buf;
}

const char *nb_os_text(char *buf, int len)
{
    char *os = sm_get_os_name();
    char *paren;
    if (!buf || len <= 0) { sm_free(os); return ""; }
    if (g_lang == LANG_EN) {
        paren = strchr(os, 40);
        if (paren) { *paren = 0; sm_trim(os); }
    }
    _snprintf(buf, (size_t)len, "%s", os);
    buf[len - 1] = 0;
    sm_free(os);
    return buf;
}
const char *nb_lic_text(const void *licp, char *buf, int len)
{
    const SmLicense *lic = (const SmLicense *)licp;
    char fmt[96];
    if (!buf || len <= 0) return "";
    switch (lic->state) {
    case SM_LIC_ACTIVATED:
        strncpy(fmt, L(STR_LIC_LICENSED_FMT), sizeof(fmt) - 1);
        fmt[sizeof(fmt) - 1] = 0;
        _snprintf(buf, (size_t)len, fmt, nb_edition_name(lic->edition));
        break;
    case SM_LIC_EXPIRED:
        _snprintf(buf, (size_t)len, "%s", lic->tampered ? L(STR_LIC_TAMPERED) : L(STR_LIC_EXPIRED));
        break;
    default:
        strncpy(fmt, L(STR_LIC_TRIAL_FMT), sizeof(fmt) - 1);
        fmt[sizeof(fmt) - 1] = 0;
        _snprintf(buf, (size_t)len, fmt, lic->trial_days_left, lic->trial_msgs_left);
        break;
    }
    buf[len - 1] = 0;
    return buf;
}

void lang_save(void)
{
    char *ini = sm_ini_path();
    sm_ini_set(ini, "display", "language", g_lang == LANG_EN ? "en" : "zh");
    sm_free(ini);
}
