//go:build linux && amd64

package luaeval

import "testing"

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
