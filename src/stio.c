#include "st_i.h"

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h> /* for fstat() */
#include <sys/stat.h> /* for fstat() */

#ifdef _MSC_VER
/* __STDC__ is defined, so these symbols aren't created. */
#define S_IFMT   _S_IFMT
#define S_IFREG  _S_IFREG
#define fstat _fstat
#endif

/* Based on zlib's minigzip: */
#if defined(WIN32) || defined(__NT__)
#include <fcntl.h>
#include <io.h>
#ifndef O_BINARY
#define O_BINARY _O_BINARY
#endif
#define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#define SET_BINARY_MODE(file)
#endif

void set_endianness_if_not_already_set(ft_t ft)
{
  if (ft->signal.reverse_bytes == ST_OPTION_DEFAULT) {
    if (ft->h->flags & ST_FILE_ENDIAN)
      ft->signal.reverse_bytes = ST_IS_LITTLEENDIAN != !(ft->h->flags & ST_FILE_ENDBIG);
    else
      ft->signal.reverse_bytes = ST_OPTION_NO;
  }
  if (ft->signal.reverse_nibbles == ST_OPTION_DEFAULT)
    ft->signal.reverse_nibbles = ST_OPTION_NO;
  if (ft->signal.reverse_bits == ST_OPTION_DEFAULT)
    ft->signal.reverse_bits = ST_OPTION_NO;
}

static int is_seekable(ft_t ft)
{
        struct stat st;

        fstat(fileno(ft->fp), &st);

        return ((st.st_mode & S_IFMT) == S_IFREG);
}

/* check that all settings have been given */
static int st_checkformat(ft_t ft)
{

        ft->st_errno = ST_SUCCESS;

        if (ft->signal.rate == 0)
        {
                st_fail_errno(ft,ST_EFMT,"sampling rate was not specified");
                return ST_EOF;
        }

        if (ft->signal.size == -1)
        {
                st_fail_errno(ft,ST_EFMT,"data size was not specified");
                return ST_EOF;
        }

        if (ft->signal.encoding == ST_ENCODING_UNKNOWN)
        {
                st_fail_errno(ft,ST_EFMT,"data encoding was not specified");
                return ST_EOF;
        }

        if ((ft->signal.size <= 0) || (ft->signal.size > ST_INFO_SIZE_MAX))
        {
                st_fail_errno(ft,ST_EFMT,"data size %d is invalid", ft->signal.size);
                return ST_EOF;
        }

        if (ft->signal.encoding <= 0  || ft->signal.encoding >= ST_ENCODINGS)
        {
                st_fail_errno(ft,ST_EFMT,"data encoding %d is invalid", ft->signal.encoding);
                return ST_EOF;
        }

        return ST_SUCCESS;
}

ft_t st_open_read(const char *path, const st_signalinfo_t *info,
                  const char *filetype)
{
    ft_t ft = (ft_t)xcalloc(sizeof(struct st_soundstream), 1);

    ft->filename = xstrdup(path);

    /* Let auto type do the work if user is not overriding. */
    if (!filetype)
        ft->filetype = xstrdup("auto");
    else
        ft->filetype = xstrdup(filetype);

    if (st_gettype(ft, st_false) != ST_SUCCESS) {
        st_warn("Unknown input file format for `%s':  %s",
                ft->filename,
                ft->st_errstr);
        goto input_error;
    }

    ft->signal.size = -1;
    ft->signal.encoding = ST_ENCODING_UNKNOWN;
    ft->signal.channels = 0;
    if (info)
        ft->signal = *info;
    ft->mode = 'r';

    if (!(ft->h->flags & ST_FILE_NOSTDIO))
    {
        /* Open file handler based on input name.  Used stdin file handler
         * if the filename is "-"
         */
        if (!strcmp(ft->filename, "-"))
        {
            SET_BINARY_MODE(stdin);
            ft->fp = stdin;
        }
        else if ((ft->fp = fopen(ft->filename, "rb")) == NULL)
        {
            st_warn("Can't open input file `%s': %s", ft->filename,
                    strerror(errno));
            goto input_error;
        }

        /* See if this file is seekable or not */
        ft->seekable = is_seekable(ft);
    }

    if (filetype)
      set_endianness_if_not_already_set(ft);

    /* Read and write starters can change their formats. */
    if ((*ft->h->startread)(ft) != ST_SUCCESS)
    {
        st_warn("Failed reading `%s': %s", ft->filename, ft->st_errstr);
        goto input_error;
    }

    /* Go a head and assume 1 channel audio if nothing is detected.
     * This is because libst usually doesn't set this for mono file
     * formats (for historical reasons).
     */
    if (ft->signal.channels == 0)
        ft->signal.channels = 1;

    if (st_checkformat(ft) )
    {
        st_warn("bad input format for file %s: %s", ft->filename,
                ft->st_errstr);
        goto input_error;
    }
    return ft;

input_error:

    free(ft->filename);
    free(ft->filetype);
    free(ft);
    return NULL;
}

#if defined(DOS) || defined(WIN32)
#define LASTCHAR '\\'
#else
#define LASTCHAR '/'
#endif

ft_t st_open_write(
    st_bool (*overwrite_permitted)(const char *filename),
    const char *path,
    const st_signalinfo_t *info,
    const char *filetype,
    const char *comment,
    const st_instrinfo_t *instr,
    const st_loopinfo_t *loops)
{
    ft_t ft = (ft_t)xcalloc(sizeof(struct st_soundstream), 1);
    int i;
    st_bool no_filetype_given = filetype == NULL;

    ft->filename = xstrdup(path);

    /* Let auto effect do the work if user is not overriding. */
    if (!filetype) {
        char *chop;
        int len;

        len = strlen(ft->filename);

        /* Use filename extension to determine audio type.
         * Search for the last '.' appearing in the filename, same
         * as for input files.
         */
        chop = ft->filename + len;
        while (chop > ft->filename && *chop != LASTCHAR && *chop != '.')
            chop--;

        if (*chop == '.') {
            chop++;
            ft->filetype = xstrdup(chop);
        }
    } else
        ft->filetype = xstrdup(filetype);

    if (!ft->filetype || st_gettype(ft, no_filetype_given) != ST_SUCCESS)
    {
        st_fail("Unknown output file format for '%s':  %s",
                ft->filename,
                ft->st_errstr);
        goto output_error;
    }

    ft->signal.size = -1;
    ft->signal.encoding = ST_ENCODING_UNKNOWN;
    ft->signal.channels = 0;
    if (info)
        ft->signal = *info;
    ft->mode = 'w';

    if (!(ft->h->flags & ST_FILE_NOSTDIO))
    {
        /* Open file handler based on output name.  Used stdout file handler
         * if the filename is "-"
         */
        if (!strcmp(ft->filename, "-"))
        {
            SET_BINARY_MODE(stdout);
            ft->fp = stdout;
        }
        else {
          struct stat st;
          if (!stat(ft->filename, &st) && (st.st_mode & S_IFMT) == S_IFREG &&
              !overwrite_permitted(ft->filename)) {
            st_fail("Permission to overwrite '%s' denied", ft->filename);
            goto output_error;
          }
          if ((ft->fp = fopen(ft->filename, "wb")) == NULL) {
            st_fail("Can't open output file '%s': %s", ft->filename,
                    strerror(errno));
            goto output_error;
          }
        }

        /* stdout tends to be line-buffered.  Override this */
        /* to be Full Buffering. */
        if (setvbuf (ft->fp, NULL, _IOFBF, sizeof(char)*ST_BUFSIZ))
        {
            st_fail("Can't set write buffer");
            goto output_error;
        }

        /* See if this file is seekable or not */
        ft->seekable = is_seekable(ft);
    }

    ft->comment = xstrdup(comment);

    if (loops)
        for (i = 0; i < ST_MAX_NLOOPS; i++)
            ft->loops[i] = loops[i];

    /* leave SMPTE # alone since it's absolute */
    if (instr)
        ft->instr = *instr;

    set_endianness_if_not_already_set(ft);

    /* Read and write starters can change their formats. */
    if ((*ft->h->startwrite)(ft) != ST_SUCCESS)
    {
        st_fail("Failed writing %s: %s", ft->filename, ft->st_errstr);
        goto output_error;
    }

    if (st_checkformat(ft) )
    {
        st_fail("bad output format for file %s: %s", ft->filename,
                ft->st_errstr);
        goto output_error;
    }

    return ft;

output_error:

    free(ft->filename);
    free(ft->filetype);
    free(ft);
    return NULL;
}

st_size_t st_read(ft_t f, st_sample_t * buf, st_size_t len)
{
  st_size_t actual = (*f->h->read)(f, buf, len);
  return (actual > len? 0 : actual);
}

st_size_t st_write(ft_t ft, const st_sample_t *buf, st_size_t len)
{
    return (*ft->h->write)(ft, buf, len);
}

/* N.B. The file (if any) may already have been deleted. */
int st_close(ft_t ft)
{
    int rc;

    if (ft->mode == 'r')
        rc = (*ft->h->stopread)(ft);
    else
        rc = (*ft->h->stopwrite)(ft);

    if (!(ft->h->flags & ST_FILE_NOSTDIO))
        fclose(ft->fp);
    free(ft->filename);
    free(ft->filetype);
    /* Currently, since startread() mallocs comments, stopread
     * is expected to also free it. */
    if (ft->mode == 'w')
        free(ft->comment);

    return rc;
}

int st_seek(ft_t ft, st_size_t offset, int whence)       
{       
    /* FIXME: Implement ST_SEEK_CUR and ST_SEEK_END. */         
    if (whence != ST_SEEK_SET)          
        return ST_EOF; /* FIXME: return ST_EINVAL */    

    /* If file is a seekable file and this handler supports seeking,    
     * the invoke handlers function.    
     */         
    if (ft->seekable  && (ft->h->flags & ST_FILE_SEEK))         
        return (*ft->h->seek)(ft, offset);      
    else        
        return ST_EOF; /* FIXME: return ST_EBADF */     
}
