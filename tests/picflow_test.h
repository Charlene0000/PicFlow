/*
 * picflow_test.h
 * PicFlow Core —— 极简测试框架（零依赖）
 *
 * 刻意不引入任何测试框架：让项目在 Windows / Linux / macOS 上
 * 用同一个 gcc/clang 命令就能跑，不需要网络下载依赖。
 *
 * 每个测试文件编译成独立可执行文件，因此计数器用 static，
 * 避免多文件链接时的符号重复。
 */

#ifndef PICFLOW_TEST_H
#define PICFLOW_TEST_H

#include <stdio.h>
#include <string.h>
#include <math.h>

static int g_test_total = 0;
static int g_test_failed = 0;
static int g_test_current_failed = 0;

#define PF_TEST_BEGIN(name) \
    do { g_test_current_failed = 0; printf("  [ RUN  ] %s\n", name); } while (0)

#define PF_TEST_END(name) \
    do { \
        g_test_total++; \
        if (g_test_current_failed) { \
            g_test_failed++; \
            printf("  [ FAIL ] %s\n", name); \
        } else { \
            printf("  [  OK  ] %s\n", name); \
        } \
    } while (0)

#define PF_ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            g_test_current_failed = 1; \
            printf("    x %s:%d 断言失败: %s (%s)\n", __FILE__, __LINE__, msg, #cond); \
        } \
    } while (0)

#define PF_ASSERT_EQ_INT(actual, expected, msg) \
    do { \
        long _a = (long)(actual), _e = (long)(expected); \
        if (_a != _e) { \
            g_test_current_failed = 1; \
            printf("    x %s:%d %s 期望 %ld，实际 %ld\n", __FILE__, __LINE__, msg, _e, _a); \
        } \
    } while (0)

#define PF_ASSERT_NEAR(actual, expected, tolerance, msg) \
    do { \
        double _a = (double)(actual), _e = (double)(expected); \
        if (fabs(_a - _e) > (tolerance)) { \
            g_test_current_failed = 1; \
            printf("    x %s:%d %s 期望 %.4f，实际 %.4f\n", __FILE__, __LINE__, msg, _e, _a); \
        } \
    } while (0)

/* 两个指针必须分开声明：写成 const char *_a = ..., _e = ... 时，_e 会继承 char 类型而非 char *。 */
#define PF_ASSERT_STR_EQ(actual, expected, msg) \
    do { \
        const char *_a = (const char *)(actual); \
        const char *_e = (const char *)(expected); \
        if (_a == NULL || _e == NULL || strcmp(_a, _e) != 0) { \
            g_test_current_failed = 1; \
            printf("    x %s:%d %s 期望 \"%s\"，实际 \"%s\"\n", \
                   __FILE__, __LINE__, msg, _e ? _e : "(null)", _a ? _a : "(null)"); \
        } \
    } while (0)

/* 各测试文件实现的 main 统一调用这个宏收尾 */
#define PF_TEST_REPORT(suite_name) \
    do { \
        printf("\n----------------------------------------\n"); \
        printf("  %s：共 %d 组测试，失败 %d 组\n", suite_name, g_test_total, g_test_failed); \
        printf("========================================\n\n"); \
        return (g_test_failed == 0) ? 0 : 1; \
    } while (0)

#endif /* PICFLOW_TEST_H */
