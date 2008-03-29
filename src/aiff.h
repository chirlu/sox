/* libSoX SGI/Amiga AIFF format.
 * Copyright 1991-2007 Guido van Rossum And Sundry Contributors
 *
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Guido van Rossum And Sundry Contributors are not responsible for
 * the consequences of using this software.
 *
 * Used by SGI on 4D/35 and Indigo.
 * This is a subformat of the EA-IFF-85 format.
 * This is related to the IFF format used by the Amiga.
 * But, apparently, not the same.
 * Also AIFF-C format output that is defined in DAVIC 1.4 Part 9 Annex B
 * (usable for japanese-data-broadcasting, specified by ARIB STD-B24.)
 */

typedef struct {
    sox_size_t nsamples;  /* number of 1-channel samples read or written */
                         /* Decrements for read increments for write */
    sox_size_t dataStart; /* need to for seeking */
} aiff_priv_t;

int sox_aiffseek(sox_format_t * ft, sox_size_t offset);
int sox_aiffstartread(sox_format_t * ft);
sox_size_t sox_aiffread(sox_format_t * ft, sox_sample_t *buf, sox_size_t len);
int sox_aiffstopread(sox_format_t * ft);
int sox_aiffstartwrite(sox_format_t * ft);
sox_size_t sox_aiffwrite(sox_format_t * ft, const sox_sample_t *buf, sox_size_t len);
int sox_aiffstopwrite(sox_format_t * ft);
int sox_aifcstartwrite(sox_format_t * ft);
int sox_aifcstopwrite(sox_format_t * ft);
