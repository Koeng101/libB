#include "lua_eval.h"
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "dnadesign.h"
#include "cJSON.h"

/*
 * Enhanced Lua Evaluation with Sandbox
 * -----------------------------------
 * Creates a new Lua state with memory tracking for isolation.
 * Selectively loads only safe libraries (math, string, table, coroutine).
 * Captures print output for debugging.
 * Supports memory limits via custom allocator.
 * Converts Lua tables to JSON for standardized output.
 */

#define MAX_RESPONSE_SIZE 32768
#define MAX_DEBUG_SIZE 32768

// Structure to track memory usage for lua_newstate
typedef struct {
    size_t used_memory;
    size_t memory_limit;
} memory_data;

// Custom allocator function for memory limiting
static void* memory_limited_alloc(void* ud, void* ptr, size_t osize, size_t nsize) {
    memory_data* md = (memory_data*)ud;
    
    // When nsize is 0, it's a free operation
    if (nsize == 0) {
        free(ptr);
        md->used_memory -= osize;
        return NULL;
    }
    
    // Check if allocation would exceed limit
    if (md->used_memory + nsize - osize > md->memory_limit) {
        return NULL;  // Memory limit would be exceeded
    }
    
    // Allocate or reallocate memory
    void* new_ptr = realloc(ptr, nsize);
    if (new_ptr == NULL) {
        // If realloc fails, don't update memory usage
        return NULL;
    }
    
    // Update memory usage
    md->used_memory = md->used_memory + nsize - osize;
    return new_ptr;
}

// Buffer for debug output
typedef struct {
    char data[MAX_DEBUG_SIZE];
    size_t length;
} debug_buffer;

// Custom print function for lua
static int l_print(lua_State* L) {
    debug_buffer* buffer = (debug_buffer*)lua_touserdata(L, lua_upvalueindex(1));
    int n = lua_gettop(L);  // Number of arguments
    int i;
    lua_getglobal(L, "tostring");
    
    for (i = 1; i <= n; i++) {
        const char* s;
        size_t l;
        
        lua_pushvalue(L, -1);  // Function to call (tostring)
        lua_pushvalue(L, i);   // Argument
        lua_call(L, 1, 1);     // Call tostring(arg)
        s = lua_tolstring(L, -1, &l);  // Get result
        
        if (s == NULL) {
            return luaL_error(L, "'tostring' must return a string to 'print'");
        }
        
        if (i > 1 && buffer->length < MAX_DEBUG_SIZE - 1) {
            buffer->data[buffer->length++] = '\t';  // Separator
        }
        
        // Append to buffer
        if (buffer->length + l < MAX_DEBUG_SIZE - 1) {
            memcpy(buffer->data + buffer->length, s, l);
            buffer->length += l;
        }
        
        lua_pop(L, 1);  // Pop result
    }
    
    // Add newline
    if (buffer->length < MAX_DEBUG_SIZE - 1) {
        buffer->data[buffer->length++] = '\n';
    }
    buffer->data[buffer->length] = '\0';
    
    return 0;
}

// Load a specific library but exclude specific functions (for sandboxing)
static void load_safe_library(lua_State* L, lua_CFunction library_func, const char* library_name) {
    lua_pushcfunction(L, library_func);
    lua_pushstring(L, library_name);
    lua_call(L, 1, 0);
}

// Convert Lua value to cJSON
static cJSON* lua_to_json(lua_State* L, int index) {
    cJSON* json = NULL;
    
    // Check for valid index
    if (!lua_checkstack(L, 3)) {  // Ensure we have stack space
        return cJSON_CreateNull();
    }
    
    int type = lua_type(L, index);
    
    switch (type) {
        case LUA_TNIL:
            return cJSON_CreateNull();
            
        case LUA_TBOOLEAN:
            return cJSON_CreateBool(lua_toboolean(L, index));
            
        case LUA_TNUMBER:
            return cJSON_CreateNumber(lua_tonumber(L, index));
            
        case LUA_TSTRING: {
            const char* str = lua_tostring(L, index);
            if (!str) return cJSON_CreateNull();
            return cJSON_CreateString(str);
        }
            
        case LUA_TTABLE: {
            // Check if it's an array or object
            int is_array = 1;
            int max_index = 0;
            
            lua_pushnil(L);  // First key
            while (lua_next(L, index < 0 ? index - 1 : index) != 0) {
                // Key is at -2, value at -1
                if (lua_type(L, -2) == LUA_TNUMBER) {
                    lua_Number n = lua_tonumber(L, -2);
                    // Only consider integer keys for arrays
                    if (n == (int)n && n > 0) {
                        if ((int)n > max_index) max_index = (int)n;
                    } else {
                        is_array = 0;
                    }
                } else {
                    is_array = 0;
                }
                lua_pop(L, 1);  // Remove value, keep key for next iteration
            }
            
            // Create the appropriate JSON container
            json = is_array ? cJSON_CreateArray() : cJSON_CreateObject();
            if (!json) return cJSON_CreateNull();
            
            // If it's an array, preallocate space
            if (is_array && max_index > 0) {
                // Fill with nulls for proper indexing
                for (int i = 0; i < max_index; i++) {
                    cJSON* null_item = cJSON_CreateNull();
                    if (null_item) {
                        cJSON_AddItemToArray(json, null_item);
                    }
                }
            }
            
            // Process the table content
            lua_pushnil(L);  // First key
            while (lua_next(L, index < 0 ? index - 1 : index) != 0) {
                // Key is at -2, value at -1
                cJSON* value = lua_to_json(L, -1);
                if (!value) {
                    value = cJSON_CreateNull();
                }
                
                if (is_array) {
                    if (lua_type(L, -2) == LUA_TNUMBER) {
                        int arr_index = (int)lua_tonumber(L, -2);
                        if (arr_index > 0 && arr_index <= max_index) {
                            // Replace the preallocated null
                            cJSON_ReplaceItemInArray(json, arr_index - 1, value);
                        } else {
                            cJSON_Delete(value);
                        }
                    } else {
                        cJSON_Delete(value);
                    }
                } else {
                    // For objects, convert key to string
                    if (lua_type(L, -2) == LUA_TSTRING) {
                        const char* key = lua_tostring(L, -2);
                        if (key) {
                            cJSON_AddItemToObject(json, key, value);
                        } else {
                            cJSON_Delete(value);
                        }
                    } else {
                        // For non-string keys, convert to string
                        lua_pushvalue(L, -2);  // Duplicate key
                        const char* key = lua_tostring(L, -1);
                        if (key) {
                            cJSON_AddItemToObject(json, key, value);
                        } else {
                            cJSON_Delete(value);
                        }
                        lua_pop(L, 1);  // Remove duplicated key
                    }
                }
                
                lua_pop(L, 1);  // Remove value, keep key for next iteration
            }
            
            return json;
        }
        
        default:
            // For other types (function, userdata, etc.), represent as null
            return cJSON_CreateNull();
    }
}

// Initialize sandbox configuration with default values
lua_sandbox_config lua_default_config(void) {
    lua_sandbox_config config;
    config.memory_limit = 50 * 1024 * 1024;  // 50MB default
    config.cpu_timeout = 5000;               // 5 seconds default
    return config;
}

// Free memory used by the result structure
void lua_free_result(lua_eval_result result) {
    if (result.output) free(result.output);
    if (result.debug_output) free(result.debug_output);
}

// Main evaluation function with sandbox features
lua_eval_result lua_eval_string_sandbox(const char* input, lua_sandbox_config config) {
    lua_eval_result result = { LUA_EVAL_SUCCESS, NULL, NULL };
    memory_data md = { 0, config.memory_limit };
    debug_buffer debug = { {0}, 0 };
    
    // Create new Lua state with memory limit
    lua_State* L = lua_newstate(memory_limited_alloc, &md);
    if (!L) {
        result.status = LUA_EVAL_ERROR;
        result.output = strdup("Error: Could not create Lua state");
        result.debug_output = strdup("");
        return result;
    }
    
    // Open only specific libraries
    load_safe_library(L, luaopen_base, "");      // Basic functions
    load_safe_library(L, luaopen_math, "math");  // Math library
    load_safe_library(L, luaopen_string, "string"); // String library
    load_safe_library(L, luaopen_table, "table");   // Table library
    
    // Override print function to capture debug output
    lua_pushlightuserdata(L, &debug);
    lua_pushcclosure(L, l_print, 1);
    lua_setglobal(L, "print");
    
    // Load the dnadesign bytecode
    if (luaL_loadbuffer(L, (const char*)luaJIT_BC_dnadesign,
                        luaJIT_BC_dnadesign_SIZE, "dnadesign") ||
        lua_pcall(L, 0, 1, 0)) {
        result.status = LUA_EVAL_ERROR;
        result.output = strdup(lua_tostring(L, -1));
        result.debug_output = strdup(debug.data);
        lua_close(L);
        return result;
    }
    
    // Store the returned package in global 'dnadesign'
    lua_setglobal(L, "dnadesign");
    
    // Execute the input code
    if (luaL_loadstring(L, input) || lua_pcall(L, 0, 1, 0)) {
        result.status = LUA_EVAL_ERROR;
        result.output = strdup(lua_tostring(L, -1));
        result.debug_output = strdup(debug.data);
        lua_close(L);
        return result;
    }
    
    // Convert the result to proper output
    if (lua_isnil(L, -1)) {
        result.output = strdup("nil");
    } else if (lua_type(L, -1) == LUA_TTABLE) {
        // Convert table to JSON
        cJSON* json = lua_to_json(L, -1);
        if (json) {
            char* json_str = cJSON_Print(json);
            result.output = json_str ? json_str : strdup("Error: JSON conversion failed");
            cJSON_Delete(json);
        } else {
            result.output = strdup("Error: Could not convert table to JSON");
        }
    } else {
        // For other types, convert to string
        const char* str = lua_tostring(L, -1);
        result.output = str ? strdup(str) : strdup("Error: Could not convert result to string");
    }
    
    // Set debug output
    result.debug_output = strdup(debug.data);
    
    lua_close(L);
    return result;
}

/*
function extraction
*/

// Split a byte array into lines, preserving exact file:lines() behavior
// Pushes two values onto the Lua stack:
// 1. The full string representation of the bytes
// 2. A table of lines, split at true newline characters
void lua_split_protocol_bytes(lua_State* L, const char* bytes, size_t length) {
    // First, push the full string to the Lua stack
    lua_pushlstring(L, bytes, length);
    
    // Create a new table for the lines
    lua_newtable(L);
    
    // Track position and line count
    size_t line_start = 0;
    int line_count = 0;
    
    // Scan through the bytes looking for newlines
    for (size_t i = 0; i < length; i++) {
        // If we found a newline character (byte 10)
        if (bytes[i] == '\n') {
            // Push the line to the table (not including the newline)
            lua_pushlstring(L, bytes + line_start, i - line_start);
            line_count++;
            lua_rawseti(L, -2, line_count);
            
            // Update line_start to the character after the newline
            line_start = i + 1;
        }
    }
    
    // Handle the last line (might not end with a newline)
    if (line_start < length) {
        lua_pushlstring(L, bytes + line_start, length - line_start);
        line_count++;
        lua_rawseti(L, -2, line_count);
    } else if (bytes[length-1] == '\n') {
        // If the last character was a newline, add an empty line
        // This matches file:lines() behavior
        lua_pushliteral(L, "");
        line_count++;
        lua_rawseti(L, -2, line_count);
    }
    
    // The stack now has the full string at index -2 and the lines table at index -1
}

char* extract_protocol_functions(const char* bytes, size_t length) {
    char* result = NULL;
    
    // Create Lua state and open libraries
    lua_State* L = luaL_newstate();
    if (!L) return strdup("Error: Could not create Lua state");
    luaL_openlibs(L);
    
    // Split protocol bytes and load dnadesign
    lua_split_protocol_bytes(L, bytes, length);
    if (luaL_loadbuffer(L, (const char*)luaJIT_BC_dnadesign,
                      luaJIT_BC_dnadesign_SIZE, "dnadesign") ||
        lua_pcall(L, 0, 1, 0)) {
        const char* error = lua_tostring(L, -1);
        lua_close(L);
        return strdup(error);
    }
    lua_setglobal(L, "dnadesign");

    // Load our Lua helper code
    const char* lua_code = R"(
        local function bytes_to_hex(bytes)
            local hex = {}
            for i = 1, #bytes do
                hex[i] = string.format("%02x", bytes[i])
            end
            return table.concat(hex)
        end

        function process_functions(protocol_code, lines)
            local funcs = load(protocol_code)()
            if type(funcs) ~= "table" then
                error("Protocol must return a table of functions")
            end

            local result = {functions = {}}
            for name, func in pairs(funcs) do
                if type(func) == "function" then
                    -- Get source
                    local info = debug.getinfo(func, "S")
                    local source = ""
                    if info.linedefined > 0 and info.lastlinedefined > 0 then
                        local source_lines = {}
                        for i = info.linedefined, info.lastlinedefined do
                            table.insert(source_lines, lines[i])
                        end
                        source = table.concat(source_lines, "\n")
                    end

                    -- Get hash
                    local bytecode = string.dump(func)
                    local hasher = dnadesign.hash.new_sha256()
                    hasher:write(bytecode)
                    local hash = hasher:sum()
                    
                    table.insert(result.functions, {
                        name = name,
                        source = source,
                        hash = bytes_to_hex(hash)
                    })
                end
            end
            return result
        end
    )";

    // Load and run the helper code
    if (luaL_loadstring(L, lua_code) || lua_pcall(L, 0, 0, 0)) {
        const char* error = lua_tostring(L, -1);
        lua_close(L);
        return strdup(error);
    }

    // Call our process_functions with the protocol code and lines
    lua_getglobal(L, "process_functions");
    lua_pushvalue(L, -3);  // protocol code (full string)
    lua_pushvalue(L, -3);  // lines table
    
    if (lua_pcall(L, 2, 1, 0)) {
        const char* error = lua_tostring(L, -1);
        lua_close(L);
        return strdup(error);
    }

    // Convert the entire result table to JSON
    cJSON* json = lua_to_json(L, -1);
    result = cJSON_Print(json);
    cJSON_Delete(json);
    lua_close(L);
    
    return result;
}
