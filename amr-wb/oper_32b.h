/* Double precision operations */
/* $Id: oper_32b.h,v 1.1 2007/02/15 23:22:35 robs Exp $ */

void L_Extract (Word32 L_32, Word16 *hi, Word16 *lo);
Word32 L_Comp (Word16 hi, Word16 lo);
Word32 Mpy_32 (Word16 hi1, Word16 lo1, Word16 hi2, Word16 lo2);
Word32 Mpy_32_16 (Word16 hi, Word16 lo, Word16 n);
Word32 Div_32 (Word32 L_num, Word16 denom_hi, Word16 denom_lo);

