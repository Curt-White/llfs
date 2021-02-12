CC := gcc
CFLAGS := -Wall -Werror -Wno-unused-variable -std=c11

TESTS := test01 test02 test_file test_system test_journal

all: $(TESTS)

clean:
	rm -rf $(TESTS) *.dSYM *disk

%: %.c $(wildcard ../disk/*.c) $(wildcard ../io/*.c)
	$(CC) $(CFLAGS) -o $@ $< $(wildcard ../disk/*.c) $(wildcard ../io/*.c) -lm

run: clean $(TESTS)
	$(foreach exec, $(TESTS), ./$(exec);)
