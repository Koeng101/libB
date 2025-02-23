#include "lua_eval.h"
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <string.h>
#include <stdio.h>
#include "dnadesign.h"

#define MAX_RESPONSE_SIZE 32768

/*
 * Lua Evaluation
 * -------------
 * Creates a new Lua state per evaluation for isolation.
 * Uses thread-local storage for result buffer to avoid concurrency issues.
 *
 * Design Decisions:
 * - New Lua state per eval prevents state pollution
 * - Thread-local storage avoids need for result buffer synchronization
 * - Fixed result size prevents memory management complexity
 *
 * Alternative Approaches:
 * - Lua state pool: Rejected due to state cleanup complexity
 * - Dynamic result buffer: Rejected due to memory management complexity
 */
char* lua_eval_string(const char* input) {
    static __thread char result[MAX_RESPONSE_SIZE];  // Thread-local storage
    
    lua_State *L = luaL_newstate();
    if (!L) {
        strcpy(result, "Error: Could not create Lua state");
        return result;
    }

    luaL_openlibs(L);

	// Load the bytecode and store return value in global 'dnadesign'
    if (luaL_loadbuffer(L, (const char*)luaJIT_BC_dnadesign,
                        luaJIT_BC_dnadesign_SIZE, "dnadesign") ||
        lua_pcall(L, 0, 1, 0)) {  // Note: changed to 1 return value
        snprintf(result, MAX_RESPONSE_SIZE, "Error loading bytecode: %s", 
                 lua_tostring(L, -1));
        lua_close(L);
        return result;
    }
    
    // Store the returned package in global 'dnadesign'
    lua_setglobal(L, "dnadesign");

    if (luaL_loadstring(L, input) || lua_pcall(L, 0, 1, 0)) {
        snprintf(result, MAX_RESPONSE_SIZE, "Error: %s", lua_tostring(L, -1));
        lua_close(L);
        return result;
    }

    if (lua_isnil(L, -1)) {
        strcpy(result, "nil");
    } else {
        const char* str = lua_tostring(L, -1);
        if (str) {
            strncpy(result, str, MAX_RESPONSE_SIZE - 1);
            result[MAX_RESPONSE_SIZE - 1] = '\0';
        } else {
            strcpy(result, "Error: Could not convert result to string");
        }
    }

    lua_close(L);
    return result;
}
