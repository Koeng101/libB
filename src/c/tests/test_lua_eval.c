#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
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

void test_protocol_line_splitting() {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    
    // Test case 1: Basic line splitting
    const char* test1 = "line1\nline2\nline3";
    printf("Testing basic line splitting... ");
    lua_split_protocol_bytes(L, test1, strlen(test1));
    
    // Check results - should have a string and a table on the stack
    assert(lua_isstring(L, -2));
    assert(lua_istable(L, -1));
    
    // Verify line count
    size_t line_count = lua_objlen(L, -1);
    assert(line_count == 3);
    
    // Verify individual lines
    lua_rawgeti(L, -1, 1);
    assert(strcmp(lua_tostring(L, -1), "line1") == 0);
    lua_pop(L, 1);
    
    lua_rawgeti(L, -1, 2);
    assert(strcmp(lua_tostring(L, -1), "line2") == 0);
    lua_pop(L, 1);
    
    lua_rawgeti(L, -1, 3);
    assert(strcmp(lua_tostring(L, -1), "line3") == 0);
    lua_pop(L, 1);
    
    // Clear stack
    lua_pop(L, 2);
    printf("✓ Passed\n");
    
    // Test case 2: Empty lines and trailing newline
    const char* test2 = "line1\n\nline3\n";
    printf("Testing empty lines and trailing newline... ");
    lua_split_protocol_bytes(L, test2, strlen(test2));
    
    // Verify line count
    line_count = lua_objlen(L, -1);
    assert(line_count == 4); // 3 explicit lines + empty line from trailing newline
    
    // Verify the empty line
    lua_rawgeti(L, -1, 2);
    assert(strcmp(lua_tostring(L, -1), "") == 0);
    lua_pop(L, 1);
    
    // Verify the last empty line (from trailing newline)
    lua_rawgeti(L, -1, 4);
    assert(strcmp(lua_tostring(L, -1), "") == 0);
    lua_pop(L, 1);
    
    // Clear stack
    lua_pop(L, 2);
    printf("✓ Passed\n");
    
    // Test case 3: No newlines
    const char* test3 = "single line";
    printf("Testing no newlines... ");
    lua_split_protocol_bytes(L, test3, strlen(test3));
    
    // Verify line count
    line_count = lua_objlen(L, -1);
    assert(line_count == 1);
    
    // Clear stack
    lua_pop(L, 2);
    printf("✓ Passed\n");
    
    lua_close(L);
    printf("All line splitting tests passed\n");
}

void test_extract_protocol_functions() {
    // Test protocol code
    const char* protocol_code = R"(
function add(a, b)
    -- Add two numbers
    return a + b
end

function multiply(a, b)
    -- Multiply two numbers
    return a * b
end

return {
    add = add,
    multiply = multiply
}
)";

    // Extract function info
    char* result = extract_protocol_functions(protocol_code, strlen(protocol_code));
    if (!result) {
        printf("Error: Failed to extract functions\n");
        return;
    }

    // Parse the JSON result
    cJSON* json = cJSON_Parse(result);
    if (!json) {
        printf("Error: Failed to parse JSON result\n");
        free(result);
        return;
    }

    // Verify structure
    cJSON* functions = cJSON_GetObjectItem(json, "functions");
    if (!functions || !cJSON_IsArray(functions)) {
        printf("Error: Result missing 'functions' array\n");
        cJSON_Delete(json);
        free(result);
        return;
    }

    // Check number of functions
    int num_functions = cJSON_GetArraySize(functions);
    if (num_functions != 2) {
        printf("Error: Expected 2 functions, got %d\n", num_functions);
        cJSON_Delete(json);
        free(result);
        return;
    }

    // Check each function
    for (int i = 0; i < num_functions; i++) {
        cJSON* func = cJSON_GetArrayItem(functions, i);
        
        // Check required fields
        cJSON* name = cJSON_GetObjectItem(func, "name");
        cJSON* source = cJSON_GetObjectItem(func, "source");
        cJSON* hash = cJSON_GetObjectItem(func, "hash");

        if (!name || !cJSON_IsString(name)) {
            printf("Error: Function missing 'name' field\n");
            continue;
        }
        if (!source || !cJSON_IsString(source)) {
            printf("Error: Function missing 'source' field\n");
            continue;
        }
        if (!hash || !cJSON_IsString(hash) || strlen(hash->valuestring) != 64) {
            printf("Error: Function has invalid hash\n");
            continue;
        }
    }

    printf("All function extraction tests passed!\n");

    // Clean up
    cJSON_Delete(json);
    free(result);
}


int main() {
    test_basic_evaluation();
    test_string_manipulation();
    test_error_handling();
    test_dnadesign_reverse_complement();
    test_memory_limit();
    test_debug_output();
    test_table_to_json();
	test_protocol_line_splitting();
	test_extract_protocol_functions();
    
    printf("All tests passed!\n");
    return 0;
}
