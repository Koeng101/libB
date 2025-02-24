//go:build linux && amd64

package luaeval

import (
	"reflect"
	"strings"
	"testing"
)

func TestEval(t *testing.T) {
	tests := []struct {
		name    string
		input   string
		want    string
		wantErr bool
	}{
		{
			name:  "basic arithmetic",
			input: "return 2 + 2",
			want:  "4",
		},
		{
			name:  "string manipulation",
			input: "return string.upper('hello')",
			want:  "HELLO",
		},
		{
			name:    "syntax error",
			input:   "invalid lua code",
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := Eval(tt.input)
			if (err != nil) != tt.wantErr {
				t.Errorf("Eval() error = %v, wantErr %v", err, tt.wantErr)
				return
			}
			if !tt.wantErr && got != tt.want {
				t.Errorf("Eval() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestEvalWithConfig(t *testing.T) {
	tests := []struct {
		name    string
		input   string
		config  SandboxConfig
		want    *EvalResult
		wantErr bool
	}{
		{
			name:  "with debug output",
			input: "print('debug line')\nreturn 42",
			config: SandboxConfig{
				MemoryLimit: 1024 * 1024,
				CPUTimeout:  1000,
			},
			want: &EvalResult{
				Output:      "42",
				DebugOutput: "debug line\n",
			},
		},
		{
			name:  "memory limit",
			input: "local t = {}\nfor i=1,1000 do t[i] = string.rep('x', 1000) end\nreturn #t",
			config: SandboxConfig{
				MemoryLimit: 1024, // Very small limit
				CPUTimeout:  1000,
			},
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := EvalWithConfig(tt.input, tt.config)
			if (err != nil) != tt.wantErr {
				t.Errorf("EvalWithConfig() error = %v, wantErr %v", err, tt.wantErr)
				return
			}
			if !tt.wantErr {
				if got.Output != tt.want.Output {
					t.Errorf("Output = %v, want %v", got.Output, tt.want.Output)
				}
				if !strings.Contains(got.DebugOutput, tt.want.DebugOutput) {
					t.Errorf("DebugOutput = %v, want to contain %v", got.DebugOutput, tt.want.DebugOutput)
				}
			}
		})
	}
}

func TestEvalToJSON(t *testing.T) {
	type TestStruct struct {
		A int    `json:"a"`
		B string `json:"b"`
	}

	tests := []struct {
		name    string
		input   string
		want    TestStruct
		wantErr bool
	}{
		{
			name:  "simple table to struct",
			input: "return {a=123, b='hello world'}",
			want:  TestStruct{A: 123, B: "hello world"},
		},
		{
			name:    "invalid json",
			input:   "return function() end",
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			var got TestStruct
			err := EvalToJSON(tt.input, &got)
			
			if (err != nil) != tt.wantErr {
				t.Errorf("EvalToJSON() error = %v, wantErr %v", err, tt.wantErr)
				return
			}
			
			if !tt.wantErr && !reflect.DeepEqual(got, tt.want) {
				t.Errorf("EvalToJSON() = %v, want %v", got, tt.want)
			}
		})
	}
}