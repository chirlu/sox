/*
 * util.c.
 * Utility functions mostly related to logging.
 *
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "sox_i.h"
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#ifdef _MSC_VER
#define popen _popen
#endif

sox_output_message_handler_t sox_output_message_handler = NULL;
unsigned sox_output_verbosity_level = 2;

void sox_output_message(FILE *file, const char *filename, const char *fmt, va_list ap)
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



/* This is a bit of a hack.  It's useful to have libSoX
 * report which driver (i.e. format or effect handler) is outputing
 * the message.  Using the filename for this purpose is only an
 * approximation, but it saves a lot of work. ;)
 */
char const * sox_message_filename = 0;



static void sox_emit_message(unsigned level, char const *fmt, va_list ap)
{
  if (sox_output_message_handler != NULL)
    (*sox_output_message_handler)(level, sox_message_filename, fmt, ap);
}



#undef sox_fail
#undef sox_warn
#undef sox_report
#undef sox_debug
#undef sox_debug_more
#undef sox_debug_most

#define SOX_MESSAGE_FUNCTION(name,level) \
void name(char const * fmt, ...) \
{ \
  va_list args; \
\
  va_start(args, fmt); \
  sox_emit_message(level, fmt, args); \
  va_end(args); \
}

SOX_MESSAGE_FUNCTION(sox_fail  , 1)
SOX_MESSAGE_FUNCTION(sox_warn  , 2)
SOX_MESSAGE_FUNCTION(sox_report, 3)
SOX_MESSAGE_FUNCTION(sox_debug , 4)
SOX_MESSAGE_FUNCTION(sox_debug_more , 5)
SOX_MESSAGE_FUNCTION(sox_debug_most , 6)

#undef SOX_MESSAGE_FUNCTION

void sox_fail_errno(ft_t ft, int sox_errno, const char *fmt, ...)
{
        va_list args;

        ft->sox_errno = sox_errno;

        va_start(args, fmt);
#ifdef _MSC_VER
        vsprintf(ft->sox_errstr, fmt, args);
#else
        vsnprintf(ft->sox_errstr, sizeof(ft->sox_errstr), fmt, args);
#endif
        va_end(args);
        ft->sox_errstr[255] = '\0';
}

/*
 * Check that we have a known format suffix string.
 */
int sox_gettype(ft_t formp, sox_bool is_file_extension)
{
    const char * const *list;
    int i;

    if (!formp->filetype) {
        sox_fail_errno(formp, SOX_EFMT, "Filetype was not specified");
        return SOX_EFMT;
    }
    for (i = 0; i < sox_formats; i++) {
      /* FIXME: add only non-NULL formats to the list */
      if (sox_format_fns[i].fn) {
        const sox_format_t *f = sox_format_fns[i].fn();
        if (is_file_extension && (f->flags & SOX_FILE_DEVICE))
          continue; /* don't match device name in file name extensions */
        for (list = f->names; *list; list++) {
            const char *s1 = *list, *s2 = formp->filetype;
            if (!strcasecmp(s1, s2))
                break;  /* not a match */
        }
        if (!*list)
            continue;
        /* Found it! */
        formp->h = f;
        return SOX_SUCCESS;
      }
    }
    sox_fail_errno(formp, SOX_EFMT, "File type `%s' is not known",
                  formp->filetype);
    return SOX_EFMT;
}

/*
 * Check that we have a known effect name.  If found, copy name of
 * effect into structure and place a pointer to internal data.
 * Returns -1 on error else it turns the total number of arguments
 * that should be passed to this effect's getopt() function.
 */
int sox_geteffect_opt(eff_t effp, int argc, char **argv)
{
    int i, optind;

    for (i = 0; sox_effect_fns[i]; i++)
    {
        const sox_effect_t *e = sox_effect_fns[i]();

        if (e && e->name && strcasecmp(e->name, argv[0]) == 0) {
          effp->name = e->name;
          effp->h = e;
          for (optind = 1; optind < argc; optind++)
          {
              for (i = 0; sox_effect_fns[i]; i++)
              {
                  const sox_effect_t *e = sox_effect_fns[i]();
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

    return (SOX_EOF);
}

/*
 * Check that we have a known effect name.  If found, copy name of
 * effect into structure and place a pointer to internal data.
 * Returns -1 on on failure.
 */

int sox_geteffect(eff_t effp, const char *effect_name)
{
    int i;

    for(i = 0; sox_effect_fns[i]; i++) {
        const sox_effect_t *e = sox_effect_fns[i]();

        if (e && e->name && strcasecmp(e->name, effect_name) == 0) {
          effp->name = e->name;
          effp->h = e;
          return SOX_SUCCESS;
        }
    }

    return (SOX_EOF);
}

/*
 * Check if we have a known effect name.
 */
sox_bool is_effect_name(char const * text)
{
    int i;

    for(i = 0; sox_effect_fns[i]; i++) {
        const sox_effect_t *e = sox_effect_fns[i]();

        if (e && e->name && strcasecmp(e->name, text) == 0)
          return sox_true;
    }

    return sox_false;
}

/*
 * Copy input and output signal info into effect structures.
 * Must pass in a bitmask containing info of wheither SOX_EFF_CHAN
 * or SOX_EFF_RATE has been used previously on this effect stream.
 * If not running multiple effects then just pass in a value of 0.
 *
 * Return value is the same mask plus addition of SOX_EFF_CHAN or
 * SOX_EFF_RATE if it was used in this effect.  That make this
 * return value can be passed back into this function in future
 * calls.
 */

int sox_updateeffect(eff_t effp, const sox_signalinfo_t *in, const sox_signalinfo_t *out, 
                    int effect_mask)
{
    effp->ininfo = *in;

    effp->outinfo = *out;

    if (in->channels != out->channels)
    {
        /* Only effects with SOX_EFF_CHAN flag can actually handle
         * outputing a different number of channels then the input.
         */
        if (!(effp->h->flags & SOX_EFF_CHAN))
        {
            /* If this effect is being ran before a SOX_EFF_CHAN effect
             * then effect's output is the same as the input file. Else its
             * input contains same number of channels as the output
             * file.
             */
            if (effect_mask & SOX_EFF_CHAN)
                effp->ininfo.channels = out->channels;
            else
                effp->outinfo.channels = in->channels;

        }
    }

    if (in->rate != out->rate)
    {
        /* Only the SOX_EFF_RATE effect can handle an input that
         * is a different sample rate then the output.
         */
        if (!(effp->h->flags & SOX_EFF_RATE))
        {
            if (effect_mask & SOX_EFF_RATE)
                effp->ininfo.rate = out->rate;
            else
                effp->outinfo.rate = in->rate;
        }
    }

    if (effp->h->flags & SOX_EFF_CHAN)
        effect_mask |= SOX_EFF_CHAN;
    if (effp->h->flags & SOX_EFF_RATE)
        effect_mask |= SOX_EFF_RATE;

    return effect_mask;
}

/*
 * sox_parsesamples
 *
 * Parse a string for # of samples.  If string ends with a 's'
 * then the string is interpreted as a user calculated # of samples.
 * If string contains ':' or '.' or if it ends with a 't' then its
 * treated as an amount of time.  This is converted into seconds and
 * fraction of seconds and then use the sample rate to calculate
 * # of samples.
 * Returns NULL on error, pointer to next char to parse otherwise.
 */
char const * sox_parsesamples(sox_rate_t rate, const char *str, sox_size_t *samples, int def)
{
    int found_samples = 0, found_time = 0;
    int time = 0;
    long long_samples;
    float frac = 0;
    char const * end;
    char const * pos;
    sox_bool found_colon, found_dot;

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
