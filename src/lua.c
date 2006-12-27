/*
 * Lua - write filters in Lua.
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

 
/* TODO: If efficiency is a problem, move the call of the Lua script
   into the flow phase. Instrument the Lua environment so that scripts
   can still be written naively: reading beyond the end of the input
   array yields to read more data, and writing output similarly. In
   order not to need nonetheless to buffer all input and output until
   finished, need low-water-marks that the script can update to signal
   that it has finished reading and writing respectively.
   Alternatively, assume that each location can only be read/written
   once. */

 
#include "st_i.h"

#include <assert.h>
#include <string.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/* Private data for effect */
typedef struct lua {
  char *file;                   /* Script file */
  lua_State *L;                 /* Lua state */
  st_size_t nsamp;              /* Number of samples in input */
  st_sample_t *data;            /* Input data */
} *lua_t;

assert_static(sizeof(struct lua) <= ST_MAX_EFFECT_PRIVSIZE, 
              /* else */ lua_PRIVSIZE_too_big);


/*
 * Process command-line options
 */
static int st_lua_getopts(eff_t effp, int n, char **argv) 
{
  lua_t lua = (lua_t)effp->priv;
  int i;

  if (n < 1) {
    st_fail(effp->h->usage);
    return ST_EOF;
  }

  /* Collect options into global arg table */
  lua_createtable(lua->L, n - 1, 0);
  for (i = 1; i < n; i++) {
    lua_pushstring(lua->L, argv[i]);
    lua_rawseti(lua->L, -2, i);
  }
  lua_setglobal(lua->L, "arg");

  lua->file = xstrdup(argv[0]);
  return ST_SUCCESS;
}

static void *lua_alloc(void *ud UNUSED, void *ptr, size_t osize UNUSED, size_t nsize)
{
  if (nsize == 0) {
    free(ptr);
    return NULL;
  } else
    return xrealloc(ptr, nsize);
}


/* st_sample_t arrays */

typedef struct {
  st_size_t size;
  st_sample_t *data;
} st_sample_t_array_t;

static const char *handle = "st_sample_t array";

static int newarr(lua_State *L, st_sample_t_array_t arr)
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


/*
 * Prepare processing.
 * Do all initializations.
 */
static int st_lua_start(eff_t effp)
{
  lua_t lua = (lua_t)effp->priv;
  int ret;

  lua->data = NULL;

  /* Since the allocator quits if it fails, this should always
     succeed if it returns. */
  assert((lua->L = lua_newstate(lua_alloc, NULL)));

  /* TODO: If concerned about security, lock down here: in particular,
     don't open the io library. */
  luaL_openlibs(lua->L);

  luaopen_int(lua->L);

  /* Create st_sample_t array userdata type */
  createmeta(lua->L, handle);
  luaL_register(lua->L, NULL, meta);

  if ((ret = luaL_loadfile(lua->L, lua->file)) != 0) {
    st_fail("cannot load Lua script %s: error %d", lua->file, ret);
    return ST_EOF;
  }

  return ST_SUCCESS;
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
static int st_lua_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf UNUSED, 
                       st_size_t *isamp, st_size_t *osamp)
{
  lua_t lua = (lua_t)effp->priv;

  lua->data = (st_sample_t *)xrealloc(lua->data, (lua->nsamp + *isamp) * sizeof(st_sample_t));
  memcpy(lua->data + lua->nsamp, ibuf, *isamp * sizeof(st_sample_t));
  lua->nsamp += *isamp;

  *osamp = 0;           /* Signal that we didn't produce any output */

  return ST_SUCCESS;
}

/*
 * Drain out remaining samples if the effect generates any.
 * If there's nothing to do, use st_effect_nothing_drain instead.
 */
static int st_lua_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
  lua_t lua = (lua_t)effp->priv;
  int ret;
  st_sample_t_array_t inarr, outarr;

  inarr.size = lua->nsamp;
  inarr.data = lua->data;
  outarr.size = *osamp;
  outarr.data = obuf;

  newarr(lua->L, inarr);
  newarr(lua->L, outarr);
  if ((ret = lua_pcall(lua->L, 2, LUA_MULTRET, 0)) != 0)
    st_fail("error in Lua script: %d", ret);

  *osamp = 0;
  /* Help out application and return ST_EOF when drain
   * will not return any mre information.  *osamp == 0
   * also indicates that.
   */
  return ST_EOF;
}

/*
 * Do anything required when you stop reading samples.  
 *      (free allocated memory, etc.)
 * If there's nothing to do, use st_effect_nothing instead.
 */
static int st_lua_stop(eff_t effp)
{
  lua_t lua = (lua_t)effp->priv;

  lua_close(lua->L);

  return ST_SUCCESS;
}


/*
 * Effect descriptor.
 * If one of the methods does nothing, use the relevant
 * st_effect_nothing* method.
 */
static st_effect_t st_lua_effect = {
  "lua",
  "Usage: lua script [options]",
  ST_EFF_MCHAN,
  st_lua_getopts,
  st_lua_start,
  st_lua_flow,
  st_lua_drain,
  st_lua_stop,
st_effect_nothing
};

/*
 * Function returning effect descriptor. This should be the only
 * externally visible object.
 */
const st_effect_t *st_lua_effect_fn(void)
{
  return &st_lua_effect;
}
