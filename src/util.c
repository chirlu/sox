/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

#include "st.h"
#include <string.h>
#include <ctype.h>
#include <signal.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

/*
 * util.c.
 * Incorporate Jimen Ching's fixes for real library operation: Aug 3, 1994.
 * Redo all work from scratch, unfortunately.
 * Separate out all common variables used by effects & handlers,
 * and utility routines for other main programs to use.
 */

/* export flags */
/* FIXME: To be moved inside of fileop structure per handler. */
int verbose = 0;	/* be noisy on stderr */

/* FIXME:  These functions are user level concepts.  Move them outside
 * the ST library. 
 */
char *myname = 0;

void
st_report(const char *fmt, ...) 
{
	va_list args;

	if (! verbose)
		return;

	fprintf(stderr, "%s: ", myname);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}


void
st_warn(const char *fmt, ...) 
{
	va_list args;

	fprintf(stderr, "%s: ", myname);
	va_start(args, fmt);

	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}

void
st_fail(const char *fmt, ...) 
{
	va_list args;
	extern void cleanup();

	fprintf(stderr, "%s: ", myname);

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	cleanup();
	exit(2);
}


/* Warning: no error checking is done with errstr.  Be sure not to
 * go over the array limit ourself!
 */
void
st_fail_errno(ft_t ft, int st_errno, const char *fmt, ...)
{
	va_list args;

	ft->st_errno = st_errno;

	va_start(args, fmt);
	vsprintf(ft->st_errstr, fmt, args);
	va_end(args);
}

int st_is_bigendian(void)
{
    int b;
    char *p;

    b = 1;
    p = (char *) &b;
    if (!*p)
	return 1;
    else
	return 0;
}

int st_is_littleendian(void)
{
    int b;
    char *p;

    b = 1;
    p = (char *) &b;
    if (*p)
	return 1;
    else
	return 0;
}

int strcmpcase(s1, s2)
char *s1, *s2;
{
	while(*s1 && *s2 && (tolower(*s1) == tolower(*s2)))
		s1++, s2++;
	return *s1 - *s2;
}

/*
 * Check that we have a known format suffix string.
 */
void
st_gettype(formp)
ft_t formp;
{
	char **list;
	int i;

	if (! formp->filetype)
st_fail("Must give file type for %s file, either as suffix or with -t option",
formp->filename);
	for(i = 0; st_formats[i].names; i++) {
		for(list = st_formats[i].names; *list; list++) {
			char *s1 = *list, *s2 = formp->filetype;
			if (! strcmpcase(s1, s2))
				break;	/* not a match */
		}
		if (! *list)
			continue;
		/* Found it! */
		formp->h = &st_formats[i];
		return;
	}
	if (! strcmpcase(formp->filetype, "snd")) {
		verbose = 1;
		st_report("File type '%s' is used to name several different formats.", formp->filetype);
		st_report("If the file came from a Macintosh, it is probably");
		st_report("a .ub file with a sample rate of 11025 (or possibly 5012 or 22050).");
		st_report("Use the sequence '-t .ub -r 11025 file.snd'");
		st_report("If it came from a PC, it's probably a Soundtool file.");
		st_report("Use the sequence '-t .sndt file.snd'");
		st_report("If it came from a NeXT, it's probably a .au file.");
		st_fail("Use the sequence '-t .au file.snd'\n");
	}
	st_fail("File type '%s' of %s file is not known!",
		formp->filetype, formp->filename);
}

/*
 * Check that we have a known effect name.  If found, copy name of
 * effect into structure and place a pointer to internal data.
 * Returns -1 on error else it turns the total number of arguments
 * that should be passed to this effects getopt() function.
 */
int st_geteffect_opt(eff_t effp, int argc, char **argv)
{
	int i, optind;

	for(i = 0; st_effects[i].name; i++) 
	{
	    char *s1 = st_effects[i].name, *s2 = argv[0];

	    while(*s1 && *s2 && (tolower(*s1) == tolower(*s2)))
		s1++, s2++;
	    if (*s1 || *s2)
		continue;	/* not a match */

	    /* Found it! */
	    effp->name = st_effects[i].name;
	    effp->h = &st_effects[i];

	    optind = 1;

	    while (optind < argc)
	    {
	        for (i = 0; st_effects[i].name; i++)
	        {
		    char *s1 = st_effects[i].name, *s2 = argv[optind];
		    while (*s1 && *s2 && (tolower(*s1) == tolower(*s2)))
		    s1++, s2++;
		    if (*s1 || *s2)
		        continue;

		    /* Found it! */
		    return (optind - 1);
	        }
		/* Didn't find a match, try the next argument. */
		optind++;
	    }
	    /* 
	     * No matches found, all the following arguments are
	     * for this effect passed in.
	     */
	    return (optind - 1);
	}

	return (ST_EOF);
}

/*
 * Check that we have a known effect name.  If found, copy name of
 * effect into structure and place a pointer to internal data.
 * Returns -1 on on failure.
 */

int st_geteffect(eff_t effp, char *effect_name)
{
	int i;

	for(i = 0; st_effects[i].name; i++) {
		char *s1 = st_effects[i].name, *s2 = effect_name;

		while(*s1 && *s2 && (tolower(*s1) == tolower(*s2)))
			s1++, s2++;
		if (*s1 || *s2)
			continue;	/* not a match */

		/* Found it! */
		effp->name = st_effects[i].name;
		effp->h = &st_effects[i];

		return ST_SUCCESS;
	}

	return (ST_EOF);
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

int st_updateeffect(eff_t effp, ft_t in, ft_t out, int effect_mask)
{
    int i;

    effp->ininfo = in->info;
    effp->ininfo = in->info;

    effp->outinfo = out->info;
    effp->outinfo = out->info;

    for(i = 0; i < 8; i++) {
        memcpy(&effp->loops[i], &in->loops[i], sizeof(struct st_loopinfo));
	memcpy(&effp->loops[i], &in->loops[i], sizeof(struct st_loopinfo));
    }
    effp->instr = in->instr;
    effp->instr = in->instr;

    if (in->info.channels != out->info.channels)
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
		effp->ininfo.channels = out->info.channels;
	    else
		effp->outinfo.channels = in->info.channels;

	}
    }

    if (in->info.rate != out->info.rate)
    {
	/* Only the ST_EFF_RATE effect can handle an input that
	 * is a different sample rate then the output.
	 */
	if (!(effp->h->flags & ST_EFF_RATE))
	{
	    if (effect_mask & ST_EFF_RATE)
		effp->ininfo.rate = out->info.rate;
	    else
		effp->outinfo.rate = in->info.rate;
	}
    }

    if (effp->h->flags & ST_EFF_CHAN)
	effect_mask |= ST_EFF_CHAN;
    if (effp->h->flags & ST_EFF_RATE)
	effect_mask |= ST_EFF_RATE;

    return effect_mask;
}

/*
 * File format routines 
 */

void st_copyformat(ft, ft2)
ft_t ft, ft2;
{
	int noise = 0, i;
	double factor;

	if (ft2->info.rate == 0) {
		ft2->info.rate = ft->info.rate;
		noise = 1;
	}
	if (ft2->info.size == -1) {
		ft2->info.size = ft->info.size;
		noise = 1;
	}
	if (ft2->info.encoding == -1) {
		ft2->info.encoding = ft->info.encoding;
		noise = 1;
	}
	if (ft2->info.channels == -1) {
		ft2->info.channels = ft->info.channels;
		noise = 1;
	}
	if (ft2->comment == NULL) {
		ft2->comment = ft->comment;
		noise = 1;
	}
	/* 
	 * copy loop info, resizing appropriately 
	 * it's in samples, so # channels don't matter
	 */
	factor = (double) ft2->info.rate / (double) ft->info.rate;
	for(i = 0; i < ST_MAX_NLOOPS; i++) {
		ft2->loops[i].start = ft->loops[i].start * factor;
		ft2->loops[i].length = ft->loops[i].length * factor;
		ft2->loops[i].count = ft->loops[i].count;
		ft2->loops[i].type = ft->loops[i].type;
	}
	/* leave SMPTE # alone since it's absolute */
	ft2->instr = ft->instr;
}

void st_cmpformats(ft, ft2)
ft_t ft, ft2;
{
}

/* check that all settings have been given */
void st_checkformat(ft) 
ft_t ft;
{
	if (ft->info.rate == 0)
		st_fail("Sampling rate for %s file was not given\n", ft->filename);
	if ((ft->info.rate < 100) || (ft->info.rate > 999999L))
		st_fail("Sampling rate %lu for %s file is bogus\n", 
			ft->info.rate, ft->filename);
	if (ft->info.size == -1)
		st_fail("Data size was not given for %s file\nUse one of -b/-w/-l/-f/-d/-D", ft->filename);
	if (ft->info.encoding == -1 && ft->info.size != ST_SIZE_FLOAT)
		st_fail("Data encoding was not given for %s file\nUse one of -s/-u/-U/-A", ft->filename);
	/* it's so common, might as well default */
	if (ft->info.channels == -1)
		ft->info.channels = 1;
	/*	st_fail("Number of output channels was not given for %s file",
			ft->filename); */
}

static ft_t ft_queue[2];

void
sigint(s)
int s;
{
    if (s == SIGINT) {
	if (ft_queue[0])
	    ft_queue[0]->file.eof = 1;
	if (ft_queue[1])
	    ft_queue[1]->file.eof = 1;
    }
}

void
sigintreg(ft)
ft_t ft;
{
    if (ft_queue[0] == 0)
	ft_queue[0] = ft;
    else
	ft_queue[1] = ft;
    signal(SIGINT, sigint);
}
