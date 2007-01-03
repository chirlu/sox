/* Lua integer types library
 *
 * Copyright 2006 Reuben Thomas
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, write to the Free Software
 * Foundation, Fifth Floor, 51 Franklin Street, Boston, MA 02111-1301,
 * USA.  */


#include "st_i.h"

#include <lua.h>
#include <lauxlib.h>
#include <luaconf.h>

#include <assert.h>
#ifndef _MSC_VER
#include <inttypes.h>
#endif


/* Library configuration */
#define LIBNAME int
typedef st_sample_t Integer;
typedef st_usample_t UInteger;


/* Auxiliary functions and macros */

#define PASTE(x, y) x ## y
#define PREFIX(x) PASTE(LIBNAME, x)

#define STRINGIFY(x) #x
#define STRING(x) STRINGIFY(x)

static const char *handle = "st_size_t";

#define checkudata(L, ty, n) \
  ((ty *)luaL_checkudata(L, n, handle))

static int newint(lua_State *L, Integer val)
{
  lua_newuserdata(L, sizeof(Integer));
  *(Integer *)lua_touserdata(L, -1) = val;
  luaL_getmetatable(L, handle);
  lua_setmetatable(L, -2);
  return 1;
}

static int new(lua_State *L)
{
  return newint(L, (Integer)(lua_tonumber(L, -1)));
}


/* Macros for operations */

#define MONADIC(name, op, ty) \
  static int name(lua_State *L) \
  { \
    Integer res = op *checkudata(L, ty, 1); \
    newint(L, res); \
    return 1; \
  }

#define DYADIC(name, op, ty1, ty2) \
  static int name(lua_State *L) \
  { \
    Integer res = *checkudata(L, ty1, 1) op *checkudata(L, ty2, 2); \
    newint(L, res); \
    return 1; \
  }

#define VARIADIC(name, op, ty) \
  static int name(lua_State *L) \
  { \
    int n = lua_gettop(L), i; \
    Integer res = *checkudata(L, ty, 1); \
    for (i = 2; i <= n; i++) \
      res op *checkudata(L, ty, i); \
    newint(L, res); \
    return 1; \
  }


/* Declare operations */
MONADIC(PREFIX(_unm), -, Integer)
DYADIC(PREFIX(_add), +, Integer, Integer)
DYADIC(PREFIX(_sub), -, Integer, Integer)
DYADIC(PREFIX(_mul), *, Integer, Integer)
DYADIC(PREFIX(_div), /, Integer, Integer)
DYADIC(PREFIX(_mod), %, Integer, Integer)
MONADIC(PREFIX(_bnot),    ~,  Integer)
VARIADIC(PREFIX(_band),   &=, Integer)
VARIADIC(PREFIX(_bor),    |=, Integer)
VARIADIC(PREFIX(_bxor),   ^=, Integer)
DYADIC(PREFIX(_lshift),  <<, Integer, UInteger)
DYADIC(PREFIX(_rshift),  >>, UInteger, UInteger)
DYADIC(PREFIX(_arshift), >>, Integer, UInteger)


static int tonumber(lua_State *L)
{
  lua_pushnumber(L, (lua_Number)(*(Integer *)checkudata(L, Integer, 1)));
  return 1;
}

static int tostring(lua_State *L)
{
  assert(tonumber(L) == 1);
  lua_tostring(L, -1);
  return 1;
}

/* Metatable */
static const luaL_reg meta[] = {
  {"__unm",      PREFIX(_unm)},
  {"__add",      PREFIX(_add)},
  {"__sub",      PREFIX(_sub)},
  {"__mul",      PREFIX(_mul)},
  {"__div",      PREFIX(_div)},
  {"__mod",      PREFIX(_mod)},
  {"__tostring", tostring},
  {"__tonumber", tonumber},
  {NULL, NULL}
};

/* Functions table */
static const struct luaL_reg funcs[] = {
  {"new",        new},
  {"bnot",       PREFIX(_bnot)},
  {"band",       PREFIX(_band)},
  {"bor",        PREFIX(_bor)},
  {"bxor",       PREFIX(_bxor)},
  {"lshift",     PREFIX(_lshift)},
  {"rshift",     PREFIX(_rshift)},
  {"arshift",    PREFIX(_arshift)},
  {NULL, NULL}
};

void createmeta(lua_State *L, const char *name)
{
  luaL_newmetatable(L, name);   /* create new metatable */
  lua_pushliteral(L, "__index");
  lua_pushvalue(L, -2);         /* push metatable */
  lua_rawset(L, -3); /* metatable.__index = metatable, for OO-style use */
}

/* Library entry point */
LUALIB_API int luaopen_int (lua_State *L) {
  createmeta(L, handle);
  luaL_register(L, NULL, meta);
  lua_pop(L, 1);
  luaL_register(L, STRING(LIBNAME), funcs);
  return 1;
}
