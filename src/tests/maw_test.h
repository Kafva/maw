#ifndef MAW_TEST_H
#define MAW_TEST_H

#ifdef MAW_TEST
int test_maw_update(void);

#define RUN_TEST(desc, fn) do { \
    if (fn() == 0) { \
        fprintf(stderr, "[tests] %s: \033[92mOK\033[0m\n", desc); \
    } else { \
        fprintf(stderr, "[tests] %s: \033[91mFAILED\033[0m\n", desc); \
    } \
} while (0)

#endif

#endif // MAW_TEST_H
