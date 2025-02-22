#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../src/lua_eval.h"

void test_basic_evaluation() {
    const char* result = lua_eval_string("return 2 + 2");
    assert(strcmp(result, "4") == 0);
    printf("✓ Basic arithmetic works\n");
}

void test_string_manipulation() {
    const char* result = lua_eval_string("return string.upper('hello')");
    assert(strcmp(result, "HELLO") == 0);
    printf("✓ String manipulation works\n");
}

void test_error_handling() {
    const char* result = lua_eval_string("invalid lua code");
    assert(strstr(result, "Error") != NULL);
    printf("✓ Error handling works\n");
}

void test_dnadesign_reverse_complement() {
    const char* result = lua_eval_string(" return dnadesign.transform.reverse_complement('GATTACA')");
    assert(strcmp(result, "TGTAATC") == 0);
    printf("✓ dnadesign reverse_complement works\n");
}

int main() {
    test_basic_evaluation();
    test_string_manipulation();
    test_error_handling();
	test_dnadesign_reverse_complement();
    printf("All tests passed!\n");
    return 0;
}
