/*--------------------------------------------------------------------------*
 *                         Q_PULSE.H                                        *
 *--------------------------------------------------------------------------*
 * Coding and decoding of algebraic codebook                                *
 *--------------------------------------------------------------------------*/

#include "typedef.h"

Word32 quant_1p_N1(                        /* (o) return (N+1) bits           */
     Word16 pos,                           /* (i) position of the pulse       */
     Word16 N);                            /* (i) number of bits for position */

Word32 quant_2p_2N1(                       /* (o) return (2*N)+1 bits         */
     Word16 pos1,                          /* (i) position of the pulse 1     */
     Word16 pos2,                          /* (i) position of the pulse 2     */
     Word16 N);                            /* (i) number of bits for position */

Word32 quant_3p_3N1(                       /* (o) return (3*N)+1 bits         */
     Word16 pos1,                          /* (i) position of the pulse 1     */
     Word16 pos2,                          /* (i) position of the pulse 2     */
     Word16 pos3,                          /* (i) position of the pulse 3     */
     Word16 N);                            /* (i) number of bits for position */

Word32 quant_4p_4N1(                       /* (o) return (4*N)+1 bits         */
     Word16 pos1,                          /* (i) position of the pulse 1     */
     Word16 pos2,                          /* (i) position of the pulse 2     */
     Word16 pos3,                          /* (i) position of the pulse 3     */
     Word16 pos4,                          /* (i) position of the pulse 4     */
     Word16 N);                            /* (i) number of bits for position */

Word32 quant_4p_4N(                        /* (o) return 4*N bits             */
     Word16 pos[],                         /* (i) position of the pulse 1..4  */
     Word16 N);                            /* (i) number of bits for position */

Word32 quant_5p_5N(                        /* (o) return 5*N bits             */
     Word16 pos[],                         /* (i) position of the pulse 1..5  */
     Word16 N);                            /* (i) number of bits for position */

Word32 quant_6p_6N_2(                      /* (o) return (6*N)-2 bits         */
     Word16 pos[],                         /* (i) position of the pulse 1..6  */
     Word16 N);                            /* (i) number of bits for position */


void dec_1p_N1(Word32 index, Word16 N, Word16 offset, Word16 pos[]);
void dec_2p_2N1(Word32 index, Word16 N, Word16 offset, Word16 pos[]);
void dec_3p_3N1(Word32 index, Word16 N, Word16 offset, Word16 pos[]);
void dec_4p_4N1(Word32 index, Word16 N, Word16 offset, Word16 pos[]);
void dec_4p_4N(Word32 index, Word16 N, Word16 offset, Word16 pos[]);
void dec_5p_5N(Word32 index, Word16 N, Word16 offset, Word16 pos[]);
void dec_6p_6N_2(Word32 index, Word16 N, Word16 offset, Word16 pos[]);
