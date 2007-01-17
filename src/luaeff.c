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

 
/* TODO: To increase speed, move the call of the Lua script into the
   flow phase. Instrument the Lua environment so that scripts can
   still be written naively: have script called as a coroutine, and
   make reading beyond the end of the input array yields to read more
   data, and writing output similarly. In order not to need
   nonetheless to buffer all input and output until finished, add
   low-water-marks that the script can update to signal that it has
   finished reading and writing respectively. */


#include "st_i.h"

#include <string.h>
#include <lua.h>
#include <lauxlib.h>

/* Private data for effect */
typedef struct luaeff {
  lua_State *L;                 /* Lua state */
  char *script;                 /* Script filename */
  bool gotdata;                 /* Script has been run */
  st_size_t isamp;              /* Number of samples in input */
  st_size_t osamp;              /* Number of samples in output */
  st_size_t ostart;             /* Next sample to output */
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

  lua->data = (st_sample_t *)xrealloc(lua->data, (lua->isamp + *isamp) * sizeof(st_sample_t));
  memcpy(lua->data + lua->isamp, ibuf, *isamp * sizeof(st_sample_t));
  lua->isamp += *isamp;

  *osamp = 0;           /* Signal that we didn't produce any output */

  return ST_SUCCESS;
}

/*
 * Send samples to script.
 */
static int lua_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
  luaeff_t lua = (luaeff_t)effp->priv;
  int ret, i;
  st_sample_t_array_t inarr;

  if (!lua->gotdata) {
    /* Function is left on stack by getopts */

    inarr.size = lua->isamp;
    inarr.data = lua->data;
    st_lua_pusharray(lua->L, inarr);

    if ((ret = lua_pcall(lua->L, 1, 1, 0)) != 0) {
      st_fail("error in Lua script: %d", ret);
      return ST_EOF;
    } else if (lua_type(lua->L, -1) != LUA_TTABLE) {
    st_fail("Lua script did not return an array");
    return ST_EOF;
    }
    lua->gotdata = true;
    lua->osamp = lua_objlen(lua->L, -1);
    lua->ostart = 0;
  }

  *osamp = min(*osamp, lua->osamp);
  if (*osamp > INT_MAX) {
    st_fail("output buffer size %d too large for Lua", *osamp);
    return ST_EOF;
  }
  /* Read output: Lua array is 1-based */
  for (i = 1; i <= (int)*osamp; i++) {
    lua_rawgeti(lua->L, -1, i);
    obuf[i] = lua_tointeger(lua->L, -1);
    lua_pop(lua->L, 1);
  }
  lua->osamp -= *osamp;
  lua->ostart += *osamp;

  if (lua->osamp > 0)
    return ST_SUCCESS;
  else
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
  "Usage: lua lua-script [option...]",
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
