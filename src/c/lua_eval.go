//go:build linux && amd64

package luaeval

/*
#cgo CFLAGS: -I${SRCDIR}/build/include
#cgo LDFLAGS: -L${SRCDIR}/build/lib -l:liblua_eval.a -ldl -lm
#include <stdlib.h>
#include "lua_eval.h"
*/
import "C"
import (
	"encoding/json"
	"fmt"
	"unsafe"
)

// SandboxConfig holds configuration for the Lua sandbox
type SandboxConfig struct {
	MemoryLimit int // Maximum memory in bytes
	CPUTimeout  int // Maximum execution time in milliseconds
}

// DefaultConfig returns a default sandbox configuration
func DefaultConfig() SandboxConfig {
	return SandboxConfig{
		MemoryLimit: 50 * 1024 * 1024, // 50MB
		CPUTimeout:  5000,             // 5 seconds
	}
}

// EvalResult contains the output of a Lua evaluation
type EvalResult struct {
	Output      string
	DebugOutput string
}

// Error represents a Lua evaluation error
type Error struct {
	Message     string
	DebugOutput string
}

func (e *Error) Error() string {
	return fmt.Sprintf("lua evaluation error: %s", e.Message)
}

// Eval evaluates a Lua expression with default settings and returns the string result
func Eval(code string) (string, error) {
	result, err := EvalWithConfig(code, DefaultConfig())
	if err != nil {
		return "", err
	}
	return result.Output, nil
}

// EvalWithConfig evaluates a Lua expression with the given configuration
func EvalWithConfig(code string, config SandboxConfig) (*EvalResult, error) {
	cCode := C.CString(code)
	defer C.free(unsafe.Pointer(cCode))

	// Create config structure
	cConfig := C.lua_sandbox_config{}
	cConfig.memory_limit = C.size_t(config.MemoryLimit)
	cConfig.cpu_timeout = C.int(config.CPUTimeout)

	// Call C function
	cResult := C.lua_eval_string_sandbox(cCode, cConfig)
	defer C.lua_free_result(cResult)

	// Convert result to Go structures
	goResult := &EvalResult{
		Output:      C.GoString(cResult.output),
		DebugOutput: C.GoString(cResult.debug_output),
	}

	// Check for evaluation errors
	if cResult.status == C.LUA_EVAL_ERROR {
		return nil, &Error{
			Message:     goResult.Output,
			DebugOutput: goResult.DebugOutput,
		}
	}

	return goResult, nil
}

// EvalToJSON evaluates Lua code and attempts to parse the result as JSON
func EvalToJSON(code string, v interface{}) error {
	result, err := EvalWithConfig(code, DefaultConfig())
	if err != nil {
		return err
	}

	// Parse JSON output
	return json.Unmarshal([]byte(result.Output), v)
}