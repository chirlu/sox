/*

 * Revision 1.2  1996/08/20  20:28:05  jaf
 * Removed all static local variables that were SAVE'd in the Fortran
 * code, and put them in struct lpc10_encoder_state that is passed as an
 * argument.
 *
 * Removed init function, since all initialization is now done in
 * init_lpc10_encoder_state().
 *
 * Revision 1.1  1996/08/19  22:32:04  jaf
 * Initial revision
 *

*/

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

extern int hp100_(real *speech, integer *start, integer *end,
		  struct lpc10_encoder_state *st);
extern int inithp100_(void);

/* ********************************************************************* */

/*      HP100 Version 55 */

/* ********************************************************************* */

/*    100 Hz High Pass Filter */

/* Jan 92 - corrected typo (1.937148 to 1.935715), */
/*          rounded coefficients to 7 places, */
/*          corrected and merged gain (.97466**4), */
/*          merged numerator into first two sections. */

/* Input: */
/*  start, end - Range of samples to filter */
/* Input/Output: */
/*  speech(end) - Speech data. */
/*                Indices start through end are read and modified. */

/* This subroutine maintains local state from one call to the next.  If */
/* you want to switch to using a new audio stream for this filter, or */
/* reinitialize its state for any other reason, call the ENTRY */
/* INITHP100. */
/* Subroutine */ int hp100_(real *speech, integer *start, integer *end,
	struct lpc10_encoder_state *st)
{
    /* Temporary local copies of variables in lpc10_encoder_state.
       I've only created these because it might cause the loop below
       to execute a bit faster to access local variables, rather than
       variables in the lpc10_encoder_state structure.  It is just a
       guess that it will be faster. */

    real z11;
    real z21;
    real z12;
    real z22;

    /* System generated locals */
    integer i__1;

    /* Local variables */
    integer i__;
    real si, err;

/*       Arguments */
/*       Local variables that need not be saved */
/*       Local state */
    /* Parameter adjustments */
    if (speech) {
	--speech;
	}

    /* Function Body */

    z11 = st->z11;
    z21 = st->z21;
    z12 = st->z12;
    z22 = st->z22;

    i__1 = *end;
    for (i__ = *start; i__ <= i__1; ++i__) {
	si = speech[i__];
	err = si + z11 * 1.859076f - z21 * .8648249f;
	si = err - z11 * 2.f + z21;
	z21 = z11;
	z11 = err;
	err = si + z12 * 1.935715f - z22 * .9417004f;
	si = err - z12 * 2.f + z22;
	z22 = z12;
	z12 = err;
	speech[i__] = si * .902428f;
    }

    st->z11 = z11;
    st->z21 = z21;
    st->z12 = z12;
    st->z22 = z22;

    return 0;
} /* hp100_ */
