/*
 * util.c.
 * Incorporate Jimen Ching's fixes for real library operation: Aug 3, 1994.
 * Redo all work from scratch, unfortunately.
 * Separate out all common variables used by effects & handlers,
 * and utility routines for other main programs to use.
 */

/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "st_i.h"
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

st_output_message_handler_t st_output_message_handler = NULL;
int st_output_verbosity_level = 2;

void st_output_message(FILE *file, const char *filename, const char *fmt, va_list ap)
{
  char buffer[10];
  char const * drivername;
  char const * dot_pos;
 
  drivername = strrchr(filename, '/');
  if (drivername != NULL) {
    ++drivername;
  } else {
    drivername = strrchr(filename, '\\');
    if (drivername != NULL)
      ++drivername;
    else
      drivername = filename;
  }

  dot_pos = strrchr(drivername, '.');
  if (dot_pos != NULL && dot_pos - drivername <= (ptrdiff_t)(sizeof(buffer) - 1)) {
    strncpy(buffer, drivername, (size_t)(dot_pos - drivername));
    buffer[dot_pos - drivername] = '\0';
    drivername = buffer;
  }

  fprintf(file, "%s: ", drivername);
  vfprintf(file, fmt, ap);
}



/* This is a bit of a hack.  It's useful to have the ST library
 * report which driver (i.e. format or effect handler) is outputing
 * the message.  Using the filename for this purpose is only an
 * approximation, but it saves a lot of work. ;)
 */
char const * st_message_filename = 0;



static void st_emit_message(int level, char const *fmt, va_list ap)
{
  if (st_output_message_handler != NULL)
    (*st_output_message_handler)(level, st_message_filename, fmt, ap);
}



#undef st_fail
#undef st_warn
#undef st_report
#undef st_debug
#undef st_debug_more
#undef st_debug_most

#define ST_MESSAGE_FUNCTION(name,level) \
void name(char const * fmt, ...) \
{ \
  va_list args; \
\
  va_start(args, fmt); \
  st_emit_message(level, fmt, args); \
  va_end(args); \
}

ST_MESSAGE_FUNCTION(st_fail  , 1)
ST_MESSAGE_FUNCTION(st_warn  , 2)
ST_MESSAGE_FUNCTION(st_report, 3)
ST_MESSAGE_FUNCTION(st_debug , 4)
ST_MESSAGE_FUNCTION(st_debug_more , 5)
ST_MESSAGE_FUNCTION(st_debug_most , 6)

#undef ST_MESSAGE_FUNCTION

void st_fail_errno(ft_t ft, int st_errno, const char *fmt, ...)
{
        va_list args;

        ft->st_errno = st_errno;

        va_start(args, fmt);
#ifdef _MSC_VER
        vsprintf(ft->st_errstr, fmt, args);
#else
        vsnprintf(ft->st_errstr, sizeof(ft->st_errstr), fmt, args);
#endif
        va_end(args);
        ft->st_errstr[255] = '\0';
}

/*
 * Check that we have a known format suffix string.
 */
int st_gettype(ft_t formp, st_bool is_file_extension)
{
    const char * const *list;
    int i;
    const st_format_t *f;

    if (!formp->filetype) {
        st_fail_errno(formp, ST_EFMT, "Filetype was not specified");
        return ST_EFMT;
    }
    for (i = 0; st_format_fns[i]; i++) {
        f = st_format_fns[i]();
        if (is_file_extension && (f->flags & ST_FILE_DEVICE))
          continue; /* don't match device names in real file names */
        for (list = f->names; *list; list++) {
            const char *s1 = *list, *s2 = formp->filetype;
            if (!strcasecmp(s1, s2))
                break;  /* not a match */
        }
        if (!*list)
            continue;
        /* Found it! */
        formp->h = f;
        return ST_SUCCESS;
    }
    st_fail_errno(formp, ST_EFMT, "File type `%s' is not known",
                  formp->filetype);
    return ST_EFMT;
}

/*
 * Check that we have a known effect name.  If found, copy name of
 * effect into structure and place a pointer to internal data.
 * Returns -1 on error else it turns the total number of arguments
 * that should be passed to this effect's getopt() function.
 */
int st_geteffect_opt(eff_t effp, int argc, char **argv)
{
    int i, optind;

    for (i = 0; st_effect_fns[i]; i++)
    {
        const st_effect_t *e = st_effect_fns[i]();

        if (e && e->name && strcasecmp(e->name, argv[0]) == 0) {
          effp->name = e->name;
          effp->h = e;
          for (optind = 1; optind < argc; optind++)
          {
              for (i = 0; st_effect_fns[i]; i++)
              {
                  const st_effect_t *e = st_effect_fns[i]();
                  if (e && e->name && strcasecmp(e->name, argv[optind]) == 0)
                    return (optind - 1);
              }
              /* Didn't find a match, try the next argument. */
          }
          /*
           * No matches found, all the following arguments are
           * for this effect passed in.
           */
          return (optind - 1);
        }
    }

    return (ST_EOF);
}

/*
 * Check that we have a known effect name.  If found, copy name of
 * effect into structure and place a pointer to internal data.
 * Returns -1 on on failure.
 */

int st_geteffect(eff_t effp, const char *effect_name)
{
    int i;

    for(i = 0; st_effect_fns[i]; i++) {
        const st_effect_t *e = st_effect_fns[i]();

        if (e && e->name && strcasecmp(e->name, effect_name) == 0) {
          effp->name = e->name;
          effp->h = e;
          return ST_SUCCESS;
        }
    }

    return (ST_EOF);
}

/*
 * Check if we have a known effect name.
 */
st_bool is_effect_name(char const * text)
{
    int i;

    for(i = 0; st_effect_fns[i]; i++) {
        const st_effect_t *e = st_effect_fns[i]();

        if (e && e->name && strcasecmp(e->name, text) == 0)
          return st_true;
    }

    return st_false;
}

/*
 * Copy input and output signal info into effect structures.
 * Must pass in a bitmask containing info of wheither ST_EFF_CHAN
 * or ST_EFF_RATE has been used previously on this effect stream.
 * If not running multiple effects then just pass in a value of 0.
 *
 * Return value is the same mask plus addition of ST_EFF_CHAN or
 * ST_EFF_RATE if it was used in this effect.  That make this
 * return value can be passed back into this function in future
 * calls.
 */

int st_updateeffect(eff_t effp, const st_signalinfo_t *in, const st_signalinfo_t *out, 
                    int effect_mask)
{
    effp->ininfo = *in;

    effp->outinfo = *out;

    if (in->channels != out->channels)
    {
        /* Only effects with ST_EFF_CHAN flag can actually handle
         * outputing a different number of channels then the input.
         */
        if (!(effp->h->flags & ST_EFF_CHAN))
        {
            /* If this effect is being ran before a ST_EFF_CHAN effect
             * then effect's output is the same as the input file. Else its
             * input contains same number of channels as the output
             * file.
             */
            if (effect_mask & ST_EFF_CHAN)
                effp->ininfo.channels = out->channels;
            else
                effp->outinfo.channels = in->channels;

        }
    }

    if (in->rate != out->rate)
    {
        /* Only the ST_EFF_RATE effect can handle an input that
         * is a different sample rate then the output.
         */
        if (!(effp->h->flags & ST_EFF_RATE))
        {
            if (effect_mask & ST_EFF_RATE)
                effp->ininfo.rate = out->rate;
            else
                effp->outinfo.rate = in->rate;
        }
    }

    if (effp->h->flags & ST_EFF_CHAN)
        effect_mask |= ST_EFF_CHAN;
    if (effp->h->flags & ST_EFF_RATE)
        effect_mask |= ST_EFF_RATE;

    return effect_mask;
}

/*
 * st_parsesamples
 *
 * Parse a string for # of samples.  If string ends with a 's'
 * then the string is interpreted as a user calculated # of samples.
 * If string contains ':' or '.' or if it ends with a 't' then its
 * treated as an amount of time.  This is converted into seconds and
 * fraction of seconds and then use the sample rate to calculate
 * # of samples.
 * Returns NULL on error, pointer to next char to parse otherwise.
 */
char const * st_parsesamples(st_rate_t rate, const char *str, st_size_t *samples, char def)
{
    int found_samples = 0, found_time = 0;
    int time = 0;
    long long_samples;
    float frac = 0;
    char const * end;
    char const * pos;
    st_bool found_colon, found_dot;

    for (end = str; *end && strchr("0123456789:.ts", *end); ++end);
    if (end == str)
      return NULL;

    pos = strchr(str, ':');
    found_colon = pos && pos < end;
    
    pos = strchr(str, '.');
    found_dot = pos && pos < end;

    if (found_colon || found_dot || *(end-1) == 't')
        found_time = 1;
    else if (*(end-1) == 's')
        found_samples = 1;

    if (found_time || (def == 't' && !found_samples))
    {
        *samples = 0;

        while(1)
        {
            if (str[0] != '.' && sscanf(str, "%d", &time) != 1)
                return NULL;
            *samples += time;

            while (*str != ':' && *str != '.' && *str != 0)
                str++;

            if (*str == '.' || *str == 0)
                break;

            /* Skip past ':' */
            str++;
            *samples *= 60;
        }

        if (*str == '.')
        {
            if (sscanf(str, "%f", &frac) != 1)
                return NULL;
        }

        *samples *= rate;
        *samples += (rate * frac) + 0.5;
        return end;
    }
    if (found_samples || (def == 's' && !found_time))
    {
        if (sscanf(str, "%ld", &long_samples) != 1)
            return NULL;
        *samples = long_samples;
        return end;
    }
    return NULL;
}
