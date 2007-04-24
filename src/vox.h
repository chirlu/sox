/*
 * SOX file format handler for Dialogic/Oki ADPCM VOX files.
 *
 * Copyright 1991-2007 Tony Seebregts And Sundry Contributors
 *
 * This source code is freely redistributable and may be used for any
 * purpose.  This copyright notice must be maintained.
 *
 * Tony Seebregts And Sundry Contributors are not responsible for the
 * consequences of using this software.
 */

int sox_vox_start(ft_t ft);
int sox_ima_start(ft_t ft);
sox_size_t sox_vox_read(ft_t ft, sox_ssample_t *buffer, sox_size_t len);
int sox_vox_stopread(ft_t ft);
sox_size_t sox_vox_write(ft_t ft, const sox_ssample_t *buffer, sox_size_t length);
int sox_vox_stopwrite(ft_t ft);
