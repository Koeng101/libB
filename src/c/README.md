# c

This is a work-in-progress C that binds luajit execution environment to something that can be linked to by other projects. It uses musl to statically link luajit to a C shared library containing the `lua_eval` code, as well as the server for a production environment.

```
luajit -b dnadesign.lua dnadesign.h
```
