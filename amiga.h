#ifdef AMIGA

#include <fcntl.h>

#ifdef AMIGA_MC68881
#include <m68881.h>
#endif /* AMIGA_MC68881 */

#include "patchlvl.h"		/* yeah, I know it's not really a header...but why not? */

/* Following is a really screwy way of incorporating compile-time info into *
 * the binary as an Amiga version string.  Unfortunately, it was the only   *
 * method I could find.  --dgc, 13 Jan 93                                   */

#define AmiVerChars1	{'$', 'V', 'E', 'R', ':', ' ', 'S', 'o', 'u', 'n', 'd', ' ', 'E', 'x', 'c', 'h', 'a', 'n', 'g', 'e', ' ', 
#define AmiVerChars2	'6', '8', '0', '3', '0', 
#define AmiVerChars3	'/', 
#define AmiVerChars4	'6', '8', '8', '8', '1', 
#define AmiVerChars5	' ', 'P', 'a', 't', 'c', 'h', 'l', 'e', 'v', 'e', 'l', 
	' ', '0'+(PATCHLEVEL/10), '0'+(PATCHLEVEL%10), '\0'}

#ifdef AMIGA_MC68881
#ifdef AMIGA_MC68030
#define AmiVerChars	AmiVerChars1 AmiVerChars2 AmiVerChars3 AmiVerChars4 AmiVerChars5
#else
#define AmiVerChars	AmiVerChars1 AmiVerChars4 AmiVerChars5
#endif /* AMIGA_MC68030 */
#else
#ifdef AMIGA_MC68030
#define AmiVerChars	AmiVerChars1 AmiVerChars2 AmiVerChars5
#else
#define AmiVerChars	AmiVerChars1 AmiVerChars5
#endif /* AMIGA_MC68030 */
#endif /*AMIGA_MC68881*/

/* if you change these strings, be sure to change the size here! */
/* (and remember, sizeof() won't work)                           */
#define AmiVerSize 46

/* stdarg adjustments */
#ifndef va_dcl
#define va_dcl int va_alist;
#endif /* !va_dcl*/

/* BSD compat */
#include <string.h>
/* SAS/C does these; other might not */
#ifndef bcopy
#define	bcopy(from, to, len)	memmove(to, from, len)
#endif

/* SAS/C library code includes unlink().   *
 * If your compiler doesn't have unlink(), *
 * uncomment this section.                 */
/*
#ifndef unlink
#define	unlink		DeleteFile
#endif
*/

#endif /*AMIGA*/
