// qkregex.c —— QuarkLang regex 库的 PCRE2 后端（把正则匹配这个性能服务托管给 C 库）。
//
// 设计：C 侧只做「编译 / 设主题 / 执行 / 取偏移」，其余（findAll/split/replace 的循环与
// $N 替换展开）留在 qk 里，保证与纯 qk 引擎的语义逐点一致（tests/test_regex.qk 为等价判据）。
//
// 关键点：
//   - 编译用 PCRE2_UTF：`.` 与字符类按 rune 处理（纯 qk 引擎同样按 rune）；
//     不开 PCRE2_UCP：\w \d \s 保持 ASCII，与纯 qk 引擎一致（测试 "word-ascii"）。
//   - 偏移在 C 侧是**字节**，通过 qk_re_rune_of_byte / qk_re_byte_of_rune 与 qk 的 rune 下标互转。
//   - 被纯 qk 引擎明确拒绝的特性（反向引用、前后瞻、内联标志）由 qk 侧预校验拦下，
//     这里不做特殊处理。
//
// 构建：native/build.sh → ../bin/libqkregex.so（依赖系统 libpcre2-8）

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    pcre2_code *code;
    pcre2_match_data *md;
    char *subject;   // 主题副本（NUL 结尾；每次 set_subject 覆盖）
    int slen;        // 主题字节长度
    int ngroups;     // 捕获组数（不含整体组 0）
    int errcode;     // 0 = 编译成功
    char errmsg[200];
    int matched;     // 上次 exec 是否匹配
    int ovcount;     // 上次 exec 的 ovector 有效项数
    PCRE2_SIZE *ov;  // 指向 match_data 的 ovector
} qk_re;

static void qk_set_error(qk_re *re, int code) {
    re->errcode = code;
    PCRE2_UCHAR buf[160];
    int n = pcre2_get_error_message(code, buf, sizeof buf);
    if (n > 0) {
        snprintf(re->errmsg, sizeof re->errmsg, "%s", (const char *)buf);
    } else {
        snprintf(re->errmsg, sizeof re->errmsg, "pcre2 error %d", code);
    }
}

void *qk_re_compile(const char *pat) {
    qk_re *re = (qk_re *)calloc(1, sizeof(qk_re));
    if (!re) return NULL;
    int errcode = 0;
    PCRE2_SIZE erroff = 0;
    re->code = pcre2_compile((PCRE2_SPTR)pat, PCRE2_ZERO_TERMINATED, PCRE2_UTF,
                             &errcode, &erroff, NULL);
    if (!re->code) {
        qk_set_error(re, errcode);
        return re; // 对象仍返回：isValid() 为 false，错误信息可读
    }
    re->md = pcre2_match_data_create_from_pattern(re->code, NULL);
    if (!re->md) {
        qk_set_error(re, PCRE2_ERROR_NOMEMORY);
        return re;
    }
    uint32_t cap = 0;
    pcre2_pattern_info(re->code, PCRE2_INFO_CAPTURECOUNT, &cap);
    re->ngroups = (int)cap;
    re->errcode = 0;
    re->errmsg[0] = '\0';
    return re;
}

int qk_re_ok(void *p) {
    qk_re *re = (qk_re *)p;
    return (re && re->code && re->md && re->errcode == 0) ? 1 : 0;
}

// 返回错误信息（静态缓冲：调用方不必释放；Cleg 风格的 String 返回值）。
char *qk_re_error(void *p) {
    qk_re *re = (qk_re *)p;
    if (!re) return (char *)"null regex";
    if (re->errcode == 0) return (char *)"";
    return re->errmsg;
}

int qk_re_groups(void *p) {
    qk_re *re = (qk_re *)p;
    return re ? re->ngroups : 0;
}

// 设置主题（复制一份：后续 exec 不再传串，避免每次匹配都 malloc+copy）
int qk_re_set_subject(void *p, const char *s) {
    qk_re *re = (qk_re *)p;
    if (!re) return -1;
    size_t n = strlen(s);
    char *buf = (char *)realloc(re->subject, n + 1);
    if (!buf) return -1;
    memcpy(buf, s, n + 1);
    re->subject = buf;
    re->slen = (int)n;
    return 0;
}

// 执行匹配（带选项）：1=锚定，2=全串
int qk_re_match_at(void *p, int startbyte, int anchored, int endanchored) {
    qk_re *re = (qk_re *)p;
    if (!re || !re->code || !re->md) return 0;
    if (startbyte < 0) startbyte = 0;
    if (startbyte > re->slen) return 0;
    uint32_t opts = 0;
    if (anchored) opts |= PCRE2_ANCHORED;
    if (endanchored) opts |= PCRE2_ANCHORED | PCRE2_ENDANCHORED;
    int rc = pcre2_match(re->code, (PCRE2_SPTR)re->subject, (PCRE2_SIZE)re->slen,
                         (PCRE2_SIZE)startbyte, opts, re->md, NULL);
    if (rc < 0) {
        re->matched = 0;
        re->ovcount = 0;
        re->ov = NULL;
        return 0;
    }
    re->matched = 1;
    re->ov = pcre2_get_ovector_pointer(re->md);
    re->ovcount = (rc == 0) ? (re->ngroups + 1) : rc;
    return 1;
}

int qk_re_matched(void *p) {
    qk_re *re = (qk_re *)p;
    return re ? re->matched : 0;
}

int qk_re_gstart(void *p, int i) {
    qk_re *re = (qk_re *)p;
    if (!re || !re->matched || i < 0 || i >= re->ovcount) return -1;
    PCRE2_SIZE v = re->ov[2 * i];
    if (v == PCRE2_UNSET) return -1;
    return (int)v;
}

int qk_re_gend(void *p, int i) {
    qk_re *re = (qk_re *)p;
    if (!re || !re->matched || i < 0 || i >= re->ovcount) return -1;
    PCRE2_SIZE v = re->ov[2 * i + 1];
    if (v == PCRE2_UNSET) return -1;
    return (int)v;
}

int qk_re_slen(void *p) {
    qk_re *re = (qk_re *)p;
    return re ? re->slen : 0;
}

// UTF-8：字节偏移 → rune 下标（前缀中完整 rune 个数）
int qk_re_rune_of_byte(void *p, int byteoff) {
    qk_re *re = (qk_re *)p;
    if (!re) return 0;
    if (byteoff < 0) byteoff = 0;
    if (byteoff > re->slen) byteoff = re->slen;
    int runes = 0;
    for (int i = 0; i < byteoff; i++) {
        if (((unsigned char)re->subject[i] & 0xC0) != 0x80) runes++;
    }
    return runes;
}

// UTF-8：rune 下标 → 字节偏移
int qk_re_byte_of_rune(void *p, int runeoff) {
    qk_re *re = (qk_re *)p;
    if (!re) return 0;
    if (runeoff <= 0) return 0;
    int runes = 0;
    for (int i = 0; i < re->slen; i++) {
        if (((unsigned char)re->subject[i] & 0xC0) != 0x80) {
            if (runes == runeoff) return i;
            runes++;
        }
    }
    return re->slen;
}

void qk_re_free(void *p) {
    qk_re *re = (qk_re *)p;
    if (!re) return;
    if (re->md) pcre2_match_data_free(re->md);
    if (re->code) pcre2_code_free(re->code);
    free(re->subject);
    free(re);
}
