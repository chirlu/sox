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
#include <string.h>
#include <stdarg.h>


void sox_output_message(FILE *file, const char *filename, const char *fmt, va_list ap)
{
  char const * slash_pos = LAST_SLASH(filename);
  char const * base_name = slash_pos? slash_pos + 1 : filename;
  char const * dot_pos   = strrchr(base_name, '.');
  fprintf(file, "%.*s: ", dot_pos? dot_pos - base_name : -1, base_name);
  vfprintf(file, fmt, ap);
}

#undef sox_fail
#undef sox_warn
#undef sox_report
#undef sox_debug
#undef sox_debug_more
#undef sox_debug_most

#define SOX_MESSAGE_FUNCTION(name,level) \
void name(char const * fmt, ...) { \
  va_list ap; \
  va_start(ap, fmt); \
  if (sox_globals.output_message_handler) \
    (*sox_globals.output_message_handler)(level,sox_globals.subsystem,fmt,ap); \
  va_end(ap); \
}

SOX_MESSAGE_FUNCTION(sox_fail  , 1)
SOX_MESSAGE_FUNCTION(sox_warn  , 2)
SOX_MESSAGE_FUNCTION(sox_report, 3)
SOX_MESSAGE_FUNCTION(sox_debug , 4)
SOX_MESSAGE_FUNCTION(sox_debug_more , 5)
SOX_MESSAGE_FUNCTION(sox_debug_most , 6)

#undef SOX_MESSAGE_FUNCTION

void sox_fail_errno(sox_format_t * ft, int sox_errno, const char *fmt, ...)
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



/*--------------------------------- Comments ---------------------------------*/

size_t num_comments(comments_t comments)
{
  size_t result = 0;
  if (!comments)
    return 0;
  while (*comments++)
    ++result;
  return result;
}

void append_comment(comments_t * comments, char const * comment)
{
  size_t n = num_comments(*comments);
  *comments = xrealloc(*comments, (n + 2) * sizeof(**comments));
  assert(comment);
  (*comments)[n++] = xstrdup(comment);
  (*comments)[n] = 0;
}

void append_comments(comments_t * comments, char const * comment)
{
  char * end;
  if (comment) {
    while ((end = strchr(comment, '\n'))) {
      size_t len = end - comment;
      char * c = xmalloc((len + 1) * sizeof(*c));
      strncpy(c, comment, len);
      c[len] = '\0';
      append_comment(comments, c);
      comment += len + 1;
      free(c);
    }
    if (*comment)
      append_comment(comments, comment);
  }
}

comments_t copy_comments(comments_t comments)
{
  comments_t result = 0;

  if (comments) while (*comments)
    append_comment(&result, *comments++);
  return result;
}

void delete_comments(comments_t * comments)
{
  comments_t p = *comments;

  if (p) while (*p)
    free(*p++);
  free(*comments);
  *comments = 0;
}

char * cat_comments(comments_t comments)
{
  comments_t p = comments;
  size_t len = 0;
  char * result;

  if (p) while (*p)
    len += strlen(*p++) + 1;

  result = xcalloc(len? len : 1, sizeof(*result));

  if ((p = comments) && *p) {
    strcpy(result, *p);
    while (*++p)
      strcat(strcat(result, "\n"), *p);
  }
  return result;
}
