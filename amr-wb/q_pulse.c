/*--------------------------------------------------------------------------*
 *                         Q_PULSE.C                                        *
 *--------------------------------------------------------------------------*
 * Coding and decodeing of algebraic codebook                               *
 *--------------------------------------------------------------------------*/

#include <stdio.h>

#include "typedef.h"
#include "basic_op.h"
#include "count.h"

#include "q_pulse.h"


#define NB_POS 16                          /* pos in track, mask for sign bit */


Word32 quant_1p_N1(                        /* (o) return N+1 bits             */
     Word16 pos,                           /* (i) position of the pulse       */
     Word16 N)                             /* (i) number of bits for position */
{
    Word16 mask;
    Word32 index;

    mask = sub(shl(1, N), 1);              /* mask = ((1<<N)-1); */
    /*-------------------------------------------------------*
     * Quantization of 1 pulse with N+1 bits:                *
     *-------------------------------------------------------*/
    index = L_deposit_l((Word16) (pos & mask));
    test();
    if ((pos & NB_POS) != 0)
    {
        index = L_add(index, L_deposit_l(shl(1, N)));   /* index += 1 << N; */
    }
    return (index);
}

void dec_1p_N1(Word32 index, Word16 N, Word16 offset, Word16 pos[])
{
    Word16 pos1;
    Word32 mask, i;

    mask = L_deposit_l(sub(shl(1, N), 1));   /* mask = ((1<<N)-1); */
    /*-------------------------------------------------------*
     * Decode 1 pulse with N+1 bits:                         *
     *-------------------------------------------------------*/
    pos1 = add(extract_l(index & mask), offset);        /* pos1 = ((index & mask) + offset); */
    i = (L_shr(index, N) & 1L);              /* i = ((index >> N) & 1); */
    test();
    if (L_sub(i, 1) == 0)
    {
        pos1 = add(pos1, NB_POS);
    }
    pos[0] = pos1;                         move16();

    return;
}


Word32 quant_2p_2N1(                       /* (o) return (2*N)+1 bits         */
     Word16 pos1,                          /* (i) position of the pulse 1     */
     Word16 pos2,                          /* (i) position of the pulse 2     */
     Word16 N)                             /* (i) number of bits for position */
{
    Word16 mask, tmp;
    Word32 index;

    mask = sub(shl(1, N), 1);              /* mask = ((1<<N)-1); */
    /*-------------------------------------------------------*
     * Quantization of 2 pulses with 2*N+1 bits:             *
     *-------------------------------------------------------*/
    test();logic16();logic16();
    if (((pos2 ^ pos1) & NB_POS) == 0)
    {
        /* sign of 1st pulse == sign of 2th pulse */
        test();
        if (sub(pos1, pos2) <= 0)          /* ((pos1 - pos2) <= 0) */
        {
            /* index = ((pos1 & mask) << N) + (pos2 & mask); */
            index = L_deposit_l(add(shl(((Word16) (pos1 & mask)), N), ((Word16) (pos2 & mask))));
        } else
        {
            /* ((pos2 & mask) << N) + (pos1 & mask); */
            index = L_deposit_l(add(shl(((Word16) (pos2 & mask)), N), ((Word16) (pos1 & mask))));
        }
        test();logic16();
        if ((pos1 & NB_POS) != 0)
        {
            tmp = shl(N, 1);
            index = L_add(index, L_shl(1L, tmp));       /* index += 1 << (2*N); */
        }
    } else
    {
        /* sign of 1st pulse != sign of 2th pulse */
        test();logic16();logic16();
        if (sub((Word16) (pos1 & mask), (Word16) (pos2 & mask)) <= 0)
        {
            /* index = ((pos2 & mask) << N) + (pos1 & mask); */
            index = L_deposit_l(add(shl(((Word16) (pos2 & mask)), N), ((Word16) (pos1 & mask))));       logic16();logic16();
            test();logic16();
            if ((pos2 & NB_POS) != 0)
            {
                tmp = shl(N, 1);           /* index += 1 << (2*N); */
                index = L_add(index, L_shl(1L, tmp));
            }
        } else
        {
            /* index = ((pos1 & mask) << N) + (pos2 & mask);     */
            index = L_deposit_l(add(shl(((Word16) (pos1 & mask)), N), ((Word16) (pos2 & mask))));       logic16();logic16();
            test();logic16();
            if ((pos1 & NB_POS) != 0)
            {
                tmp = shl(N, 1);
                index = L_add(index, L_shl(1, tmp));    /* index += 1 << (2*N); */
            }
        }
    }
    return (index);
}

void dec_2p_2N1(Word32 index, Word16 N, Word16 offset, Word16 pos[])
{
    Word16 pos1, pos2, tmp;
    Word32 mask, i;

    mask = L_deposit_l(sub(shl(1, N), 1)); /* mask = ((1<<N)-1); */
    /*-------------------------------------------------------*
     * Decode 2 pulses with 2*N+1 bits:                      *
     *-------------------------------------------------------*/
    /* pos1 = (((index >> N) & mask) + offset); */
    pos1 = extract_l(L_add((L_shr(index, N) & mask), L_deposit_l(offset)));     logic16();
    tmp = shl(N, 1);
    i = (L_shr(index, tmp) & 1L);          logic16();/* i = (index >> (2*N)) & 1; */
    pos2 = add(extract_l(index & mask), offset);        logic16();/* pos2 = ((index & mask) + offset); */
    test();
    if (sub(pos2, pos1) < 0)               /* ((pos2 - pos1) < 0) */
    {
        test();
        if (L_sub(i, 1L) == 0)
        {                                  /* (i == 1) */
            pos1 = add(pos1, NB_POS);      /* pos1 += NB_POS; */
        } else
        {
            pos2 = add(pos2, NB_POS);      /* pos2 += NB_POS;    */
        }
    } else
    {
        test();
        if (L_sub(i, 1L) == 0)
        {                                  /* (i == 1) */
            pos1 = add(pos1, NB_POS);      /* pos1 += NB_POS; */
            pos2 = add(pos2, NB_POS);      /* pos2 += NB_POS; */
        }
    }

    pos[0] = pos1;                         move16();
    pos[1] = pos2;                         move16();

    return;
}


Word32 quant_3p_3N1(                       /* (o) return (3*N)+1 bits         */
     Word16 pos1,                          /* (i) position of the pulse 1     */
     Word16 pos2,                          /* (i) position of the pulse 2     */
     Word16 pos3,                          /* (i) position of the pulse 3     */
     Word16 N)                             /* (i) number of bits for position */
{
    Word16 nb_pos;
    Word32 index;

    nb_pos = shl(1, sub(N, 1));            /* nb_pos = (1<<(N-1)); */
    /*-------------------------------------------------------*
     * Quantization of 3 pulses with 3*N+1 bits:             *
     *-------------------------------------------------------*/
    test();test();logic16();logic16();logic16();logic16();
    if (((pos1 ^ pos2) & nb_pos) == 0)
    {
        index = quant_2p_2N1(pos1, pos2, sub(N, 1));    /* index = quant_2p_2N1(pos1, pos2, (N-1)); */
        /* index += (pos1 & nb_pos) << N; */
        index = L_add(index, L_shl(L_deposit_l((Word16) (pos1 & nb_pos)), N));  logic16();
        /* index += quant_1p_N1(pos3, N) << (2*N); */
        index = L_add(index, L_shl(quant_1p_N1(pos3, N), shl(N, 1)));

    } else if (((pos1 ^ pos3) & nb_pos) == 0)
    {
        index = quant_2p_2N1(pos1, pos3, sub(N, 1));    /* index = quant_2p_2N1(pos1, pos3, (N-1)); */
        index = L_add(index, L_shl(L_deposit_l((Word16) (pos1 & nb_pos)), N));  logic16();
        /* index += (pos1 & nb_pos) << N; */
        index = L_add(index, L_shl(quant_1p_N1(pos2, N), shl(N, 1)));
        /* index += quant_1p_N1(pos2, N) <<
                                                                         * (2*N); */
    } else
    {
        index = quant_2p_2N1(pos2, pos3, sub(N, 1));    /* index = quant_2p_2N1(pos2, pos3, (N-1)); */
        /* index += (pos2 & nb_pos) << N;            */
        index = L_add(index, L_shl(L_deposit_l((Word16) (pos2 & nb_pos)), N));  logic16();
        /* index += quant_1p_N1(pos1, N) << (2*N);   */
        index = L_add(index, L_shl(quant_1p_N1(pos1, N), shl(N, 1)));
    }
    return (index);
}

void dec_3p_3N1(Word32 index, Word16 N, Word16 offset, Word16 pos[])
{
    Word16 j, tmp;
    Word32 mask, idx;

    /*-------------------------------------------------------*
     * Decode 3 pulses with 3*N+1 bits:                      *
     *-------------------------------------------------------*/
    tmp = sub(shl(N, 1), 1);               /* mask = ((1<<((2*N)-1))-1); */
    mask = L_sub(L_shl(1L, tmp), 1L);

    idx = index & mask;                    logic16();
    j = offset;
    tmp = sub(shl(N, 1), 1);

    test();logic16();
    if ((L_shr(index, tmp) & 1L) != 0L)
    {                                      /* if (((index >> ((2*N)-1)) & 1) == 1){ */
        j = add(j, shl(1, sub(N, 1)));     /* j += (1<<(N-1)); */
    }
    dec_2p_2N1(idx, (Word16) (N - 1), j, pos);

    mask = sub(shl(1, add(N, 1)), 1);      /* mask = ((1<<(N+1))-1); */
    tmp = shl(N, 1);                       /* idx = (index >> (2*N)) & mask; */
    idx = L_shr(index, tmp) & mask;        logic16();

    dec_1p_N1(idx, N, offset, pos + 2);    move16();

    return;
}


Word32 quant_4p_4N1(                       /* (o) return (4*N)+1 bits         */
     Word16 pos1,                          /* (i) position of the pulse 1     */
     Word16 pos2,                          /* (i) position of the pulse 2     */
     Word16 pos3,                          /* (i) position of the pulse 3     */
     Word16 pos4,                          /* (i) position of the pulse 4     */
     Word16 N)                             /* (i) number of bits for position */
{
    Word16 nb_pos;
    Word32 index;

    nb_pos = shl(1, sub(N, 1));            /* nb_pos = (1<<(N-1));  */
    /*-------------------------------------------------------*
     * Quantization of 4 pulses with 4*N+1 bits:             *
     *-------------------------------------------------------*/
    test();test();logic16();logic16();logic16();logic16();
    if (((pos1 ^ pos2) & nb_pos) == 0)
    {
        index = quant_2p_2N1(pos1, pos2, sub(N, 1));    /* index = quant_2p_2N1(pos1, pos2, (N-1)); */
        /* index += (pos1 & nb_pos) << N;    */
        index = L_add(index, L_shl(L_deposit_l((Word16) (pos1 & nb_pos)), N));  logic16();
        /* index += quant_2p_2N1(pos3, pos4, N) << (2*N); */
        index = L_add(index, L_shl(quant_2p_2N1(pos3, pos4, N), shl(N, 1)));
    } else if (((pos1 ^ pos3) & nb_pos) == 0)
    {
        index = quant_2p_2N1(pos1, pos3, sub(N, 1));
        /* index += (pos1 & nb_pos) << N; */
        index = L_add(index, L_shl(L_deposit_l((Word16) (pos1 & nb_pos)), N));  logic16();
        /* index += quant_2p_2N1(pos2, pos4, N) << (2*N); */
        index = L_add(index, L_shl(quant_2p_2N1(pos2, pos4, N), shl(N, 1)));
    } else
    {
        index = quant_2p_2N1(pos2, pos3, sub(N, 1));
        /* index += (pos2 & nb_pos) << N; */
        index = L_add(index, L_shl(L_deposit_l((Word16) (pos2 & nb_pos)), N));  logic16();
        /* index += quant_2p_2N1(pos1, pos4, N) << (2*N); */
        index = L_add(index, L_shl(quant_2p_2N1(pos1, pos4, N), shl(N, 1)));
    }
    return (index);
}

void dec_4p_4N1(Word32 index, Word16 N, Word16 offset, Word16 pos[])
{
    Word16 j, tmp;
    Word32 mask, idx;

    /*-------------------------------------------------------*
     * Decode 4 pulses with 4*N+1 bits:                      *
     *-------------------------------------------------------*/
    tmp = sub(shl(N, 1), 1);               /* mask = ((1<<((2*N)-1))-1); */
    mask = L_sub(L_shl(1L, tmp), 1L);
    idx = index & mask;                    logic16();
    j = offset;                            move16();
    tmp = sub(shl(N, 1), 1);

    test();logic16();
    if ((L_shr(index, tmp) & 1L) != 0L)
    {                                      /* (((index >> ((2*N)-1)) & 1) == 1) */
        j = add(j, shl(1, sub(N, 1)));     /* j += (1<<(N-1)); */
    }
    dec_2p_2N1(idx, (Word16) (N - 1), j, pos);


    tmp = add(shl(N, 1), 1);               /* mask = ((1<<((2*N)+1))-1); */
    mask = L_sub(L_shl(1L, tmp), 1L);
    idx = L_shr(index, shl(N, 1)) & mask;  logic16();/* idx = (index >> (2*N)) & mask; */
    dec_2p_2N1(idx, N, offset, pos + 2);   move16();  /* dec_2p_2N1(idx, N, offset, pos+2); */

    return;
}


Word32 quant_4p_4N(                        /* (o) return 4*N bits             */
     Word16 pos[],                         /* (i) position of the pulse 1..4  */
     Word16 N)                             /* (i) number of bits for position */
{
    Word16 i, j, k, nb_pos, mask, n_1, tmp;
    Word16 posA[4], posB[4];
    Word32 index;

    n_1 = (Word16) (N - 1);                move16();
    nb_pos = shl(1, n_1);                  /* nb_pos = (1<<n_1); */
    mask = sub(shl(1, N), 1);              /* mask = ((1<<N)-1); */

    i = 0;                                 move16();
    j = 0;                                 move16();
    for (k = 0; k < 4; k++)
    {
        test();logic16();
        if ((pos[k] & nb_pos) == 0)
        {
            posA[i++] = pos[k];            move16();
        } else
        {
            posB[j++] = pos[k];            move16();
        }
    }

    switch (i)
    {
    case 0:
        tmp = sub(shl(N, 2), 3);           /* index = 1 << ((4*N)-3); */
        index = L_shl(1L, tmp);
        /* index += quant_4p_4N1(posB[0], posB[1], posB[2], posB[3], n_1); */
        index = L_add(index, quant_4p_4N1(posB[0], posB[1], posB[2], posB[3], n_1));
        break;
    case 1:
        /* index = quant_1p_N1(posA[0], n_1) << ((3*n_1)+1); */
        tmp = add(extract_l(L_shr(L_mult(3, n_1), 1)), 1);
        index = L_shl(quant_1p_N1(posA[0], n_1), tmp);
        /* index += quant_3p_3N1(posB[0], posB[1], posB[2], n_1); */
        index = L_add(index, quant_3p_3N1(posB[0], posB[1], posB[2], n_1));
        break;
    case 2:
        tmp = add(shl(n_1, 1), 1);         /* index = quant_2p_2N1(posA[0], posA[1], n_1) << ((2*n_1)+1); */
        index = L_shl(quant_2p_2N1(posA[0], posA[1], n_1), tmp);
        /* index += quant_2p_2N1(posB[0], posB[1], n_1); */
        index = L_add(index, quant_2p_2N1(posB[0], posB[1], n_1));
        break;
    case 3:
        /* index = quant_3p_3N1(posA[0], posA[1], posA[2], n_1) << N; */
        index = L_shl(quant_3p_3N1(posA[0], posA[1], posA[2], n_1), N);
        index = L_add(index, quant_1p_N1(posB[0], n_1));        /* index += quant_1p_N1(posB[0], n_1); */
        break;
    case 4:
        index = quant_4p_4N1(posA[0], posA[1], posA[2], posA[3], n_1);
        break;
    default:
        index = 0;
        fprintf(stderr, "Error in function quant_4p_4N\n");
    }
    tmp = sub(shl(N, 2), 2);               /* index += (i & 3) << ((4*N)-2); */
    index = L_add(index, L_shl((L_deposit_l(i) & (3L)), tmp));  logic16();

    return (index);
}

void dec_4p_4N(Word32 index, Word16 N, Word16 offset, Word16 pos[])
{
    Word16 j, n_1, tmp;

    /*-------------------------------------------------------*
     * Decode 4 pulses with 4*N bits:                        *
     *-------------------------------------------------------*/

    n_1 = (Word16) (N - 1);                move16();
    j = add(offset, shl(1, n_1));          /* j = offset + (1 << n_1); */

    tmp = sub(shl(N, 2), 2);
    test();logic16();
    switch (L_shr(index, tmp) & 3)
    {                                      /* ((index >> ((4*N)-2)) & 3) */
    case 0:
        tmp = add(shl(n_1, 2), 1);

        test();logic16();
        if ((L_shr(index, tmp) & 1) == 0)
        {                                  /* (((index >> ((4*n_1)+1)) & 1) == 0) */
            dec_4p_4N1(index, n_1, offset, pos);
        } else
        {
            dec_4p_4N1(index, n_1, j, pos);
        }
        break;
    case 1:
        tmp = add(extract_l(L_shr(L_mult(3, n_1), 1)), 1); /* dec_1p_N1((index>>((3*n_1)+1)), n_1, offset, pos) */
        dec_1p_N1(L_shr(index, tmp), n_1, offset, pos);
        dec_3p_3N1(index, n_1, j, pos + 1);move16();
        break;
    case 2:
        tmp = add(shl(n_1, 1), 1);         /* dec_2p_2N1((index>>((2*n_1)+1)), n_1, offset, pos); */
        dec_2p_2N1(L_shr(index, tmp), n_1, offset, pos);
        dec_2p_2N1(index, n_1, j, pos + 2);move16();
        break;
    case 3:
        tmp = add(n_1, 1);                 /* dec_3p_3N1((index>>(n_1+1)), n_1, offset, pos); */
        dec_3p_3N1(L_shr(index, tmp), n_1, offset, pos);
        dec_1p_N1(index, n_1, j, pos + 3); move16();
        break;
    }
    return;
}


Word32 quant_5p_5N(                        /* (o) return 5*N bits             */
     Word16 pos[],                         /* (i) position of the pulse 1..5  */
     Word16 N)                             /* (i) number of bits for position */
{
    Word16 i, j, k, nb_pos, n_1, tmp;
    Word16 posA[5], posB[5];
    Word32 index, tmp2;

    n_1 = (Word16) (N - 1);                move16();
    nb_pos = shl(1, n_1);                  /* nb_pos = (1<<n_1); */

    i = 0;                                 move16();
    j = 0;                                 move16();
    for (k = 0; k < 5; k++)
    {
        test();logic16();
        if ((pos[k] & nb_pos) == 0)
        {
            posA[i++] = pos[k];            move16();
        } else
        {
            posB[j++] = pos[k];            move16();
        }
    }

    switch (i)
    {
    case 0:
        tmp = sub(extract_l(L_shr(L_mult(5, N), 1)), 1);        /* ((5*N)-1)) */
        index = L_shl(1L, tmp);   /* index = 1 << ((5*N)-1); */
        tmp = add(shl(N, 1), 1);  /* index += quant_3p_3N1(posB[0], posB[1], posB[2], n_1) << ((2*N)+1);*/
        tmp2 = L_shl(quant_3p_3N1(posB[0], posB[1], posB[2], n_1), tmp);
        index = L_add(index, tmp2);
        index = L_add(index, quant_2p_2N1(posB[3], posB[4], N));        /* index += quant_2p_2N1(posB[3], posB[4], N); */
        break;
    case 1:
        tmp = sub(extract_l(L_shr(L_mult(5, N), 1)), 1);        /* index = 1 << ((5*N)-1); */
        index = L_shl(1L, tmp);
        tmp = add(shl(N, 1), 1);   /* index += quant_3p_3N1(posB[0], posB[1], posB[2], n_1) <<((2*N)+1);  */
        tmp2 = L_shl(quant_3p_3N1(posB[0], posB[1], posB[2], n_1), tmp);
        index = L_add(index, tmp2);
        index = L_add(index, quant_2p_2N1(posB[3], posA[0], N));        /* index += quant_2p_2N1(posB[3], posA[0], N); */
        break;
    case 2:
        tmp = sub(extract_l(L_shr(L_mult(5, N), 1)), 1);        /* ((5*N)-1)) */
        index = L_shl(1L, tmp);            /* index = 1 << ((5*N)-1); */
        tmp = add(shl(N, 1), 1);           /* index += quant_3p_3N1(posB[0], posB[1], posB[2], n_1) << ((2*N)+1);  */
        tmp2 = L_shl(quant_3p_3N1(posB[0], posB[1], posB[2], n_1), tmp);
        index = L_add(index, tmp2);
        index = L_add(index, quant_2p_2N1(posA[0], posA[1], N));        /* index += quant_2p_2N1(posA[0], posA[1], N); */
        break;
    case 3:
        tmp = add(shl(N, 1), 1);           /* index = quant_3p_3N1(posA[0], posA[1], posA[2], n_1) << ((2*N)+1);  */
        index = L_shl(quant_3p_3N1(posA[0], posA[1], posA[2], n_1), tmp);
        index = L_add(index, quant_2p_2N1(posB[0], posB[1], N));        /* index += quant_2p_2N1(posB[0], posB[1], N); */
        break;
    case 4:
        tmp = add(shl(N, 1), 1);           /* index = quant_3p_3N1(posA[0], posA[1], posA[2], n_1) << ((2*N)+1);  */
        index = L_shl(quant_3p_3N1(posA[0], posA[1], posA[2], n_1), tmp);
        index = L_add(index, quant_2p_2N1(posA[3], posB[0], N));        /* index += quant_2p_2N1(posA[3], posB[0], N); */
        break;
    case 5:
        tmp = add(shl(N, 1), 1);           /* index = quant_3p_3N1(posA[0], posA[1], posA[2], n_1) << ((2*N)+1);  */
        index = L_shl(quant_3p_3N1(posA[0], posA[1], posA[2], n_1), tmp);
        index = L_add(index, quant_2p_2N1(posA[3], posA[4], N));        /* index += quant_2p_2N1(posA[3], posA[4], N); */
        break;
    default:
        index = 0;
        fprintf(stderr, "Error in function quant_5p_5N\n");
    }

    return (index);
}

void dec_5p_5N(Word32 index, Word16 N, Word16 offset, Word16 pos[])
{
    Word16 j, n_1, tmp;
    Word32 idx;

    /*-------------------------------------------------------*
     * Decode 5 pulses with 5*N bits:                        *
     *-------------------------------------------------------*/

    n_1 = (Word16) (N - 1);                move16();
    j = add(offset, shl(1, n_1));          /* j = offset + (1 << n_1); */
    tmp = add(shl(N, 1), 1);               /* idx = (index >> ((2*N)+1)); */
    idx = L_shr(index, tmp);
    tmp = sub(extract_l(L_shr(L_mult(5, N), 1)), 1);    /* ((5*N)-1)) */

    test();logic16();
    if ((L_shr(index, tmp) & 1) == 0)      /* ((index >> ((5*N)-1)) & 1)  */
    {
        dec_3p_3N1(idx, n_1, offset, pos);
        dec_2p_2N1(index, N, offset, pos + 3);  move16();
    } else
    {
        dec_3p_3N1(idx, n_1, j, pos);
        dec_2p_2N1(index, N, offset, pos + 3);  move16();
    }
    return;
}


Word32 quant_6p_6N_2(                      /* (o) return (6*N)-2 bits         */
     Word16 pos[],                         /* (i) position of the pulse 1..6  */
     Word16 N)                             /* (i) number of bits for position */
{
    Word16 i, j, k, nb_pos, n_1;
    Word16 posA[6], posB[6];
    Word32 index;

    /* !!  N and n_1 are constants -> it doesn't need to be operated by Basic Operators */

    n_1 = (Word16) (N - 1);                move16();
    nb_pos = shl(1, n_1);                  /* nb_pos = (1<<n_1); */

    i = 0;                                 move16();
    j = 0;                                 move16();
    for (k = 0; k < 6; k++)
    {
        test();logic16();
        if ((pos[k] & nb_pos) == 0)
        {
            posA[i++] = pos[k];            move16();
        } else
        {
            posB[j++] = pos[k];            move16();
        }
    }

    switch (i)
    {
    case 0:
        index = L_shl(1L, (Word16) (6 * N - 5));        /* index = 1 << ((6*N)-5); */
        index = L_add(index, L_shl(quant_5p_5N(posB, n_1), N)); /* index += quant_5p_5N(posB, n_1) << N; */
        index = L_add(index, quant_1p_N1(posB[5], n_1));        /* index += quant_1p_N1(posB[5], n_1); */
        break;
    case 1:
        index = L_shl(1L, (Word16) (6 * N - 5));        /* index = 1 << ((6*N)-5); */
        index = L_add(index, L_shl(quant_5p_5N(posB, n_1), N)); /* index += quant_5p_5N(posB, n_1) << N; */
        index = L_add(index, quant_1p_N1(posA[0], n_1));        /* index += quant_1p_N1(posA[0], n_1); */
        break;
    case 2:
        index = L_shl(1L, (Word16) (6 * N - 5));        /* index = 1 << ((6*N)-5); */
        /* index += quant_4p_4N(posB, n_1) << ((2*n_1)+1); */
        index = L_add(index, L_shl(quant_4p_4N(posB, n_1), (Word16) (2 * n_1 + 1)));
        index = L_add(index, quant_2p_2N1(posA[0], posA[1], n_1));      /* index += quant_2p_2N1(posA[0], posA[1], n_1); */
        break;
    case 3:
        index = L_shl(quant_3p_3N1(posA[0], posA[1], posA[2], n_1), (Word16) (3 * n_1 + 1));    /* index = quant_3p_3N1(posA[0], posA[1], posA[2], n_1) << ((3*n_1)+1); */
        index = L_add(index, quant_3p_3N1(posB[0], posB[1], posB[2], n_1));     /* index += quant_3p_3N1(posB[0], posB[1], posB[2], n_1); */
        break;
    case 4:
        i = 2;                             move16();
        index = L_shl(quant_4p_4N(posA, n_1), (Word16) (2 * n_1 + 1));  /* index = quant_4p_4N(posA, n_1) << ((2*n_1)+1); */
        index = L_add(index, quant_2p_2N1(posB[0], posB[1], n_1));      /* index += quant_2p_2N1(posB[0], posB[1], n_1); */
        break;
    case 5:
        i = 1;                             move16();
        index = L_shl(quant_5p_5N(posA, n_1), N);       /* index = quant_5p_5N(posA, n_1) << N; */
        index = L_add(index, quant_1p_N1(posB[0], n_1));        /* index += quant_1p_N1(posB[0], n_1); */
        break;
    case 6:
        i = 0;                             move16();
        index = L_shl(quant_5p_5N(posA, n_1), N);       /* index = quant_5p_5N(posA, n_1) << N; */
        index = L_add(index, quant_1p_N1(posA[5], n_1));        /* index += quant_1p_N1(posA[5], n_1); */
        break;
    default:
        index = 0;
        fprintf(stderr, "Error in function quant_6p_6N_2\n");
    }
    index = L_add(index, L_shl((L_deposit_l(i) & 3L), (Word16) (6 * N - 4)));   logic16();/* index += (i & 3) << ((6*N)-4); */

    return (index);
}

void dec_6p_6N_2(Word32 index, Word16 N, Word16 offset, Word16 pos[])
{
    Word16 j, n_1, offsetA, offsetB;

    n_1 = (Word16) (N - 1);                move16();
    j = add(offset, shl(1, n_1));          /* j = offset + (1 << n_1); */


    /* !!  N and n_1 are constants -> it doesn't need to be operated by Basic Operators */

    offsetA = offsetB = j;                 move16();move16();
    test();logic16();
    if ((L_shr(index, (Word16) (6 * N - 5)) & 1L) == 0)
    {                                      /* if (((index >> ((6*N)-5)) & 1) == 0) */
        offsetA = offset;                  move16();
    } else
    {
        offsetB = offset;                  move16();
    }

    test();logic16();
    switch (L_shr(index, (Word16) (6 * N - 4)) & 3)
    {                                      /* (index >> ((6*N)-4)) & 3 */
    case 0:
        dec_5p_5N(L_shr(index, N), n_1, offsetA, pos);  /* dec_5p_5N(index>>N, n_1, offsetA, pos); */
        dec_1p_N1(index, n_1, offsetA, pos + 5);        move16();
        break;
    case 1:
        dec_5p_5N(L_shr(index, N), n_1, offsetA, pos);  /* dec_5p_5N(index>>N, n_1, offsetA, pos); */
        dec_1p_N1(index, n_1, offsetB, pos + 5);        move16();
        break;
    case 2:
        dec_4p_4N(L_shr(index, (Word16) (2 * n_1 + 1)), n_1, offsetA, pos); /* dec_4p_4N(index>>((2*n_1)+1 ), n_1, offsetA, pos); */
        dec_2p_2N1(index, n_1, offsetB, pos + 4);       move16();
        break;
    case 3:
        dec_3p_3N1(L_shr(index, (Word16) (3 * n_1 + 1)), n_1, offset, pos); /* dec_3p_3N1(index>>((3*n_1)+ 1), n_1, offset, pos); */
        dec_3p_3N1(index, n_1, j, pos + 3);move16();
        break;
    }
    return;
}
