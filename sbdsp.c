
char ansi_c_is_very_stupid_and_needs_a_variable_here;

#if	defined(BLASTER) || defined(SBLAST)
/*
 * Copyright 1992 Rick Richardson
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Rick Richardson, Lance Norskog And Sundry Contributors are not
 * responsible for the consequences of using this software.
 */

/*
 * Direct to Sound Blaster device driver.
 * SBLAST patches by John T. Kohl.
 */

#include <sys/types.h>
#ifdef SBLAST
#include <i386/isa/sblast.h>
#else
#include <sys/sb.h>
#endif
#include <signal.h>
#include "st.h"

/* Private data for SKEL file */
typedef struct sbdspstuff {
	int	samples;		/* bytes remaining in current block */
} *sbdsp_t;

static got_int = 0;

static void
sigint(s)
int s;
{
	if (s) got_int = 1;
	else signal(SIGINT, sigint);
}

/*
 * Do anything required before you start reading samples.
 * Read file header. 
 *	Find out sampling rate, 
 *	size and style of samples, 
 *	mono/stereo/quad.
 */
sbdspstartread(ft) 
ft_t ft;
{
	sbdsp_t sbdsp = (sbdsp_t) ft->priv;
#ifdef	SBLAST
	int off = 0;
#endif

	/* If you need to seek around the input file. */
	if (0 && ! ft->seekable)
		fail("SKEL input file must be a file, not a pipe");

	if (!ft->info.rate)
		ft->info.rate = 11000;
	ft->info.size = BYTE;
	ft->info.style = UNSIGNED;
	ft->info.channels = 1;
	ioctl(fileno(ft->fp), DSP_IOCTL_RESET, 0);
#ifdef SBLAST
	ioctl(fileno(ft->fp), DSP_IOCTL_VOICE, &off);
	ioctl(fileno(ft->fp), DSP_IOCTL_SPEED, &ft->info.rate);
#else
	ioctl(fileno(ft->fp), DSP_IOCTL_VOICE, 0);
	ioctl(fileno(ft->fp), DSP_IOCTL_SPEED, ft->info.rate);
#endif
	sigint(0);	/* Prepare to catch SIGINT */
}

/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */

sbdspread(ft, buf, len) 
ft_t ft;
LONG *buf, len;
{
	sbdsp_t sbdsp = (sbdsp_t) ft->priv;
	int		rc;

	if (got_int) return (0);
	rc = rawread(ft, buf, len);
	if (rc < 0) return 0;
	return (rc);
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
sbdspstopread(ft) 
ft_t ft;
{
#ifdef SBLAST
	ioctl(fileno(ft->fp), DSP_IOCTL_FLUSH, 0);
#endif
}

sbdspstartwrite(ft) 
ft_t ft;
{
	sbdsp_t sbdsp = (sbdsp_t) ft->priv;
#ifdef	SBLAST
	int on = 1;
#endif

	/* If you have to seek around the output file */
	if (0 && ! ft->seekable)
		fail("Output .sbdsp file must be a file, not a pipe");

	if (!ft->info.rate)
		ft->info.rate = 11000;
	ft->info.size = BYTE;
	ft->info.style = UNSIGNED;
	ft->info.channels = 1;
	ioctl(fileno(ft->fp), DSP_IOCTL_RESET, 0);
#ifdef SBLAST
	ioctl(fileno(ft->fp), DSP_IOCTL_FLUSH, 0);
	ioctl(fileno(ft->fp), DSP_IOCTL_VOICE, &on);
	ioctl(fileno(ft->fp), DSP_IOCTL_SPEED, &ft->info.rate);
#else
	ioctl(fileno(ft->fp), DSP_IOCTL_VOICE, 1);
	ioctl(fileno(ft->fp), DSP_IOCTL_SPEED, ft->info.rate);
#endif
}

sbdspwrite(ft, buf, len) 
ft_t ft;
LONG *buf, len;
{
	sbdsp_t sbdsp = (sbdsp_t) ft->priv;

	if (len == 0) return 0;
	return (rawwrite(ft, buf, len));
}

sbdspstopwrite(ft) 
ft_t ft;
{
	/* All samples are already written out. */
	/* If file header needs fixing up, for example it needs the */
 	/* the number of samples in a field, seek back and write them here. */
	fflush(ft->fp);
	ioctl(fileno(ft->fp), DSP_IOCTL_FLUSH, 0);
}
#endif
