/*
 * May 19, 1992
 * Copyright 1992 Guido van Rossum And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Guido van Rossum And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * A meta-handler that recognizes most file types by looking in the
 * first part of the file.  The file must be seekable!
 * (IRCAM sound files are not recognized -- these don't seem to be
 * used any more -- but this is just laziness on my part.) 
 */

#include "sox_i.h"
#include <string.h>

#if defined(DOS) || defined(WIN32)
#define LASTCHAR '\\'
#else
#define LASTCHAR '/'
#endif

static int sox_autostartread(ft_t ft)
{
    char *type = NULL;
    char header[256];
    int rc, loop;

    /* Attempt to auto-detect filetype using magic values.  Abort loop
     * and use file extension if any errors are detected.
     */
    if (ft->seekable)
    {
        /* Most files only have 4-byte magic headers at first of
         * file.  So we start checking for those filetypes first.
         */
        memset(header,0,4);
        if (sox_readbuf(ft, header, 1, 4) == 4)
        {
            /* Look for .snd or dns. header of AU files */
            if ((strncmp(header, ".snd", 4) == 0) ||
                (strncmp(header, "dns.", 4) == 0) ||
                ((header[0] == '\0') && 
                 (strncmp(header+1, "ds.", 3) == 0)) ||
                ((strncmp(header, "sd.", 3) == 0) && 
                 (header[3] == '\0')))
                type = "au";
            else if (strncmp(header, "FORM", 4) == 0) 
            {
                /* Need to read more data to see what type of FORM file */
                if (sox_readbuf(ft, header, 1, 8) == 8)
                {
                    if (strncmp(header + 4, "AIFF", 4) == 0)
                        type = "aiff";
                    else if (strncmp(header + 4, "AIFC", 4) == 0)
                        type = "aiff";
                        else if (strncmp(header + 4, "8SVX", 4) == 0)
                        type = "8svx";
                    else if (strncmp(header + 4, "MAUD", 4) == 0)
                        type = "maud";
                }
            }
            else if (strncmp(header, "RIFF", 4) == 0)
            {
                if (sox_readbuf(ft, header, 1, 8) == 8)
                    if (strncmp(header + 4, "WAVE", 4) == 0)
                        type = "wav";
            }
            else if (strncmp(header, "Crea", 4) == 0) 
            {
                if (sox_readbuf(ft, header, 1, 15) == 15)
                    if (strncmp(header, "tive Voice File", 15) == 0) 
                        type = "voc";
            }
            else if (strncmp(header, "SOUN", 4) == 0)
            {
                /* Check for SOUND magic header */
                if (sox_readbuf(ft, header, 1, 1) == 1 && *header == 'D')
                {
                    /* Once we've found SOUND see if its smp or sndt */
                    if (sox_readbuf(ft, header, 1, 12) == 12)
                    {
                        if (strncmp(header, " SAMPLE DATA", 12) == 0)
                            type = "smp";
                        else
                            type = "sndt";
                    }
                    else
                        type = "sndt";
                }
            }
            else if (strncmp(header, "2BIT", 4) == 0) 
                type = "avr";
            else if (strncmp(header, "NIST", 4) == 0) 
            {
                if (sox_readbuf(ft, header, 1, 3) == 3)
                    if (strncmp(header, "_1A", 3) == 0) 
                        type = "sph";
            }
            else if (strncmp(header, "ALaw", 4) == 0)
            {
                if (sox_readbuf(ft, header, 1, 11) == 11)
                    if (strncmp(header, "SoundFile**", 11) == 0)
                        type = "wve";
            }
            else if (strncmp(header, "Ogg", 3) == 0)
                type = "ogg";
            else if (strncmp(header, "fLaC", 4) == 0)
                type = "flac";
            else if ((memcmp(header, "XAI\0", 4) == 0) ||
                     (memcmp(header, "XAJ\0", 4) == 0) ||
                     (memcmp(header, "XA\0\0", 4) == 0))
                type = "xa";
        } /* read 4-byte header */

        /* If we didn't find type yet then start looking for file
         * formats that the magic header is deeper in the file.
         */
        if (type == NULL)
        {
            for (loop = 0; loop < 60; loop++)
                if (sox_readbuf(ft, header, 1, 1) != 1)
                    break;
            if (sox_readbuf(ft, header, 1, 4) == 4 && 
                strncmp(header, "FSSD", 4) == 0)
            {
                for (loop = 0; loop < 62; loop++)
                    if (sox_readbuf(ft, header, 1, 1) != 1)
                        break;
                if (sox_readbuf(ft, header, 1, 4) == 0 && 
                    strncmp(header, "HCOM", 4) == 0)
                    type = "hcom";
            }
        }

        sox_rewind(ft);

        if (type == NULL) {
          if (prc_checkheader(ft, header))
            type = "prc";
          sox_rewind(ft);
        }        
    } /* if (seekable) */

    if (type == NULL)
    {
        /* Use filename extension to determine audio type. */

        /* First, chop off any path portions of filename.  This
         * prevents the next search from considering that part. */
        if ((type = strrchr(ft->filename, LASTCHAR)) == NULL)
            type = ft->filename;

        /* Now look for an filename extension */
        if ((type = strrchr(type, '.')) != NULL)
            type++;
        else
            type = NULL;
    }

    if (type == NULL)
    {
        sox_fail_errno(ft,SOX_EFMT, "Could not determine file type.");
        return (SOX_EOF);
    }
    free(ft->filetype);
    ft->filetype = strdup(type);
    rc = sox_gettype(ft, sox_true); /* Change ft->h to the new format */
    if(rc != SOX_SUCCESS)
    {
        sox_fail_errno(ft,SOX_EFMT,"Do not understand format type: %s",type);
        return (rc);
    }

    sox_debug("Detected file format type: %s", type);
    set_endianness_if_not_already_set(ft);
    return (* ft->h->startread)(ft);
}

static int sox_autostartwrite(ft_t ft) 
{
        sox_fail_errno(ft,SOX_EFMT,"Type AUTO can only be used for input!");
        return(SOX_EOF);
}

static const char *autonames[] = {
  "auto",
  NULL
};

static sox_format_t sox_auto_format = {
  autonames,
  NULL,
  SOX_FILE_DEVICE | SOX_FILE_PHONY,
  sox_autostartread,
  sox_format_nothing_read_io,
  sox_format_nothing,
  sox_autostartwrite,
  sox_format_nothing_write_io,
  sox_format_nothing,
  sox_format_nothing_seek
};

const sox_format_t *sox_auto_format_fn(void)
{
    return &sox_auto_format;
}
