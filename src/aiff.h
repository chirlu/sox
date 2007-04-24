/*
 * libSoX SGI/Amiga AIFF format.
 * Used by SGI on 4D/35 and Indigo.
 * This is a subformat of the EA-IFF-85 format.
 * This is related to the IFF format used by the Amiga.
 * But, apparently, not the same.
 * Also AIFF-C format output that is defined in DAVIC 1.4 Part 9 Annex B
 * (usable for japanese-data-broadcasting, specified by ARIB STD-B24.)
 *
 * Copyright 1991-2007 Guido van Rossum And Sundry Contributors
 * 
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Guido van Rossum And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

int sox_aiffseek(ft_t ft, sox_size_t offset);
int sox_aiffstartread(ft_t ft);
sox_size_t sox_aiffread(ft_t ft, sox_ssample_t *buf, sox_size_t len);
int sox_aiffstopread(ft_t ft);
int sox_aiffstartwrite(ft_t ft);
sox_size_t sox_aiffwrite(ft_t ft, const sox_ssample_t *buf, sox_size_t len);
int sox_aiffstopwrite(ft_t ft);
int sox_aifcstartwrite(ft_t ft);
int sox_aifcstopwrite(ft_t ft);
