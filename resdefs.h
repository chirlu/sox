
/*
 * FILE: stdefs.h
 *   BY: Christopher Lee Fraley
 * DESC: Defines standard stuff for inclusion in C programs.
 * DATE: 6-JUN-88
 * VERS: 1.0  (6-JUN-88, 2:00pm)
 */


#define TRUE  1
#define FALSE 0

/* Some files include both this file and math.h which will most
 * likely already have PI defined.
 */
#ifndef PI
#define PI (3.14159265358979232846)
#endif
#ifndef PI2
#define PI2 (6.28318530717958465692)
#endif
#define D2R (0.01745329348)          /* (2*pi)/360 */
#define R2D (57.29577951)            /* 360/(2*pi) */

#define MAX(x,y) ((x)>(y) ?(x):(y))
#define MIN(x,y) ((x)<(y) ?(x):(y))
#define ABS(x)   ((x)<0   ?(-(x)):(x))
#define SGN(x)   ((x)<0   ?(-1):((x)==0?(0):(1)))

typedef char           BOOL;
typedef short          HWORD;
typedef unsigned short UHWORD;
typedef int            IWORD;
#ifndef	WORD
typedef int		WORD;
#endif
typedef unsigned int   UWORD;

