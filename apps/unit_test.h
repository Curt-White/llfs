//
// Created by curt white on 2020-03-10.
//

// This code is a small unit test method found online at: http://www.jera.com/techinfo/jtns/jtn002.html
#define pass() printf("\033[0;33mPASS: %s\n", __func__)
#define fail() printf("\033[0;31mFAIL: %s\n", __func__)

#define fail_msg(msg) printf("\033[1;31m%s\n", msg);
#define pass_all() printf("\033[1;32mAll Tests Passes\n")

#define test_header() printf("\033[1;34mRunning Tests For: %s\n", __FILE__);

#define unit_assert(message, test) do { if (!(test)){ fail(); return message; } } while (0)
#define unit_run(test) do { const char *message = test(); tests_run++; \
                                        if (message) return message; } while (0)

typedef const char*(*unit)();
int tests_run = 0;

// Run all of the tests provided, return message on error else 0
const char *run_tests(unit *fns, int num_tests) {
    for (int i = 0; i < num_tests; i++) {
        unit_run(fns[i]);
    }

    return 0;
}