# Upstream Lua, MIT license in lua-5.4.9/doc/readme.html and src/lua.h.
file(GLOB LUA_SOURCES "lua-5.4.9/src/*.c")
list(FILTER LUA_SOURCES EXCLUDE REGEX "/(lua|luac)[.]c$")
idf_component_register(SRCS ${LUA_SOURCES} INCLUDE_DIRS "lua-5.4.9/src")
