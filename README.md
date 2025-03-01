# libB

libB is a lua library (coded in teal) for bioautomation.

## Why lua

Lua was chosen for its scripting ability, its ability to be embedded in nearly any language, and for its ease of sandboxing. libB is nearly dependency free because the intention is to embed it within larger intelligent systems with sandboxes, enabling those systems to reason about biology through code.

libB has full control over the entire library stack, which makes it easier to reason about in its whole.

# Misc notes

notes:

1. seqhash and pcr have failures be strings. Should make this more lua ergonomic
2. I need to mini-ify sequences to make it more LLM friendly. for example, pcr spec
