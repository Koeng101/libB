#ifndef LUA_EVAL_H
#define LUA_EVAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "cJSON.h"

// Configuration structure for the Lua sandbox
typedef struct {
    size_t memory_limit;   // Maximum memory in bytes
    int cpu_timeout;       // Maximum execution time in milliseconds
} lua_sandbox_config;

// Status codes for Lua evaluation
typedef enum {
    LUA_EVAL_SUCCESS = 0,
    LUA_EVAL_ERROR = 1
} lua_eval_status;

// Result structure containing output, debug info, and status
typedef struct {
    lua_eval_status status;  // Success or error status
    char* output;            // JSON/string output from evaluation or error message
    char* debug_output;      // Captured print statements
} lua_eval_result;

// Initializes a default sandbox configuration
lua_sandbox_config lua_default_config(void);

// Main evaluation function with configuration options
lua_eval_result lua_eval_string_sandbox(const char* input, lua_sandbox_config config);

// Free memory associated with evaluation result
void lua_free_result(lua_eval_result result);

// Function to split a byte array into lines for protocol parsing
// Returns both the full string and an array of lines
void lua_split_protocol_bytes(lua_State* L, const char* bytes, size_t length);

// Function to extract protocol functions and their metadata
// Returns a JSON string with function names, hashes, and source code
// Caller is responsible for freeing the returned string
char* extract_protocol_functions(const char* bytes, size_t length);

#ifdef __cplusplus
}
#endif

#endif // LUA_EVAL_H
