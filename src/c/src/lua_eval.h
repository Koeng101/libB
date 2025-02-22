#ifndef LUA_EVAL_H
#define LUA_EVAL_H

#ifdef __cplusplus
extern "C" {
#endif

// Main evaluation function that can be called from other languages
char* lua_eval_string(const char* input);

// Cleanup function to free memory if needed
void lua_eval_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // LUA_EVAL_H
