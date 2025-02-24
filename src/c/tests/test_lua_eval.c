#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../src/lua_eval.h"

void test_basic_evaluation() {
    lua_sandbox_config config = lua_default_config();
    lua_eval_result result = lua_eval_string_sandbox("return 2 + 2", config);
    
    assert(strcmp(result.output, "4") == 0);
    printf("✓ Basic arithmetic works\n");
    
    lua_free_result(result);
}

void test_string_manipulation() {
    lua_sandbox_config config = lua_default_config();
    lua_eval_result result = lua_eval_string_sandbox("return string.upper('hello')", config);
    
    assert(strcmp(result.output, "HELLO") == 0);
    printf("✓ String manipulation works\n");
    
    lua_free_result(result);
}

void test_error_handling() {
    lua_sandbox_config config = lua_default_config();
    lua_eval_result result = lua_eval_string_sandbox("invalid lua code", config);
    
    assert(result.status == LUA_EVAL_ERROR);
    printf("✓ Error handling works\n");
    
    lua_free_result(result);
}

void test_dnadesign_reverse_complement() {
    lua_sandbox_config config = lua_default_config();
    lua_eval_result result = lua_eval_string_sandbox(
        "return dnadesign.transform.reverse_complement('GATTACA')", 
        config
    );
    
    assert(strcmp(result.output, "TGTAATC") == 0);
    printf("✓ dnadesign reverse_complement works\n");
    
    lua_free_result(result);
}

void test_memory_limit() {
    lua_sandbox_config config = lua_default_config();
    config.memory_limit = 50000; // Very small limit
    
    lua_eval_result result = lua_eval_string_sandbox(
        "local t = {} for i=1,10000 do t[i] = string.rep('x', 100) end return #t", 
        config
    );
    
    assert(result.status == LUA_EVAL_ERROR || strcmp(result.output, "10000") != 0);
    printf("✓ Memory limit works\n");
    
    lua_free_result(result);
}

void test_debug_output() {
    lua_sandbox_config config = lua_default_config();
    lua_eval_result result = lua_eval_string_sandbox(
        "print('debug message 1')\nprint('debug message 2')\nreturn 42", 
        config
    );
    
    assert(strcmp(result.output, "42") == 0);
    assert(strstr(result.debug_output, "debug message 1") != NULL);
    assert(strstr(result.debug_output, "debug message 2") != NULL);
    printf("✓ Debug output capture works\n");
    
    lua_free_result(result);
}

void test_table_to_json() {
    lua_sandbox_config config = lua_default_config();
    
    // Test simple table
    lua_eval_result result = lua_eval_string_sandbox(
        "return {a=1, b='text'}", 
        config
    );
    
    assert(strstr(result.output, "\"a\"") != NULL);
    assert(strstr(result.output, "\"b\"") != NULL);
    assert(strstr(result.output, "\"text\"") != NULL);
    
    lua_free_result(result);
    
    // Test complex nested table
    result = lua_eval_string_sandbox(
        "return {a=1, b='text', c={nested=true, array={1,2,3}}}", 
        config
    );
    
    assert(strstr(result.output, "\"nested\"") != NULL);
    assert(strstr(result.output, "\"array\"") != NULL);
    assert(strstr(result.output, "true") != NULL);
    
    lua_free_result(result);
    printf("✓ Table to JSON conversion works\n");
}

int main() {
    test_basic_evaluation();
    test_string_manipulation();
    test_error_handling();
    test_dnadesign_reverse_complement();
    test_memory_limit();
    test_debug_output();
    test_table_to_json();
    
    printf("All tests passed!\n");
    return 0;
}
