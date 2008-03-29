/* libSoX file format handler for Dialogic/Oki ADPCM VOX files.
 *
 * Copyright 1991-2007 Tony Seebregts And Sundry Contributors
 *
 * This source code is freely redistributable and may be used for any
 * purpose.  This copyright notice must be maintained.
 *
 * Tony Seebregts And Sundry Contributors are not responsible for the
 * consequences of using this software.
 */

#include "sox_i.h"
#include "vox.h"
#include "adpcms.h"

/* .vox doesn't need any private state over and above adpcm_io_t *, so
   just have simple wrappers that pass it on directly. */

int sox_vox_start(sox_format_t * ft)
{
  return sox_adpcm_oki_start(ft, (adpcm_io_t *)ft->priv);
}

int sox_ima_start(sox_format_t * ft)
{
  return sox_adpcm_ima_start(ft, (adpcm_io_t *)ft->priv);
}

sox_size_t sox_vox_read(sox_format_t * ft, sox_sample_t *buffer, sox_size_t len)
{
  return sox_adpcm_read(ft, (adpcm_io_t *)ft->priv, buffer, len);
}

int sox_vox_stopread(sox_format_t * ft)
{
  return sox_adpcm_stopread(ft, (adpcm_io_t *)ft->priv);
}

sox_size_t sox_vox_write(sox_format_t * ft, const sox_sample_t *buffer, sox_size_t length)
{
  return sox_adpcm_write(ft, (adpcm_io_t *)ft->priv, buffer, length);
}

int sox_vox_stopwrite(sox_format_t * ft)
{
  return sox_adpcm_stopwrite(ft, (adpcm_io_t *)ft->priv);
}
