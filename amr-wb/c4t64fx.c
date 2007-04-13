/*------------------------------------------------------------------------*
 *                         C4T64FX.C                                      *
 *------------------------------------------------------------------------*
 * Performs algebraic codebook search for higher modes                    *
 *------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------*
 * Function  ACELP_4t64_fx()                                             *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~                                             *
 * 20, 36, 44, 52, 64, 72, 88 bits algebraic codebook.                   *
 * 4 tracks x 16 positions per track = 64 samples.                       *
 *                                                                       *
 * 20 bits --> 4 pulses in a frame of 64 samples.                        *
 * 36 bits --> 8 pulses in a frame of 64 samples.                        *
 * 44 bits --> 10 pulses in a frame of 64 samples.                       *
 * 52 bits --> 12 pulses in a frame of 64 samples.                       *
 * 64 bits --> 16 pulses in a frame of 64 samples.                       *
 * 72 bits --> 18 pulses in a frame of 64 samples.                       *
 * 88 bits --> 24 pulses in a frame of 64 samples.                       *
 *                                                                       *
 * All pulses can have two (2) possible amplitudes: +1 or -1.            *
 * Each pulse can have sixteen (16) possible positions.                  *
 *-----------------------------------------------------------------------*/

#include "typedef.h"
#include "basic_op.h"
#include "math_op.h"
#include "acelp.h"
#include "count.h"
#include "cnst.h"

#include "q_pulse.h"

static Word16 tipos[36] = {
    0, 1, 2, 3,                            /* starting point &ipos[0], 1st iter */
    1, 2, 3, 0,                            /* starting point &ipos[4], 2nd iter */
    2, 3, 0, 1,                            /* starting point &ipos[8], 3rd iter */
    3, 0, 1, 2,                            /* starting point &ipos[12], 4th iter */
    0, 1, 2, 3,
    1, 2, 3, 0,
    2, 3, 0, 1,
    3, 0, 1, 2,
    0, 1, 2, 3};                           /* end point for 24 pulses &ipos[35], 4th iter */

#define NB_PULSE_MAX  24

#define L_SUBFR   64
#define NB_TRACK  4
#define STEP      4
#define NB_POS    16
#define MSIZE     256
#define NB_MAX    8
#define NPMAXPT   ((NB_PULSE_MAX+NB_TRACK-1)/NB_TRACK)


/* locals functions */

static void cor_h_vec(
     Word16 h[],                           /* (i) scaled impulse response                 */
     Word16 vec[],                         /* (i) scaled vector (/8) to correlate with h[] */
     Word16 track,                         /* (i) track to use                            */
     Word16 sign[],                        /* (i) sign vector                             */
     Word16 rrixix[][NB_POS],              /* (i) correlation of h[x] with h[x]      */
     Word16 cor[]                          /* (o) result of correlation (NB_POS elements) */
);
static void search_ixiy(
     Word16 nb_pos_ix,                     /* (i) nb of pos for pulse 1 (1..8)       */
     Word16 track_x,                       /* (i) track of pulse 1                   */
     Word16 track_y,                       /* (i) track of pulse 2                   */
     Word16 * ps,                          /* (i/o) correlation of all fixed pulses  */
     Word16 * alp,                         /* (i/o) energy of all fixed pulses       */
     Word16 * ix,                          /* (o) position of pulse 1                */
     Word16 * iy,                          /* (o) position of pulse 2                */
     Word16 dn[],                          /* (i) corr. between target and h[]       */
     Word16 dn2[],                         /* (i) vector of selected positions       */
     Word16 cor_x[],                       /* (i) corr. of pulse 1 with fixed pulses */
     Word16 cor_y[],                       /* (i) corr. of pulse 2 with fixed pulses */
     Word16 rrixiy[][MSIZE]                /* (i) corr. of pulse 1 with pulse 2   */
);


void ACELP_4t64_fx(
     Word16 dn[],                          /* (i) <12b : correlation between target x[] and H[]      */
     Word16 cn[],                          /* (i) <12b : residual after long term prediction         */
     Word16 H[],                           /* (i) Q12: impulse response of weighted synthesis filter */
     Word16 code[],                        /* (o) Q9 : algebraic (fixed) codebook excitation         */
     Word16 y[],                           /* (o) Q9 : filtered fixed codebook excitation            */
     Word16 nbbits,                        /* (i) : 20, 36, 44, 52, 64, 72 or 88 bits                */
     Word16 ser_size,                      /* (i) : bit rate                                         */
     Word16 _index[]                       /* (o) : index (20): 5+5+5+5 = 20 bits.                   */
                                           /* (o) : index (36): 9+9+9+9 = 36 bits.                   */
                                           /* (o) : index (44): 13+9+13+9 = 44 bits.                 */
                                           /* (o) : index (52): 13+13+13+13 = 52 bits.               */
                                           /* (o) : index (64): 2+2+2+2+14+14+14+14 = 64 bits.       */
                                           /* (o) : index (72): 10+2+10+2+10+14+10+14 = 72 bits.     */
                                           /* (o) : index (88): 11+11+11+11+11+11+11+11 = 88 bits.   */
)
{
    Word16 i, j, k, st, ix = 0, iy = 0, pos, index, track, nb_pulse, nbiter;
    Word16 psk, ps, alpk, alp, val, k_cn, k_dn, exp;
    Word16 *p0, *p1, *p2, *p3, *psign;
    Word16 *h, *h_inv, *ptr_h1, *ptr_h2, *ptr_hf, h_shift;
    Word32 s, cor, L_tmp, L_index;

    Word16 dn2[L_SUBFR], sign[L_SUBFR], vec[L_SUBFR];
    Word16 ind[NPMAXPT * NB_TRACK];
    Word16 codvec[NB_PULSE_MAX], nbpos[10];
    Word16 cor_x[NB_POS], cor_y[NB_POS], pos_max[NB_TRACK];
    Word16 h_buf[4 * L_SUBFR];
    Word16 rrixix[NB_TRACK][NB_POS], rrixiy[NB_TRACK][MSIZE];
    Word16 ipos[NB_PULSE_MAX];

    switch (nbbits)
    {
    case 20:                               /* 20 bits, 4 pulses, 4 tracks */
        nbiter = 4;                        move16();  /* 4x16x16=1024 loop */
        alp = 8192;                        move16();  /* alp = 2.0 (Q12) */
        nb_pulse = 4;                      move16();
        nbpos[0] = 4;                      move16();
        nbpos[1] = 8;                      move16();
        break;
    case 36:                               /* 36 bits, 8 pulses, 4 tracks */
        nbiter = 4;                        move16();  /* 4x20x16=1280 loop */
        alp = 4096;                        move16();  /* alp = 1.0 (Q12) */
        nb_pulse = 8;                      move16();
        nbpos[0] = 4;                      move16();
        nbpos[1] = 8;                      move16();
        nbpos[2] = 8;                      move16();
        break;
    case 44:                               /* 44 bits, 10 pulses, 4 tracks */
        nbiter = 4;                        move16();  /* 4x26x16=1664 loop */
        alp = 4096;                        move16();  /* alp = 1.0 (Q12) */
        nb_pulse = 10;                     move16();
        nbpos[0] = 4;                      move16();
        nbpos[1] = 6;                      move16();
        nbpos[2] = 8;                      move16();
        nbpos[3] = 8;                      move16();
        break;
    case 52:                               /* 52 bits, 12 pulses, 4 tracks */
        nbiter = 4;                        move16();  /* 4x26x16=1664 loop */
        alp = 4096;                        move16();  /* alp = 1.0 (Q12) */
        nb_pulse = 12;                     move16();
        nbpos[0] = 4;                      move16();
        nbpos[1] = 6;                      move16();
        nbpos[2] = 8;                      move16();
        nbpos[3] = 8;                      move16();
        break;
    case 64:                               /* 64 bits, 16 pulses, 4 tracks */
        nbiter = 3;                        move16();  /* 3x36x16=1728 loop */
        alp = 3277;                        move16();  /* alp = 0.8 (Q12) */
        nb_pulse = 16;                     move16();
        nbpos[0] = 4;                      move16();
        nbpos[1] = 4;                      move16();
        nbpos[2] = 6;                      move16();
        nbpos[3] = 6;                      move16();
        nbpos[4] = 8;                      move16();
        nbpos[5] = 8;                      move16();
        break;
    case 72:                               /* 72 bits, 18 pulses, 4 tracks */
        nbiter = 3;                        move16();  /* 3x35x16=1680 loop */
        alp = 3072;                        move16();  /* alp = 0.75 (Q12) */
        nb_pulse = 18;                     move16();
        nbpos[0] = 2;                      move16();
        nbpos[1] = 3;                      move16();
        nbpos[2] = 4;                      move16();
        nbpos[3] = 5;                      move16();
        nbpos[4] = 6;                      move16();
        nbpos[5] = 7;                      move16();
        nbpos[6] = 8;                      move16();
        break;
    case 88:                               /* 88 bits, 24 pulses, 4 tracks */
        test();move16();
        if (sub(ser_size, 462) > 0)
            nbiter = 1;
        else
            nbiter = 2;                    /* 2x53x16=1696 loop */

        alp = 2048;                        move16();  /* alp = 0.5 (Q12) */
        nb_pulse = 24;                     move16();
        nbpos[0] = 2;                      move16();
        nbpos[1] = 2;                      move16();
        nbpos[2] = 3;                      move16();
        nbpos[3] = 4;                      move16();
        nbpos[4] = 5;                      move16();
        nbpos[5] = 6;                      move16();
        nbpos[6] = 7;                      move16();
        nbpos[7] = 8;                      move16();
        nbpos[8] = 8;                      move16();
        nbpos[9] = 8;                      move16();
        break;
    default:
        nbiter = 0;
        alp = 0;
        nb_pulse = 0;
    }

    for (i = 0; i < nb_pulse; i++)
    {
        codvec[i] = i;                     move16();
    }

    /*----------------------------------------------------------------*
     * Find sign for each pulse position.                             *
     *----------------------------------------------------------------*/

    /* calculate energy for normalization of cn[] and dn[] */

    /* set k_cn = 32..32767 (ener_cn = 2^30..256-0) */
    s = Dot_product12(cn, cn, L_SUBFR, &exp);
    Isqrt_n(&s, &exp);
    s = L_shl(s, add(exp, 5));             /* saturation can occur here */
    k_cn = round(s);

    /* set k_dn = 32..512 (ener_dn = 2^30..2^22) */
    s = Dot_product12(dn, dn, L_SUBFR, &exp);
    Isqrt_n(&s, &exp);
    k_dn = round(L_shl(s, add(exp, 5 + 3)));    /* k_dn = 256..4096 */
    k_dn = mult_r(alp, k_dn);              /* alp in Q12 */

    /* mix normalized cn[] and dn[] */
    for (i = 0; i < L_SUBFR; i++)
    {
        s = L_mac(L_mult(k_cn, cn[i]), k_dn, dn[i]);
        dn2[i] = extract_h(L_shl(s, 8));   move16();
    }

    /* set sign according to dn2[] = k_cn*cn[] + k_dn*dn[]    */

    for (k = 0; k < NB_TRACK; k++)
    {
        for (i = k; i < L_SUBFR; i += STEP)
        {
            val = dn[i];                   move16();
            ps = dn2[i];                   move16();

            test();
            if (ps >= 0)
            {
                sign[i] = 32767;           move16();  /* sign = +1 (Q12) */
                vec[i] = -32768;           move16();
            } else
            {
                sign[i] = -32768;          move16();  /* sign = -1 (Q12) */
                vec[i] = 32767;            move16();
                val = negate(val);
                ps = negate(ps);
            }
            dn[i] = val;                   move16();  /* modify dn[] according to the fixed sign */
            dn2[i] = ps;                   move16();  /* dn2[] = mix of dn[] and cn[]            */
        }
    }

    /*----------------------------------------------------------------*
     * Select NB_MAX position per track according to max of dn2[].    *
     *----------------------------------------------------------------*/

    pos = 0;
    for (i = 0; i < NB_TRACK; i++)
    {
        for (k = 0; k < NB_MAX; k++)
        {
            ps = -1;                       move16();
            for (j = i; j < L_SUBFR; j += STEP)
            {
                test();
                if (sub(dn2[j], ps) > 0)
                {
                    ps = dn2[j];           move16();
                    pos = j;               move16();
                }
            }
            move16();
            dn2[pos] = sub(k, NB_MAX);     /* dn2 < 0 when position is selected */
            test();
            if (k == 0)
            {
                pos_max[i] = pos;          move16();
            }
        }
    }

    /*--------------------------------------------------------------*
     * Scale h[] to avoid overflow and to get maximum of precision  *
     * on correlation.                                              *
     *                                                              *
     * Maximum of h[] (h[0]) is fixed to 2048 (MAX16 / 16).         *
     *  ==> This allow addition of 16 pulses without saturation.    *
     *                                                              *
     * Energy worst case (on resonant impulse response),            *
     * - energy of h[] is approximately MAX/16.                     *
     * - During search, the energy is divided by 8 to avoid         *
     *   overflow on "alp". (energy of h[] = MAX/128).              *
     *  ==> "alp" worst case detected is 22854 on sinusoidal wave.  *
     *--------------------------------------------------------------*/

    /* impulse response buffer for fast computation */

    h = h_buf;                             move16();
    h_inv = h_buf + (2 * L_SUBFR);         move16();
    for (i = 0; i < L_SUBFR; i++)
    {
        *h++ = 0;                          move16();
        *h_inv++ = 0;                      move16();
    }

    /* scale h[] down (/2) when energy of h[] is high with many pulses used */
    L_tmp = 0;
    for (i = 0; i < L_SUBFR; i++)
        L_tmp = L_mac(L_tmp, H[i], H[i]);
    val = extract_h(L_tmp);

    h_shift = 0;                           move16();

    test();test();
    if ((sub(nb_pulse, 12) >= 0) && (sub(val, 1024) > 0))
    {
        h_shift = 1;                       move16();
    }
    for (i = 0; i < L_SUBFR; i++)
    {
        h[i] = shr(H[i], h_shift);         move16();
        h_inv[i] = negate(h[i]);           move16();
    }

    /*------------------------------------------------------------*
     * Compute rrixix[][] needed for the codebook search.         *
     * This algorithm compute impulse response energy of all      *
     * positions (16) in each track (4).       Total = 4x16 = 64. *
     *------------------------------------------------------------*/

    /* storage order --> i3i3, i2i2, i1i1, i0i0 */

    /* Init pointers to last position of rrixix[] */
    p0 = &rrixix[0][NB_POS - 1];           move16();
    p1 = &rrixix[1][NB_POS - 1];           move16();
    p2 = &rrixix[2][NB_POS - 1];           move16();
    p3 = &rrixix[3][NB_POS - 1];           move16();

    ptr_h1 = h;                            move16();
    cor = 0x00008000L;                     move32();  /* for rounding */
    for (i = 0; i < NB_POS; i++)
    {
        cor = L_mac(cor, *ptr_h1, *ptr_h1);
        ptr_h1++;
        *p3-- = extract_h(cor);            move16();
        cor = L_mac(cor, *ptr_h1, *ptr_h1);
        ptr_h1++;
        *p2-- = extract_h(cor);            move16();
        cor = L_mac(cor, *ptr_h1, *ptr_h1);
        ptr_h1++;
        *p1-- = extract_h(cor);            move16();
        cor = L_mac(cor, *ptr_h1, *ptr_h1);
        ptr_h1++;
        *p0-- = extract_h(cor);            move16();
    }

    /*------------------------------------------------------------*
     * Compute rrixiy[][] needed for the codebook search.         *
     * This algorithm compute correlation between 2 pulses        *
     * (2 impulses responses) in 4 possible adjacents tracks.     *
     * (track 0-1, 1-2, 2-3 and 3-0).     Total = 4x16x16 = 1024. *
     *------------------------------------------------------------*/

    /* storage order --> i2i3, i1i2, i0i1, i3i0 */

    pos = MSIZE - 1;                       move16();
    ptr_hf = h + 1;                        move16();

    for (k = 0; k < NB_POS; k++)
    {
        p3 = &rrixiy[2][pos];              move16();
        p2 = &rrixiy[1][pos];              move16();
        p1 = &rrixiy[0][pos];              move16();
        p0 = &rrixiy[3][pos - NB_POS];     move16();

        cor = 0x00008000L;                 move32();  /* for rounding */
        ptr_h1 = h;                        move16();
        ptr_h2 = ptr_hf;                   move16();

        for (i = add(k, 1); i < NB_POS; i++)
        {
            cor = L_mac(cor, *ptr_h1, *ptr_h2);
            ptr_h1++;
            ptr_h2++;
            *p3 = extract_h(cor);          move16();
            cor = L_mac(cor, *ptr_h1, *ptr_h2);
            ptr_h1++;
            ptr_h2++;
            *p2 = extract_h(cor);          move16();
            cor = L_mac(cor, *ptr_h1, *ptr_h2);
            ptr_h1++;
            ptr_h2++;
            *p1 = extract_h(cor);          move16();
            cor = L_mac(cor, *ptr_h1, *ptr_h2);
            ptr_h1++;
            ptr_h2++;
            *p0 = extract_h(cor);          move16();

            p3 -= (NB_POS + 1);
            p2 -= (NB_POS + 1);
            p1 -= (NB_POS + 1);
            p0 -= (NB_POS + 1);
        }
        cor = L_mac(cor, *ptr_h1, *ptr_h2);
        ptr_h1++;
        ptr_h2++;
        *p3 = extract_h(cor);              move16();
        cor = L_mac(cor, *ptr_h1, *ptr_h2);
        ptr_h1++;
        ptr_h2++;
        *p2 = extract_h(cor);              move16();
        cor = L_mac(cor, *ptr_h1, *ptr_h2);
        ptr_h1++;
        ptr_h2++;
        *p1 = extract_h(cor);              move16();

        pos -= NB_POS;
        ptr_hf += STEP;
    }

    /* storage order --> i3i0, i2i3, i1i2, i0i1 */

    pos = MSIZE - 1;                       move16();
    ptr_hf = h + 3;                        move16();

    for (k = 0; k < NB_POS; k++)
    {
        p3 = &rrixiy[3][pos];              move16();
        p2 = &rrixiy[2][pos - 1];          move16();
        p1 = &rrixiy[1][pos - 1];          move16();
        p0 = &rrixiy[0][pos - 1];          move16();

        cor = 0x00008000L;                 move32();  /* for rounding */
        ptr_h1 = h;                        move16();
        ptr_h2 = ptr_hf;                   move16();

        for (i = add(k, 1); i < NB_POS; i++)
        {
            cor = L_mac(cor, *ptr_h1, *ptr_h2);
            ptr_h1++;
            ptr_h2++;
            *p3 = extract_h(cor);          move16();
            cor = L_mac(cor, *ptr_h1, *ptr_h2);
            ptr_h1++;
            ptr_h2++;
            *p2 = extract_h(cor);          move16();
            cor = L_mac(cor, *ptr_h1, *ptr_h2);
            ptr_h1++;
            ptr_h2++;
            *p1 = extract_h(cor);          move16();
            cor = L_mac(cor, *ptr_h1, *ptr_h2);
            ptr_h1++;
            ptr_h2++;
            *p0 = extract_h(cor);          move16();

            p3 -= (NB_POS + 1);
            p2 -= (NB_POS + 1);
            p1 -= (NB_POS + 1);
            p0 -= (NB_POS + 1);
        }
        cor = L_mac(cor, *ptr_h1, *ptr_h2);
        ptr_h1++;
        ptr_h2++;
        *p3 = extract_h(cor);              move16();

        pos--;
        ptr_hf += STEP;
    }

    /*------------------------------------------------------------*
     * Modification of rrixiy[][] to take signs into account.     *
     *------------------------------------------------------------*/

    p0 = &rrixiy[0][0];                    move16();

    for (k = 0; k < NB_TRACK; k++)
    {
        for (i = k; i < L_SUBFR; i += STEP)
        {
            psign = sign;                  move16();
            test();
            if (psign[i] < 0)
            {
                psign = vec;               move16();
            }
            for (j = (Word16) ((k + 1) % NB_TRACK); j < L_SUBFR; j += STEP)
            {
                *p0 = mult(*p0, psign[j]);    move16();
                p0++;
            }
        }
    }

    /*-------------------------------------------------------------------*
     *                       Deep first search                           *
     *-------------------------------------------------------------------*/

    psk = -1;                              move16();
    alpk = 1;                              move16();

    for (k = 0; k < nbiter; k++)
    {
        for (i = 0; i < nb_pulse; i++)
            ipos[i] = tipos[(k * 4) + i];

        test();test();test();
        if (sub(nbbits, 20) == 0)
        {
            pos = 0;                       move16();
            ps = 0;                        move16();
            alp = 0;                       move16();
            for (i = 0; i < L_SUBFR; i++)
            {
                vec[i] = 0;                move16();
            }
        } else if ((sub(nbbits, 36) == 0) || (sub(nbbits, 44) == 0))
        {
            /* first stage: fix 2 pulses */
            pos = 2;

            ix = ind[0] = pos_max[ipos[0]];move16();move16();
            iy = ind[1] = pos_max[ipos[1]];move16();move16();
            ps = add(dn[ix], dn[iy]);
            i = shr(ix, 2);                /* ix / STEP */
            j = shr(iy, 2);                /* iy / STEP */
            s = L_mult(rrixix[ipos[0]][i], 4096);
            s = L_mac(s, rrixix[ipos[1]][j], 4096);
            i = add(shl(i, 4), j);         /* (ix/STEP)*NB_POS + (iy/STEP) */
            s = L_mac(s, rrixiy[ipos[0]][i], 8192);
            alp = round(s);
            test();move16();move16();
            if (sign[ix] < 0)
                p0 = h_inv - ix;
            else
                p0 = h - ix;
            test();move16();move16();
            if (sign[iy] < 0)
                p1 = h_inv - iy;
            else
                p1 = h - iy;

            for (i = 0; i < L_SUBFR; i++)
            {
                vec[i] = add(*p0++, *p1++);move16();
            }

            test();
            if (sub(nbbits, 44) == 0)
            {
                ipos[8] = 0;               move16();
                ipos[9] = 1;               move16();
            }
        } else
        {
            /* first stage: fix 4 pulses */
            pos = 4;

            ix = ind[0] = pos_max[ipos[0]];  move16();move16();
            iy = ind[1] = pos_max[ipos[1]];  move16();move16();
            i = ind[2] = pos_max[ipos[2]];   move16();move16();
            j = ind[3] = pos_max[ipos[3]];   move16();move16();
            ps = add(add(add(dn[ix], dn[iy]), dn[i]), dn[j]);

            test();move16();move16();
            if (sign[ix] < 0)
                p0 = h_inv - ix;
            else
                p0 = h - ix;
            test();move16();move16();
            if (sign[iy] < 0)
                p1 = h_inv - iy;
            else
                p1 = h - iy;
            test();move16();move16();
            if (sign[i] < 0)
                p2 = h_inv - i;
            else
                p2 = h - i;
            test();move16();move16();
            if (sign[j] < 0)
                p3 = h_inv - j;
            else
                p3 = h - j;

            for (i = 0; i < L_SUBFR; i++)
            {
                vec[i] = add(add(add(*p0++, *p1++), *p2++), *p3++);
                move16();
            }

            L_tmp = 0L;                    move32();
            for (i = 0; i < L_SUBFR; i++)
                L_tmp = L_mac(L_tmp, vec[i], vec[i]);

            alp = round(L_shr(L_tmp, 3));

            if (sub(nbbits, 72) == 0)
            {
                ipos[16] = 0;              move16();
                ipos[17] = 1;              move16();
            }
        }

        /* other stages of 2 pulses */

        for (j = pos, st = 0; j < nb_pulse; j += 2, st++)
        {
            /*--------------------------------------------------*
            * Calculate correlation of all possible positions  *
            * of the next 2 pulses with previous fixed pulses. *
            * Each pulse can have 16 possible positions.       *
            *--------------------------------------------------*/

            cor_h_vec(h, vec, ipos[j], sign, rrixix, cor_x);
            cor_h_vec(h, vec, ipos[j + 1], sign, rrixix, cor_y);

            /*--------------------------------------------------*
            * Find best positions of 2 pulses.                 *
            *--------------------------------------------------*/

            search_ixiy(nbpos[st], ipos[j], ipos[j + 1], &ps, &alp,
                &ix, &iy, dn, dn2, cor_x, cor_y, rrixiy);

            ind[j] = ix;                   move16();
            ind[j + 1] = iy;               move16();

            test();move16();move16();
            if (sign[ix] < 0)
                p0 = h_inv - ix;
            else
                p0 = h - ix;
            test();move16();move16();
            if (sign[iy] < 0)
                p1 = h_inv - iy;
            else
                p1 = h - iy;

            for (i = 0; i < L_SUBFR; i++)
            {
                vec[i] = add(vec[i], add(*p0++, *p1++));        /* can saturate here. */
                move16();
            }
        }

        /* memorise the best codevector */

        ps = mult(ps, ps);
        s = L_msu(L_mult(alpk, ps), psk, alp);
        test();
        if (s > 0)
        {
            psk = ps;                      move16();
            alpk = alp;                    move16();
            for (i = 0; i < nb_pulse; i++)
            {
                codvec[i] = ind[i];        move16();
            }
            for (i = 0; i < L_SUBFR; i++)
            {
                y[i] = vec[i];             move16();
            }
        }
    }

    /*-------------------------------------------------------------------*
     * Build the codeword, the filtered codeword and index of codevector.*
     *-------------------------------------------------------------------*/

    for (i = 0; i < NPMAXPT * NB_TRACK; i++)
    {
        ind[i] = -1;                       move16();
    }
    for (i = 0; i < L_SUBFR; i++)
    {
        code[i] = 0;                       move16();
        y[i] = shr_r(y[i], 3);             move16();  /* Q12 to Q9 */
    }

    val = shr(512, h_shift);               /* codeword in Q9 format */

    for (k = 0; k < nb_pulse; k++)
    {
        i = codvec[k];                     move16();  /* read pulse position */
        j = sign[i];                       move16();  /* read sign           */

        index = shr(i, 2);                 /* index = pos of pulse (0..15) */
        track = (Word16) (i & 0x03);       logic16();  /* track = i % NB_TRACK (0..3)  */

        if (j > 0)
        {
            code[i] = add(code[i], val);   move16();
            codvec[k] = add(codvec[k], (2 * L_SUBFR));  move16();
        } else
        {
            code[i] = sub(code[i], val);   move16();
            index = add(index, NB_POS);    move16();
        }

        i = extract_l(L_shr(L_mult(track, NPMAXPT), 1));

        test();move16();
        while (ind[i] >= 0)
        {
            i = add(i, 1);
        }
        ind[i] = index;                    move16();
    }

    k = 0;                                 move16();
    /* Build index of codevector */
    test();test();test();test();test();test();test();
    if (sub(nbbits, 20) == 0)
    {
        for (track = 0; track < NB_TRACK; track++)
        {
            _index[track] = extract_l(quant_1p_N1(ind[k], 4));
            k += NPMAXPT;
        }
    } else if (sub(nbbits, 36) == 0)
    {
        for (track = 0; track < NB_TRACK; track++)
        {
            _index[track] = extract_l(quant_2p_2N1(ind[k], ind[k + 1], 4));
            k += NPMAXPT;
        }
    } else if (sub(nbbits, 44) == 0)
    {
        for (track = 0; track < NB_TRACK - 2; track++)
        {
            _index[track] = extract_l(quant_3p_3N1(ind[k], ind[k + 1], ind[k + 2], 4));
            k += NPMAXPT;
        }
        for (track = 2; track < NB_TRACK; track++)
        {
            _index[track] = extract_l(quant_2p_2N1(ind[k], ind[k + 1], 4));
            k += NPMAXPT;
        }
    } else if (sub(nbbits, 52) == 0)
    {
        for (track = 0; track < NB_TRACK; track++)
        {
            _index[track] = extract_l(quant_3p_3N1(ind[k], ind[k + 1], ind[k + 2], 4));
            k += NPMAXPT;
        }
    } else if (sub(nbbits, 64) == 0)
    {
        for (track = 0; track < NB_TRACK; track++)
        {
            L_index = quant_4p_4N(&ind[k], 4);
            _index[track] = extract_l(L_shr(L_index, 14) & 3);
            _index[track + NB_TRACK] = extract_l(L_index & 0x3FFF);
            k += NPMAXPT;
        }
    } else if (sub(nbbits, 72) == 0)
    {
        for (track = 0; track < NB_TRACK - 2; track++)
        {
            L_index = quant_5p_5N(&ind[k], 4);
            _index[track] = extract_l(L_shr(L_index, 10) & 0x03FF);
            _index[track + NB_TRACK] = extract_l(L_index & 0x03FF);
            k += NPMAXPT;
        }
        for (track = 2; track < NB_TRACK; track++)
        {
            L_index = quant_4p_4N(&ind[k], 4);
            _index[track] = extract_l(L_shr(L_index, 14) & 3);
            _index[track + NB_TRACK] = extract_l(L_index & 0x3FFF);
            k += NPMAXPT;
        }
    } else if (sub(nbbits, 88) == 0)
    {
        for (track = 0; track < NB_TRACK; track++)
        {
            L_index = quant_6p_6N_2(&ind[k], 4);
            _index[track] = extract_l(L_shr(L_index, 11) & 0x07FF);
            _index[track + NB_TRACK] = extract_l(L_index & 0x07FF);
            k += NPMAXPT;
        }
    }
    return;
}


/*-------------------------------------------------------------------*
 * Function  cor_h_vec()                                             *
 * ~~~~~~~~~~~~~~~~~~~~~                                             *
 * Compute correlations of h[] with vec[] for the specified track.   *
 *-------------------------------------------------------------------*/
static void cor_h_vec(
     Word16 h[],                           /* (i) scaled impulse response                 */
     Word16 vec[],                         /* (i) scaled vector (/8) to correlate with h[] */
     Word16 track,                         /* (i) track to use                            */
     Word16 sign[],                        /* (i) sign vector                             */
     Word16 rrixix[][NB_POS],              /* (i) correlation of h[x] with h[x]      */
     Word16 cor[]                          /* (o) result of correlation (NB_POS elements) */
)
{
    Word16 i, j, pos, corr;
    Word16 *p0, *p1, *p2;
    Word32 L_sum;

    p0 = rrixix[track];                    move16();

    pos = track;                           move16();
    for (i = 0; i < NB_POS; i++, pos += STEP)
    {
        L_sum = 0L;                        move32();
        p1 = h;                            move16();
        p2 = &vec[pos];                    move16();
        for (j = pos; j < L_SUBFR; j++)
            L_sum = L_mac(L_sum, *p1++, *p2++);

        L_sum = L_shl(L_sum, 1);

        corr = round(L_sum);

        cor[i] = add(mult(corr, sign[pos]), *p0++);     move16();

    }

    return;
}


/*-------------------------------------------------------------------*
 * Function  search_ixiy()                                           *
 * ~~~~~~~~~~~~~~~~~~~~~~~                                           *
 * Find the best positions of 2 pulses in a subframe.                *
 *-------------------------------------------------------------------*/

static void search_ixiy(
     Word16 nb_pos_ix,                     /* (i) nb of pos for pulse 1 (1..8)       */
     Word16 track_x,                       /* (i) track of pulse 1                   */
     Word16 track_y,                       /* (i) track of pulse 2                   */
     Word16 * ps,                          /* (i/o) correlation of all fixed pulses  */
     Word16 * alp,                         /* (i/o) energy of all fixed pulses       */
     Word16 * ix,                          /* (o) position of pulse 1                */
     Word16 * iy,                          /* (o) position of pulse 2                */
     Word16 dn[],                          /* (i) corr. between target and h[]       */
     Word16 dn2[],                         /* (i) vector of selected positions       */
     Word16 cor_x[],                       /* (i) corr. of pulse 1 with fixed pulses */
     Word16 cor_y[],                       /* (i) corr. of pulse 2 with fixed pulses */
     Word16 rrixiy[][MSIZE]                /* (i) corr. of pulse 1 with pulse 2   */
)
{
    Word16 x, y, pos, thres_ix;
    Word16 ps1, ps2, sq, sqk;
    Word16 alp_16, alpk;
    Word16 *p0, *p1, *p2;
    Word32 s, alp0, alp1, alp2;

    p0 = cor_x;                            move16();
    p1 = cor_y;                            move16();
    p2 = rrixiy[track_x];                  move16();

    thres_ix = sub(nb_pos_ix, NB_MAX);

    alp0 = L_deposit_h(*alp);
    alp0 = L_add(alp0, 0x00008000L);       /* for rounding */

    sqk = -1;                              move16();
    alpk = 1;                              move16();

    for (x = track_x; x < L_SUBFR; x += STEP)
    {
        ps1 = add(*ps, dn[x]);
        alp1 = L_mac(alp0, *p0++, 4096);

        test();
        if (sub(dn2[x], thres_ix) < 0)
        {
            pos = -1;                      move16();
            for (y = track_y; y < L_SUBFR; y += STEP)
            {
                ps2 = add(ps1, dn[y]);
                alp2 = L_mac(alp1, *p1++, 4096);
                alp2 = L_mac(alp2, *p2++, 8192);
                alp_16 = extract_h(alp2);

                sq = mult(ps2, ps2);

                s = L_msu(L_mult(alpk, sq), sqk, alp_16);

                test();
                if (s > 0)
                {
                    sqk = sq;              move16();
                    alpk = alp_16;         move16();
                    pos = y;               move16();
                }
            }
            p1 -= NB_POS;

            test();
            if (pos >= 0)
            {
                *ix = x;                   move16();
                *iy = pos;                 move16();
            }
        } else
        {
            p2 += NB_POS;
        }
    }

    *ps = add(*ps, add(dn[*ix], dn[*iy])); move16();
    *alp = alpk;                           move16();

    return;
}
