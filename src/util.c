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
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdarg.h>

sox_size_t sox_bufsiz = 8192;
sox_output_message_handler_t sox_output_message_handler = NULL;
unsigned sox_output_verbosity_level = 2;

sox_global_info_t sox_global_info;

sox_effects_global_info_t effects_global_info =
    {sox_false, 1, &sox_global_info};

void sox_output_message(FILE *file, const char *filename, const char *fmt, va_list ap)
{
  char buffer[10];
  char const * handler_name;
  char const * dot_pos;
 
  handler_name = strrchr(filename, '/');
  if (handler_name != NULL) {
    ++handler_name;
  } else {
    handler_name = strrchr(filename, '\\');
    if (handler_name != NULL)
      ++handler_name;
    else
      handler_name = filename;
  }

  dot_pos = strrchr(handler_name, '.');
  if (dot_pos != NULL && dot_pos - handler_name <= (ptrdiff_t)(sizeof(buffer) - 1)) {
    strncpy(buffer, handler_name, (size_t)(dot_pos - handler_name));
    buffer[dot_pos - handler_name] = '\0';
    handler_name = buffer;
  }

  fprintf(file, "%s: ", handler_name);
  vfprintf(file, fmt, ap);
}



/* This is a bit of a hack.  It's useful to have libSoX
 * report which format or effect handler is outputing
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
#ifdef HAVE_VSNPRINTF
        vsnprintf(ft->sox_errstr, sizeof(ft->sox_errstr), fmt, args);
#else
        vsprintf(ft->sox_errstr, fmt, args);
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
    unsigned i;

    if (!formp->filetype) {
        sox_fail_errno(formp, SOX_EFMT, "Filetype was not specified");
        return SOX_EFMT;
    }
    for (i = 0; i < sox_formats; i++) {
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
    sox_fail_errno(formp, SOX_EFMT, "File type `%s' is not known",
                  formp->filetype);
    return SOX_EFMT;
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
