/* libst.h - include file for portable sound tools library
**
** Copyright (C) 1989
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

/*******************************************************************/
/* Common function prototypes used internal to Sound Tools library */
/*******************************************************************/

/******************************************************************/
/* Define u-law and a-law functionsto convert to linear PCM data  */
/* Can use faster lookup tables by using the appropriate defines. */
/******************************************************************/
#ifdef FAST_ULAW_CONVERSION
extern const short         ulaw_exp_table[256];
extern const unsigned char ulaw_comp_table[16384];
#define st_ulaw_to_linear(ulawbyte)   ulaw_exp_table[ulawbyte]
#define st_linear_to_ulaw(linearword) ulaw_comp_table[(unsigned short)linearword >> 2]
#else
unsigned char st_linear_to_ulaw(short) REGPARM(1);
int           st_ulaw_to_linear(unsigned char) REGPARM(1);
#endif

#ifdef FAST_ALAW_CONVERSION
extern const short         Alaw_exp_table[256];
extern const unsigned char Alaw_comp_table[16384];
#define st_Alaw_to_linear(Alawbyte)   Alaw_exp_table[Alawbyte]
#define st_linear_to_Alaw(linearword) Alaw_comp_table[(unsigned short)linearword >> 2]
#else
unsigned char st_linear_to_Alaw(short) REGPARM(1);
int           st_Alaw_to_linear(unsigned char) REGPARM(1);
#endif

