/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools skeleton file format driver.
 */

#include "st_i.h"

/* Private data for SKEL file */
typedef struct skelstuff 
{
    st_size_t samples_remaining;
} *skel_t;

/*
 * Do anything required before you start reading samples.
 * Read file header. 
 *      Find out sampling rate, 
 *      size and encoding of samples, 
 *      mono/stereo/quad.
 */
int st_skelstartread(ft_t ft) 
{
    skel_t sk = (skel_t) ft->priv;

    /* If you need to seek around the input file. */
    if (!ft->seekable)
    {
        st_fail_errno(ft,ST_EVALUE,"SKEL input file must be a file, not a pipe");
        return (ST_EOF);
    }

    /*
     * If your format is headerless and has fixed values for
     * the following items, you can hard code them here (see cdr.c).
     * If your format contains a header with format information
     * then you should set it here.
     */
    ft->info.rate =  44100L;
    ft->info.size = ST_SIZE_BYTE or WORD ...;
    ft->info.encoding = ST_ENCODING_UNSIGNED or SIGN2 ...;
    ft->info.channels = 1 or 2 or 4;
    ft->comment = malloc(size_of_comment);
    strcpy(ft->comment, "any comment in file header.");

    /* If your format doesn't have a header then samples_in_file
     * can be determined by the file size.
     */
    samples_in_file = st_filelength(ft)/ft->info.size;

    /* If you can detect the length of your file, record it here. */
    ft->length = samples_in_file;
    sk->remaining_samples = samples_in_file;

    return (ST_SUCCESS);
}

/*
 * Read up to len samples from file.
 * Convert to st_sample_t.
 * Place in buf[].
 * Return number of samples read.
 */
st_ssize_t st_skelread(ft_t ft, st_sample_t *buf, st_ssize_t len) 
{
    skel_t sk = (skel_t)ft->priv;
    int done = 0;
    st_sample_t l;

    /* Always return a full frame of audio data */
    if (len % ft->info.size)
        len -= (len % ft->info.size);

    for(; done < len; done++) {
        if no more samples
            break
        get a sample
        switch (ft->info.size)
        {
            case ST_SIZE_BYTE:
                switch (ft->info.encoding)
                {
                    case ST_ENCODING_UNSIGNED;
                        *buf++ = ST_UNSIGNED_BYTE_TO_SAMPLE(sample);
                        break;
                }
                break;
        }
    }

    if (done == 0)
        return ST_EOF;
    else
        return done;
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
int skelstopread(ft_t ft) 
{
    return (ST_SUCCESS);
}

int st_skelstartwrite(ft_t ft) 
{
    skel_t sk = (skel_t) ft->priv;

    /* If you have to seek around the output file. */
    /* If header contains a length value then seeking will be
     * required.  Instead of failing, its sometimes nice to
     * just set the length to max value and not fail.
     */
    if (!ft->seekable) 
    {
        st_fail_errno(ft, ST_EVALUE, "Output .skel file must be a file, not a pipe");
        return (ST_EOF);
    }

    if (ft->info.rate != 44100L)
    {
        st_fail_errno(ft, ST_EVALUE, "Output. skel file must have a sample rate of 44100");
    }

    if (ft->info.size == -1)
    {
        st_fail_errno(ft, ST_EVALUE, "Did not specify a size for .skel output file");
        return (ST_EOF);
    }
    error check ft->info.encoding;
    error check ft->info.channels;

    /* Write file header, if any */
    /* Write comment field, if any */

    return(ST_SUCCESS);

}

st_ssize_t st_skelwrite(ft_t ft, st_sample_t *buf, st_ssize_t len) 
{
    skel_t sk = (skel_t) ft->priv;
    st_ssize_t len = 0;

    switch (ft->info.size)
    {
        case ST_SIZE_BYTE:
            switch (ft->info.encoding)
            {
                case ST_ENCODING_UNSIGNED:
                    while(len--)
                    {
                        len = st_writeb(ft, ST_SAMPLE_TO_UNSIGNED_BYTE(*buff++));
                        if (len == ST_EOF)
                            break;
                    }
                    break;
            }
            break;
    }

    if (len == ST_EOF)
        return ST_EOF;
    else
        return ST_SUCCESS;

}

int st_skelstopwrite(ft_t ft) 
{
    /* All samples are already written out. */
    /* If file header needs fixing up, for example it needs the */
    /* the number of samples in a field, seek back and write them here. */
    return (ST_SUCCESS);
}

