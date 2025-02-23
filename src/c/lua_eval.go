//go:build linux && amd64

package luaeval

/*
#cgo CFLAGS: -I${SRCDIR}/build/include
#cgo LDFLAGS: -L${SRCDIR}/build/lib -l:liblua_eval.a -ldl -lm
#include <stdlib.h>
#include "lua_eval.h"

// Embedded header
char* lua_eval_string(const char* input);
*/
import "C"
import (
	"fmt"
	"unsafe"
)

// Error represents a Lua evaluation error
type Error struct {
	Message string
}

func (e *Error) Error() string {
	return fmt.Sprintf("lua evaluation error: %s", e.Message)
}

// Eval evaluates a Lua expression and returns the result
func Eval(code string) (string, error) {
	cCode := C.CString(code)
	defer C.free(unsafe.Pointer(cCode))

	result := C.lua_eval_string(cCode)
	goResult := C.GoString(result)

	// Check if result indicates an error
	if len(goResult) > 6 && goResult[:6] == "Error:" {
		return "", &Error{Message: goResult[7:]}
	}

	return goResult, nil
}
