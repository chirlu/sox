/*
 * luafile - file formats in Lua.
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

#include <string.h>
#include <lua.h>
#include <lauxlib.h>

/* Private data */
typedef struct luafile {
  lua_State *L;                 /* Lua state */
  int readref;                  /* reference to read method */
  int writeref;                 /* reference to write method */
  int seekref;                  /* reference to seek method */
} *lua_t;

assert_static(sizeof(struct luafile) <= ST_MAX_FILE_PRIVSIZE, 
              /* else */ skel_PRIVSIZE_too_big);

/*
 * Get the given method, checking that it is a function
 */          
static st_bool get_method(lua_State *L, const char *lua_script, const char *meth, int *ref)
{
  int ty;

  lua_getfield(L, -1, meth);
  if ((ty = lua_type(L, -1)) == LUA_TNIL) {
    st_fail("Lua script %s does not have a %s method", lua_script, meth);
    lua_pop(L, 1);
    return st_false;
  } else if (ty != LUA_TFUNCTION) {
    st_fail("Lua script %s's %s method is not a function", lua_script, meth);
    lua_pop(L, 1);
    return st_false;
  }

  *ref = luaL_ref(L, LUA_REGISTRYINDEX);

  return st_true;
}

/*
 * Set up Lua state and read script.
 */
static int lua_start(ft_t ft)
{
  lua_t lua = (lua_t)ft->priv;
  int ret;

  lua->L = st_lua_new();

  if (!ft->signal.lua_script) {
    st_fail("no Lua script given");
    return ST_EOF;
  } else if ((ret = luaL_loadfile(lua->L, ft->signal.lua_script)) != 0) {
    st_fail("cannot load Lua script %s: error %d", ft->signal.lua_script, ret);
    return ST_EOF;
  } else if ((ret = lua_pcall(lua->L, 0, 1, 0)) != 0) {
    st_fail("cannot get methods from Lua script %s: error %d", ft->signal.lua_script, ret);
    return ST_EOF;
  } else if (lua_type(lua->L, -1) != LUA_TTABLE) {
    st_fail("Lua script %s did not return a table", ft->signal.lua_script);
    return ST_EOF;
  } else if (!get_method(lua->L, ft->signal.lua_script, "read", &lua->readref) ||
        !get_method(lua->L, ft->signal.lua_script, "write", &lua->writeref) ||
        !get_method(lua->L, ft->signal.lua_script, "seek", &lua->seekref))
    return ST_EOF;
  lua_pop(lua->L, 1);           /* Discard function */

  return ST_SUCCESS;
}

/*
 * Read up to len samples of type st_sample_t from file into buf[].
 * Return number of samples read.
 */
static st_size_t lua_read(ft_t ft, st_sample_t *buf, st_size_t len)
{
  lua_t lua = (lua_t)ft->priv;
  st_sample_t_array_t inarr;
  st_size_t done;
  int ret;

  inarr.size = len;
  inarr.data = buf;

  lua_rawgeti(lua->L, LUA_REGISTRYINDEX, lua->readref);
  st_lua_pushfile(lua->L, ft->fp);
  st_lua_pusharray(lua->L, inarr);
  if ((ret = lua_pcall(lua->L, 2, 1, 0)) != 0)
    st_fail("error in Lua script's read method: %d", ret);
  done = lua_tointeger(lua->L, -1);
  lua_pop(lua->L, 1);

  return done;
}

/*
 * Write len samples of type st_sample_t from buf[] to file.
 * Return number of samples written.
 */
static st_size_t lua_write(ft_t ft, const st_sample_t *buf, st_size_t len)
{
  lua_t lua = (lua_t)ft->priv;
  st_sample_t_array_t outarr;
  st_size_t done;
  int ret;

  outarr.size = len;
  outarr.data = (st_sample_t *)buf;

  lua_rawgeti(lua->L, LUA_REGISTRYINDEX, lua->writeref);
  st_lua_pushfile(lua->L, ft->fp);
  st_lua_pusharray(lua->L, outarr);
  if ((ret = lua_pcall(lua->L, 2, 1, 0)) != 0)
    st_fail("error in Lua script's write method: %d", ret);
  done = lua_tointeger(lua->L, -1);
  lua_pop(lua->L, 1);

  return done;
}

/*
 * Clean up state.
 */
static int lua_stop(ft_t ft)
{
  lua_t lua = (lua_t)ft->priv;

  lua_close(lua->L);

  return ST_SUCCESS;
}

/* Seek relative to current position. */
static int lua_seek(ft_t ft, st_size_t offset)
{
  lua_t lua = (lua_t)ft->priv;
  int ret;
  st_size_t done;

  lua_rawgeti(lua->L, LUA_REGISTRYINDEX, lua->seekref);
  st_lua_pushfile(lua->L, ft->fp);
  lua_pushinteger(lua->L, offset);
  if ((ret = lua_pcall(lua->L, 2, 1, 0)) != 0)
    st_fail("error in Lua script's seek method: %d", ret);
  done = lua_toboolean(lua->L, -1);
  lua_pop(lua->L, 1);

  return done;
}

/* Format file suffixes */
static const char *lua_names[] = {
  "lua",
  NULL
};

/* Format descriptor */
static st_format_t st_lua_format = {
  lua_names,
  NULL,
  0,
  lua_start,
  lua_read,
  lua_stop,
  lua_start,
  lua_write,
  lua_stop,
  lua_seek
};

/*
 * Function returning effect descriptor. This should be the only
 * externally visible object.
 */
const st_format_t *st_lua_format_fn(void)
{
  return &st_lua_format;
}
