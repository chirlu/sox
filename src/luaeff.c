/*
 * luaeff - write effects in Lua.
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
   into the flow phase. Instrument the Lua environment so that scripts
   can still be written naively: reading beyond the end of the input
   array yields to read more data, and writing output similarly. In
   order not to need nonetheless to buffer all input and output until
   finished, need low-water-marks that the script can update to signal
   that it has finished reading and writing respectively.
   Alternatively, assume that each location can only be read/written
   once. */

 
#include "st_i.h"

#include <string.h>
#include <lua.h>
#include <lauxlib.h>

/* Private data for effect */
typedef struct luaeff {
  char *script;                   /* Script filename */
  lua_State *L;                 /* Lua state */
  st_size_t nsamp;              /* Number of samples in input */
  st_sample_t *data;            /* Input data */
} *luaeff_t;

assert_static(sizeof(struct luaeff) <= ST_MAX_EFFECT_PRIVSIZE, 
              /* else */ lua_PRIVSIZE_too_big);

/*
 * Process command-line options
 */
static int lua_getopts(eff_t effp, int n, char **argv) 
{
  luaeff_t lua = (luaeff_t)effp->priv;
  int i, ret;

  if (n < 1) {
    st_fail(effp->h->usage);
    return ST_EOF;
  }

  lua->L = st_lua_new();
  
  /* Collect options into global arg table */
  lua_createtable(lua->L, n - 1, 0);
  for (i = 1; i < n; i++) {
    lua_pushstring(lua->L, argv[i]);
    lua_rawseti(lua->L, -2, i);
  }
  lua_setglobal(lua->L, "arg");

  lua->script = xstrdup(argv[0]);

  if ((ret = luaL_loadfile(lua->L, lua->script)) != 0) {
    st_fail("cannot load Lua script %s: error %d", lua->script, ret);
    return ST_EOF;
  }
  return ST_SUCCESS;
}


/*
 * Gather samples.
 */
static int lua_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf UNUSED, 
                       st_size_t *isamp, st_size_t *osamp)
{
  luaeff_t lua = (luaeff_t)effp->priv;

  lua->data = (st_sample_t *)xrealloc(lua->data, (lua->nsamp + *isamp) * sizeof(st_sample_t));
  memcpy(lua->data + lua->nsamp, ibuf, *isamp * sizeof(st_sample_t));
  lua->nsamp += *isamp;

  *osamp = 0;           /* Signal that we didn't produce any output */

  return ST_SUCCESS;
}

/*
 * Send samples to script.
 */
static int lua_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
  luaeff_t lua = (luaeff_t)effp->priv;
  int ret;
  st_sample_t_array_t inarr, outarr;

  inarr.size = lua->nsamp;
  inarr.data = lua->data;
  outarr.size = *osamp;
  outarr.data = obuf;

  st_lua_newarr(lua->L, inarr);
  st_lua_newarr(lua->L, outarr);
  if ((ret = lua_pcall(lua->L, 2, 0, 0)) != 0)
    st_fail("error in Lua script: %d", ret);

  *osamp = 0;
  return ST_EOF;
}

/*
 * Free sample data.
 */
static int lua_stop(eff_t effp)
{
  luaeff_t lua = (luaeff_t)effp->priv;

  free(lua->data);
  lua->data = NULL;

  return ST_SUCCESS;
}

/*
 * Clean up state.
 */
static int lua_delete(eff_t effp)
{
  luaeff_t lua = (luaeff_t)effp->priv;

  lua_close(lua->L);

  return ST_SUCCESS;
}


/*
 * Effect descriptor.
 */
static st_effect_t st_lua_effect = {
  "lua",
  "Usage: lua script [options]",
  ST_EFF_MCHAN,
  lua_getopts,
  st_effect_nothing,
  lua_flow,
  lua_drain,
  lua_stop,
  lua_delete
};

/*
 * Function returning effect descriptor. This should be the only
 * externally visible object.
 */
const st_effect_t *st_lua_effect_fn(void)
{
  return &st_lua_effect;
}
