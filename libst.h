/* libst.h - include file for portable sound tools library
**
** Copyright (C) 1989 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#define MINLIN -32768
#define MAXLIN 32767
#define LINCLIP(x) do { if ( x < MINLIN ) x = MINLIN ; else if ( x > MAXLIN ) x = MAXLIN; } while ( 0 )

/* These do not round data.  Caller must round appropriately. */

#ifdef FAST_ULAW_CONVERSION
extern int ulaw_exp_table[256];
extern unsigned char ulaw_comp_table[16384];
#define st_ulaw_to_linear(ulawbyte) ulaw_exp_table[ulawbyte]
#define st_linear_to_ulaw(linearword) ulaw_comp_table[(linearword / 4) & 0x3fff]
#else
unsigned char st_linear_to_ulaw( /* int sample */ );
int st_ulaw_to_linear( /* unsigned char ulawbyte */ );
#endif

#ifdef FAST_ALAW_CONVERSION
extern int Alaw_exp_table[256];
extern unsigned char Alaw_comp_table[16384];
#define st_Alaw_to_linear(Alawbyte) Alaw_exp_table[Alawbyte]
#define st_linear_to_Alaw(linearword) Alaw_comp_table[(linearword / 4) & 0x3fff]
#else
unsigned char st_linear_to_Alaw( /* int sample */ );
int st_Alaw_to_linear( /* unsigned char ulawbyte */ );
#endif

#ifdef	USG
#define	setbuffer(x,y,z)
#endif
