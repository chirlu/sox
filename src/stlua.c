/*
 * luast - miscellaneous Lua support functions.
 *
 * Copyright 2006-2007 Reuben Thomas
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

#include <assert.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/* st_sample_t arrays */

static const char *handle = "st_sample_t array";

int st_lua_newarr(lua_State *L, st_sample_t_array_t arr)
{
  lua_newuserdata(L, sizeof(st_sample_t_array_t));
  *(st_sample_t_array_t *)lua_touserdata(L, -1) = arr;
  luaL_getmetatable(L, handle);
  lua_setmetatable(L, -2);
  return 1;
}

static int arr_index(lua_State *L)
  /* array, key -> value */
{
  st_sample_t_array_t *p = luaL_checkudata(L, 1, handle);
  lua_Integer k = luaL_checkinteger(L, 2);

  if ((st_size_t)k >= p->size)
    lua_pushnil(L);
  else
    lua_pushinteger(L, (lua_Integer)p->data[k]);
    
  return 1;
}

static int arr_newindex(lua_State *L)
  /* array, key, value -> */
{
  st_sample_t_array_t *p = luaL_checkudata(L, 1, handle);
  lua_Integer k = luaL_checkinteger(L, 2);
  lua_Integer v = luaL_checkinteger(L, 3);

  /* FIXME: Have some indication for out of range */
  if ((st_size_t)k < p->size)
    p->data[k] = v;
    
  return 0;
}

static int arr_len(lua_State *L)
  /* array -> #array */
{
  st_sample_t_array_t *p;
  p = luaL_checkudata(L, 1, handle);
  lua_pushinteger(L, (lua_Integer)p->size);
  return 1;
}

static int arr_tostring(lua_State *L)
{
  char buf[256];
  void *udata = luaL_checkudata(L, 1, handle);
  if(udata) {
    sprintf(buf, "%s (%p)", handle, udata);
    lua_pushstring(L, buf);
  }
  else {
    sprintf(buf, "must be userdata of type '%s'", handle);
    luaL_argerror(L, 1, buf);
  }
  return 1;
}

/* Metatable */
static const luaL_reg meta[] = {
  {"__index", arr_index},
  {"__newindex", arr_newindex},
  {"__len", arr_len},
  {"__tostring", arr_tostring},
  {NULL, NULL}
};

/* Allocator function for use by Lua */
static void *lua_alloc(void *ud UNUSED, void *ptr, size_t osize UNUSED, size_t nsize)
{
  if (nsize == 0) {
    free(ptr);
    return NULL;
  } else
    return xrealloc(ptr, nsize);
}

void *st_lua_new(void)
{
  lua_State *L;

  /* Since the allocator quits if it fails, this should always
     succeed if it returns. */
  assert((L = lua_newstate(lua_alloc, NULL)));

  /* TODO: If concerned about security, lock down here: in particular,
     don't open the io library. */
  luaL_openlibs(L);

  luaopen_int(L);

  /* Create st_sample_t array userdata type */
  createmeta(L, handle);
  luaL_register(L, NULL, meta);
}
