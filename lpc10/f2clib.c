/*

$Log: f2clib.c,v $
Revision 1.2  2007/04/18 13:59:59  rrt
Remove $Log tokens and associated log messages (in many files, several
copies of every log message were being written) and lots of warnings.

Revision 1.1  2007/04/16 21:57:06  rrt
LPC-10 support, documentation still to come; I wanted to land the code
before 14.0.0 went into test, and I'll be busy tomorrow.

Not highly tested either, but it's just a format, doesn't interfere
with anything else, and I'll get on that case before we go stable.

 * Revision 1.1  1996/08/19  22:32:10  jaf
 * Initial revision
 *

*/

/*
 * f2clib.c
 *
 * SCCS ID:  @(#)f2clib.c 1.2 96/05/19
 */

#include "f2c.h"

integer pow_ii(integer *ap, integer *bp);

integer pow_ii(integer *ap, integer *bp)
{
	integer pow, x, n;
	unsigned long u;

	x = *ap;
	n = *bp;

	if (n <= 0) {
		if (n == 0 || x == 1)
			return 1;
		if (x != -1)
			return x == 0 ? 1/x : 0;
		n = -n;
		}
	u = n;
	for(pow = 1; ; )
		{
		if(u & 01)
			pow *= x;
		if(u >>= 1)
			x *= x;
		else
			break;
		}
	return(pow);
	}


double r_sign(real *a, real *b);

double r_sign(real *a, real *b)
{
double x;
x = (*a >= 0 ? *a : - *a);
return( *b >= 0 ? x : -x);
}


integer i_nint(real *x);

#undef abs
#include "math.h"
integer i_nint(real *x)
{
return( (*x)>=0 ?
	floor(*x + .5) : -floor(.5 - *x) );
}
