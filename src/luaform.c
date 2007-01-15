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

 
/* TODO: If efficiency is a problem, move the call of the Lua script
   into the read/write phase. Instrument the Lua environment so that
   scripts can still be written naively: reading beyond the end of the
   input array yields to read more data, and writing output similarly.
   In order not to need nonetheless to buffer all input and output
   until finished, need low-water-marks that the script can update to
   signal that it has finished reading and writing respectively.
   Alternatively, assume that each location can only be read/written
   once. */


#include "st_i.h"

#include <string.h>
#include <lua.h>
#include <lauxlib.h>

/* Private data */
typedef struct luafile {
  lua_State *L;                 /* Lua state */
} *lua_t;

assert_static(sizeof(struct luafile) <= ST_MAX_FILE_PRIVSIZE, 
              /* else */ skel_PRIVSIZE_too_big);

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
  }
  /* FIXME: Store script function for reuse! */

  return ST_SUCCESS;
}

/*
 * Call Lua script with (action, fp, array), and return return value
 * (an st_size_t) to the caller.
 */
static st_size_t lua_callscript(lua_State *L)
{
  st_size_t done;
  int ret;

  if ((ret = lua_pcall(L, 3, 1, 0)) != 0)
    st_fail("error in Lua script: %d", ret);
  done = lua_tointeger(L, -1);
  lua_pop(L, 1);

  return done;
}

/*
 * Read up to len samples of type st_sample_t from file into buf[].
 * Return number of samples read.
 */
static st_size_t lua_read(ft_t ft, st_sample_t *buf, st_size_t len)
{
  lua_t lua = (lua_t)ft->priv;
  st_sample_t_array_t inarr;

  inarr.size = len;
  inarr.data = buf;

  lua_pushstring(lua->L, "read");
  st_lua_pushfile(lua->L, ft->fp);
  st_lua_pusharray(lua->L, inarr);

  return lua_callscript(lua->L);
}

/*
 * Write len samples of type st_sample_t from buf[] to file.
 * Return number of samples written.
 */
static st_size_t lua_write(ft_t ft, const st_sample_t *buf, st_size_t len)
{
  lua_t lua = (lua_t)ft->priv;
  st_sample_t_array_t outarr;

  outarr.size = len;
  outarr.data = (st_sample_t *)buf;

  lua_pushstring(lua->L, "write");
  st_lua_pushfile(lua->L, ft->fp);
  st_lua_pusharray(lua->L, outarr);

  return lua_callscript(lua->L);
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

  lua_pushstring(lua->L, "seek");
  st_lua_pushfile(lua->L, ft->fp);
  lua_pushinteger(lua->L, offset);

  return lua_callscript(lua->L);
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
