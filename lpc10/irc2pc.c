/*

 * Revision 1.1  1996/08/19  22:31:56  jaf
 * Initial revision
 *

*/

/*  -- translated by f2c (version 19951025).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include "f2c.h"

extern int irc2pc_(real *rc, real *pc, integer *order, real *gprime, real *g2pass);

/* ***************************************************************** */

/* 	IRC2PC Version 48 */

/*
 * Revision 1.1  1996/08/19  22:31:56  jaf
 * Initial revision
 * */
/* Revision 1.3  1996/03/20  15:47:19  jaf */
/* Added comments about which indices of array arguments are read or */
/* written. */

/* Revision 1.2  1996/03/14  16:59:04  jaf */
/* Comments added explaining that none of the local variables of this */
/* subroutine need to be saved from one invocation to the next. */

/* Revision 1.1  1996/02/07 14:47:27  jaf */
/* Initial revision */


/* ***************************************************************** */

/*   Convert Reflection Coefficients to Predictor Coeficients */

/* Inputs: */
/*  RC     - Reflection coefficients */
/*           Indices 1 through ORDER read. */
/*  ORDER  - Number of RC's */
/*  GPRIME - Excitation modification gain */
/* Outputs: */
/*  PC     - Predictor coefficients */
/*           Indices 1 through ORDER written. */
/*           Indices 1 through ORDER-1 are read after being written. */
/*  G2PASS - Excitation modification sharpening factor */

/* This subroutine has no local state. */

/* Subroutine */ int irc2pc_(real *rc, real *pc, integer *order, real *gprime,
	 real *g2pass)
{
    /* System generated locals */
    integer i__1, i__2;

    /* Builtin functions */
    double sqrt(doublereal);

    /* Local variables */
    real temp[10];
    integer i__, j;

/* 	Arguments */
/*   LPC Configuration parameters: */
/* Frame size, Prediction order, Pitch period */
/*       Local variables that need not be saved */
    /* Parameter adjustments */
    --pc;
    --rc;

    /* Function Body */
    *g2pass = 1.f;
    i__1 = *order;
    for (i__ = 1; i__ <= i__1; ++i__) {
	*g2pass *= 1.f - rc[i__] * rc[i__];
    }
    *g2pass = *gprime * sqrt(*g2pass);
    pc[1] = rc[1];
    i__1 = *order;
    for (i__ = 2; i__ <= i__1; ++i__) {
	i__2 = i__ - 1;
	for (j = 1; j <= i__2; ++j) {
	    temp[j - 1] = pc[j] - rc[i__] * pc[i__ - j];
	}
	i__2 = i__ - 1;
	for (j = 1; j <= i__2; ++j) {
	    pc[j] = temp[j - 1];
	}
	pc[i__] = rc[i__];
    }
    return 0;
} /* irc2pc_ */

