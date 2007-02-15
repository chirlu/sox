/*-------------------------------------------------------------------*
 *                         DTX.C                                     *
 *-------------------------------------------------------------------*
 * DTX functions                                                     *
 *-------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include "typedef.h"
#include "basic_op.h"
#include "oper_32b.h"
#include "math_op.h"
#include "cnst.h"
#include "acelp.h"                         /* prototype of functions    */
#include "bits.h"
#include "dtx.h"
#include "count.h"
#include "log2.h"

static void aver_isf_history(
     Word16 isf_old[],
     Word16 indices[],
     Word32 isf_aver[]
);
static void find_frame_indices(
     Word16 isf_old_tx[],
     Word16 indices[],
     dtx_encState * st
);

static Word16 dithering_control(
     dtx_encState * st
);
static void CN_dithering(
     Word16 isf[M],
     Word32 * L_log_en_int,
     Word16 * dither_seed
);

/* excitation energy adjustment depending on speech coder mode used, Q7 */
static Word16 en_adjust[9] =
{
    230,                                   /* mode0 = 7k  :  -5.4dB  */
    179,                                   /* mode1 = 9k  :  -4.2dB  */
    141,                                   /* mode2 = 12k :  -3.3dB  */
    128,                                   /* mode3 = 14k :  -3.0dB  */
    122,                                   /* mode4 = 16k :  -2.85dB */
    115,                                   /* mode5 = 18k :  -2.7dB  */
    115,                                   /* mode6 = 20k :  -2.7dB  */
    115,                                   /* mode7 = 23k :  -2.7dB  */
    115                                    /* mode8 = 24k :  -2.7dB  */
};

/**************************************************************************
 *
 *
 * Function    : dtx_enc_init
 *
 *
 **************************************************************************/
Word16 dtx_enc_init(dtx_encState ** st, Word16 isf_init[])
{
    dtx_encState *s;

    test();
    if (st == (dtx_encState **) NULL)
    {
        fprintf(stderr, "dtx_enc_init: invalid parameter\n");
        return -1;
    }
    *st = NULL;

    /* allocate memory */
    test();
    if ((s = (dtx_encState *) malloc(sizeof(dtx_encState))) == NULL)
    {
        fprintf(stderr, "dtx_enc_init: can not malloc state structure\n");
        return -1;
    }
    dtx_enc_reset(s, isf_init);
    *st = s;

    return 0;
}

/**************************************************************************
 *
 *
 * Function    : dtx_enc_reset
 *
 *
 **************************************************************************/
Word16 dtx_enc_reset(dtx_encState * st, Word16 isf_init[])
{
    Word16 i;

    test();
    if (st == (dtx_encState *) NULL)
    {
        fprintf(stderr, "dtx_enc_reset: invalid parameter\n");
        return -1;
    }
    st->hist_ptr = 0;                      move16();
    st->log_en_index = 0;                  move16();

    /* Init isf_hist[] */
    for (i = 0; i < DTX_HIST_SIZE; i++)
    {
        Copy(isf_init, &st->isf_hist[i * M], M);
    }
    st->cng_seed = RANDOM_INITSEED;        move16();

    /* Reset energy history */
    Set_zero(st->log_en_hist, DTX_HIST_SIZE);

    st->dtxHangoverCount = DTX_HANG_CONST; move16();
    st->decAnaElapsedCount = 32767;        move16();

    for (i = 0; i < 28; i++)
    {
        st->D[i] = 0;                      move16();
    }

    for (i = 0; i < DTX_HIST_SIZE - 1; i++)
    {
        st->sumD[i] = 0;                   move32();
    }

    return 1;
}

/**************************************************************************
 *
 *
 * Function    : dtx_enc_exit
 *
 *
 **************************************************************************/
void dtx_enc_exit(dtx_encState ** st)
{
    test();
    if (st == NULL || *st == NULL)
        return;

    /* deallocate memory */
    free(*st);
    *st = NULL;

    return;
}


/**************************************************************************
 *
 *
 * Function    : dtx_enc
 *
 *
 **************************************************************************/
Word16 dtx_enc(
     dtx_encState * st,                    /* i/o : State struct                                         */
     Word16 isf[M],                        /* o   : CN ISF vector                                        */
     Word16 * exc2,                        /* o   : CN excitation                                        */
     Word16 ** prms
)
{
    Word16 i, j;
    Word16 indice[7];
    Word16 log_en, gain, level, exp, exp0, tmp;
    Word16 log_en_int_e, log_en_int_m;
    Word32 L_isf[M], ener32, level32;
    Word16 isf_order[3];
    Word16 CN_dith;

    /* VOX mode computation of SID parameters */
    log_en = 0;
    move16();
    for (i = 0; i < M; i++)
    {
        L_isf[i] = 0;
        move32();
    }
    /* average energy and isf */
    for (i = 0; i < DTX_HIST_SIZE; i++)
    {
        /* Division by DTX_HIST_SIZE = 8 has been done in dtx_buffer. log_en is in Q10 */
        log_en = add(log_en, st->log_en_hist[i]);

    }
    find_frame_indices(st->isf_hist, isf_order, st);
    aver_isf_history(st->isf_hist, isf_order, L_isf);

    for (j = 0; j < M; j++)
    {
        isf[j] = extract_l(L_shr(L_isf[j], 3)); move16();  /* divide by 8 */
    }

    /* quantize logarithmic energy to 6 bits (-6 : 66 dB) which corresponds to -2:22 in log2(E).  */
    /* st->log_en_index = (short)( (log_en + 2.0) * 2.625 ); */

    /* increase dynamics to 7 bits (Q8) */
    log_en = shr(log_en, 2);

    /* Add 2 in Q8 = 512 to get log2(E) between 0:24 */
    log_en = add(log_en, 512);

    /* Multiply by 2.625 to get full 6 bit range. 2.625 = 21504 in Q13. The result is in Q6 */
    log_en = mult(log_en, 21504);

    /* Quantize Energy */
    st->log_en_index = shr(log_en, 6);

    test();
    if (sub(st->log_en_index, 63) > 0)
    {
        st->log_en_index = 63;
        move16();
    }
    test();
    if (st->log_en_index < 0)
    {
        st->log_en_index = 0;
        move16();
    }
    /* Quantize ISFs */
    Qisf_ns(isf, isf, indice);


    Parm_serial(indice[0], 6, prms);
    Parm_serial(indice[1], 6, prms);
    Parm_serial(indice[2], 6, prms);
    Parm_serial(indice[3], 5, prms);
    Parm_serial(indice[4], 5, prms);

    Parm_serial((st->log_en_index), 6, prms);

    CN_dith = dithering_control(st);
    Parm_serial(CN_dith, 1, prms);

    /* level = (float)( pow( 2.0f, (float)st->log_en_index / 2.625 - 2.0 ) );    */
    /* log2(E) in Q9 (log2(E) lies in between -2:22) */
    log_en = shl(st->log_en_index, 15 - 6);

    /* Divide by 2.625; log_en will be between 0:24  */
    log_en = mult(log_en, 12483);
    /* the result corresponds to log2(gain) in Q10 */

    /* Find integer part  */
    log_en_int_e = shr(log_en, 10);

    /* Find fractional part */
    log_en_int_m = (Word16) (log_en & 0x3ff);   logic16();
    log_en_int_m = shl(log_en_int_m, 5);

    /* Subtract 2 from log_en in Q9, i.e divide the gain by 2 (energy by 4) */
    /* Add 16 in order to have the result of pow2 in Q16 */
    log_en_int_e = add(log_en_int_e, 16 - 1);

    level32 = Pow2(log_en_int_e, log_en_int_m); /* Q16 */
    exp0 = norm_l(level32);
    level32 = L_shl(level32, exp0);        /* level in Q31 */
    exp0 = sub(15, exp0);
    level = extract_h(level32);            /* level in Q15 */

    /* generate white noise vector */
    for (i = 0; i < L_FRAME; i++)
    {
        exc2[i] = shr(Random(&(st->cng_seed)), 4);      move16();
    }

    /* gain = level / sqrt(ener) * sqrt(L_FRAME) */

    /* energy of generated excitation */
    ener32 = Dot_product12(exc2, exc2, L_FRAME, &exp);

    Isqrt_n(&ener32, &exp);

    gain = extract_h(ener32);

    gain = mult(level, gain);              /* gain in Q15 */

    exp = add(exp0, exp);

    /* Multiply by sqrt(L_FRAME)=16, i.e. shift left by 4 */
    exp = add(exp, 4);

    for (i = 0; i < L_FRAME; i++)
    {
        tmp = mult(exc2[i], gain);         /* Q0 * Q15 */
        exc2[i] = shl(tmp, exp);           move16();
    }

    return 0;
}

/**************************************************************************
 *
 *
 * Function    : dtx_buffer Purpose     : handles the DTX buffer
 *
 *
 **************************************************************************/
Word16 dtx_buffer(
     dtx_encState * st,                    /* i/o : State struct                    */
     Word16 isf_new[],                     /* i   : isf vector                      */
     Word32 enr,                           /* i   : residual energy (in L_FRAME)    */
     Word16 codec_mode
)
{
    Word16 log_en;

    Word16 log_en_e;
    Word16 log_en_m;

    st->hist_ptr = add(st->hist_ptr, 1);   move16();
    test();
    if (sub(st->hist_ptr, DTX_HIST_SIZE) == 0)
    {
        st->hist_ptr = 0;
        move16();
    }
    /* copy lsp vector into buffer */
    Copy(isf_new, &st->isf_hist[st->hist_ptr * M], M);

    /* log_en = (float)log10(enr*0.0059322)/(float)log10(2.0f);  */
    Log2(enr, &log_en_e, &log_en_m);

    /* convert exponent and mantissa to Word16 Q7. Q7 is used to simplify averaging in dtx_enc */
    log_en = shl(log_en_e, 7);             /* Q7 */
    log_en = add(log_en, shr(log_en_m, 15 - 7));

    /* Find energy per sample by multiplying with 0.0059322, i.e subtract log2(1/0.0059322) = 7.39722 The
     * constant 0.0059322 takes into account windowings and analysis length from autocorrelation
     * computations; 7.39722 in Q7 = 947  */
    /* Subtract 3 dB = 0.99658 in log2(E) = 127 in Q7. */
    /* log_en = sub( log_en, 947 + en_adjust[codec_mode] ); */

    /* Find energy per sample (divide by L_FRAME=256), i.e subtract log2(256) = 8.0  (1024 in Q7) */
    /* Subtract 3 dB = 0.99658 in log2(E) = 127 in Q7. */

    log_en = sub(log_en, add(1024, en_adjust[codec_mode]));

    /* Insert into the buffer */
    st->log_en_hist[st->hist_ptr] = log_en;move16();
    return 0;
}

/**************************************************************************
 *
 *
 * Function    : tx_dtx_handler Purpose     : adds extra speech hangover
 *                                            to analyze speech on
 *                                            the decoding side.
 *
 **************************************************************************/
void tx_dtx_handler(dtx_encState * st,     /* i/o : State struct           */
     Word16 vad_flag,                      /* i   : vad decision           */
     Word16 * usedMode                     /* i/o : mode changed or not    */
)
{

    /* this state machine is in synch with the GSMEFR txDtx machine      */
    st->decAnaElapsedCount = add(st->decAnaElapsedCount, 1);    move16();

    test();
    if (vad_flag != 0)
    {
        st->dtxHangoverCount = DTX_HANG_CONST;  move16();
    } else
    {                                      /* non-speech */
        test();
        if (st->dtxHangoverCount == 0)
        {                                  /* out of decoder analysis hangover  */
            st->decAnaElapsedCount = 0;    move16();
            *usedMode = MRDTX;             move16();
        } else
        {                                  /* in possible analysis hangover */
            st->dtxHangoverCount = sub(st->dtxHangoverCount, 1);        move16();

            /* decAnaElapsedCount + dtxHangoverCount < DTX_ELAPSED_FRAMES_THRESH */
            test();
            if (sub(add(st->decAnaElapsedCount, st->dtxHangoverCount),
                    DTX_ELAPSED_FRAMES_THRESH) < 0)
            {
                *usedMode = MRDTX;         move16();
                /* if short time since decoder update, do not add extra HO */
            }
            /* else override VAD and stay in speech mode *usedMode and add extra hangover */
        }
    }

    return;
}

/**************************************************************************
 *
 *
 * Function    : dtx_dec_init
 *
 *
 **************************************************************************/
Word16 dtx_dec_init(dtx_decState ** st, Word16 isf_init[])
{
    dtx_decState *s;

    test();
    if (st == (dtx_decState **) NULL)
    {
        fprintf(stderr, "dtx_dec_init: invalid parameter\n");
        return -1;
    }
    *st = NULL;

    /* allocate memory */
    test();
    if ((s = (dtx_decState *) malloc(sizeof(dtx_decState))) == NULL)
    {
        fprintf(stderr, "dtx_dec_init: can not malloc state structure\n");
        return -1;
    }
    dtx_dec_reset(s, isf_init);
    *st = s;

    return 0;
}

/**************************************************************************
 *
 *
 * Function    : dtx_dec_reset
 *
 *
 **************************************************************************/
Word16 dtx_dec_reset(dtx_decState * st, Word16 isf_init[])
{
    Word16 i;

    test();
    if (st == (dtx_decState *) NULL)
    {
        fprintf(stderr, "dtx_dec_reset: invalid parameter\n");
        return -1;
    }
    st->since_last_sid = 0;                move16();
    st->true_sid_period_inv = (1 << 13);   move16();  /* 0.25 in Q15 */

    st->log_en = 3500;                     move16();
    st->old_log_en = 3500;                 move16();
    /* low level noise for better performance in  DTX handover cases */

    st->cng_seed = RANDOM_INITSEED;        move16();

    st->hist_ptr = 0;                      move16();

    /* Init isf_hist[] and decoder log frame energy */
    Copy(isf_init, st->isf, M);
    Copy(isf_init, st->isf_old, M);

    for (i = 0; i < DTX_HIST_SIZE; i++)
    {
        Copy(isf_init, &st->isf_hist[i * M], M);
        st->log_en_hist[i] = st->log_en;   move16();
    }

    st->dtxHangoverCount = DTX_HANG_CONST; move16();
    st->decAnaElapsedCount = 32767;        move16();

    st->sid_frame = 0;                     move16();
    st->valid_data = 0;                    move16();
    st->dtxHangoverAdded = 0;              move16();

    st->dtxGlobalState = SPEECH;           move16();
    st->data_updated = 0;                  move16();

    st->dither_seed = RANDOM_INITSEED;     move16();
    st->CN_dith = 0;

    return 0;
}

/**************************************************************************
 *
 *
 * Function    : dtx_dec_exit
 *
 *
 **************************************************************************/
void dtx_dec_exit(dtx_decState ** st)
{
    test();
    if (st == NULL || *st == NULL)
        return;

    /* deallocate memory */
    free(*st);
    *st = NULL;

    return;
}

/*
     Table of new SPD synthesis states

                           |     previous SPD_synthesis_state
     Incoming              |
     frame_type            | SPEECH       | DTX           | DTX_MUTE
     ---------------------------------------------------------------
     RX_SPEECH_GOOD ,      |              |               |
     RX_SPEECH_PR_DEGRADED | SPEECH       | SPEECH        | SPEECH
     ----------------------------------------------------------------
     RX_SPEECH_BAD,        | SPEECH       | DTX           | DTX_MUTE
     ----------------------------------------------------------------
     RX_SID_FIRST,         | DTX          | DTX/(DTX_MUTE)| DTX_MUTE
     ----------------------------------------------------------------
     RX_SID_UPDATE,        | DTX          | DTX           | DTX
     ----------------------------------------------------------------
     RX_SID_BAD,           | DTX          | DTX/(DTX_MUTE)| DTX_MUTE
     ----------------------------------------------------------------
     RX_NO_DATA,           | SPEECH       | DTX/(DTX_MUTE)| DTX_MUTE
     RX_SPARE              |(class2 garb.)|               |
     ----------------------------------------------------------------
*/

/**************************************************************************
 *
 *
 * Function    : dtx_dec
 *
 *
 **************************************************************************/
Word16 dtx_dec(
     dtx_decState * st,                    /* i/o : State struct                                          */
     Word16 * exc2,                        /* o   : CN excitation                                          */
     Word16 new_state,                     /* i   : New DTX state                                          */
     Word16 isf[],                         /* o   : CN ISF vector                                          */
     Word16 ** prms
)
{
    Word16 log_en_index;
    Word16 ind[7];
    Word16 i, j;
    Word16 int_fac;
    Word16 gain;

    Word32 L_isf[M], L_log_en_int, level32, ener32;
    Word16 ptr;
    Word16 tmp_int_length;
    Word16 tmp, exp, exp0, log_en_int_e, log_en_int_m, level;

    /* This function is called if synthesis state is not SPEECH the globally passed  inputs to this function
     * are st->sid_frame st->valid_data st->dtxHangoverAdded new_state  (SPEECH, DTX, DTX_MUTE) */
    test();test();
    if ((st->dtxHangoverAdded != 0) &&
        (st->sid_frame != 0))
    {
        /* sid_first after dtx hangover period */
        /* or sid_upd after dtxhangover        */

        /* consider  twice the last frame */
        ptr = add(st->hist_ptr, 1);
        test();
        if (sub(ptr, DTX_HIST_SIZE) == 0)
            ptr = 0;                       move16();

        Copy(&st->isf_hist[st->hist_ptr * M], &st->isf_hist[ptr * M], M);

        st->log_en_hist[ptr] = st->log_en_hist[st->hist_ptr];   move16();

        /* compute mean log energy and isf from decoded signal (SID_FIRST) */
        st->log_en = 0;                    move16();
        for (i = 0; i < M; i++)
        {
            L_isf[i] = 0;                  move32();
        }

        /* average energy and isf */
        for (i = 0; i < DTX_HIST_SIZE; i++)
        {
            /* Division by DTX_HIST_SIZE = 8 has been done in dtx_buffer log_en is in Q10 */
            st->log_en = add(st->log_en, st->log_en_hist[i]);   move16();

            for (j = 0; j < M; j++)
            {
                L_isf[j] = L_add(L_isf[j], L_deposit_l(st->isf_hist[i * M + j]));       move32();
            }
        }

        /* st->log_en in Q9 */
        st->log_en = shr(st->log_en, 1);   move16();

        /* Add 2 in Q9, in order to have only positive values for Pow2 */
        /* this value is subtracted back after Pow2 function */
        st->log_en = add(st->log_en, 1024);move16();
        test();
        if (st->log_en < 0)
            st->log_en = 0;                move16();

        for (j = 0; j < M; j++)
        {
            st->isf[j] = extract_l(L_shr(L_isf[j], 3)); move32();  /* divide by 8 */
        }

    }
    test();
    if (st->sid_frame != 0)
    {
        /* Set old SID parameters, always shift */
        /* even if there is no new valid_data   */

        Copy(st->isf, st->isf_old, M);
        st->old_log_en = st->log_en;       move16();
        test();
        if (st->valid_data != 0)           /* new data available (no CRC) */
        {
            /* st->true_sid_period_inv = 1.0f/st->since_last_sid; */
            /* Compute interpolation factor, since the division only works * for values of since_last_sid <
             * 32 we have to limit the      * interpolation to 32 frames                                  */
            tmp_int_length = st->since_last_sid;        move16();

            test();
            if (sub(tmp_int_length, 32) > 0)
            {
                tmp_int_length = 32;       move16();
            }
            test();
            if (sub(tmp_int_length, 2) >= 0)
            {
                move16();
                st->true_sid_period_inv = div_s(1 << 10, shl(tmp_int_length, 10));
            } else
            {
                st->true_sid_period_inv = 1 << 14;      /* 0.5 it Q15 */move16();
            }

            ind[0] = Serial_parm(6, prms); move16();
            ind[1] = Serial_parm(6, prms); move16();
            ind[2] = Serial_parm(6, prms); move16();
            ind[3] = Serial_parm(5, prms); move16();
            ind[4] = Serial_parm(5, prms); move16();

            Disf_ns(ind, st->isf);

            log_en_index = Serial_parm(6, prms);

            /* read background noise stationarity information */
            st->CN_dith = Serial_parm(1, prms); move16();

            /* st->log_en = (float)log_en_index / 2.625 - 2.0;  */
            /* log2(E) in Q9 (log2(E) lies in between -2:22) */
            st->log_en = shl(log_en_index, 15 - 6);     move16();

            /* Divide by 2.625  */
            st->log_en = mult(st->log_en, 12483);       move16();
            /* Subtract 2 in Q9 is done later, after Pow2 function  */

            /* no interpolation at startup after coder reset        */
            /* or when SID_UPD has been received right after SPEECH */
            test();test();
            if ((st->data_updated == 0) ||
                (sub(st->dtxGlobalState, SPEECH) == 0))
            {
                Copy(st->isf, st->isf_old, M);
                st->old_log_en = st->log_en;    move16();
            }
        }                                  /* endif valid_data */
    }                                      /* endif sid_frame */
    test();
    test();
    if ((st->sid_frame != 0) && (st->valid_data != 0))
    {
        st->since_last_sid = 0;            move16();
    }
    /* Interpolate SID info */
    int_fac = shl(st->since_last_sid, 10); /* Q10 */move16();
    int_fac = mult(int_fac, st->true_sid_period_inv);   /* Q10 * Q15 -> Q10 */

    /* Maximize to 1.0 in Q10 */
    test();
    if (sub(int_fac, 1024) > 0)
    {
        int_fac = 1024;                    move16();
    }
    int_fac = shl(int_fac, 4);             /* Q10 -> Q14 */

    L_log_en_int = L_mult(int_fac, st->log_en); /* Q14 * Q9 -> Q24 */

    for (i = 0; i < M; i++)
    {
        isf[i] = mult(int_fac, st->isf[i]);/* Q14 * Q15 -> Q14 */move16();
    }

    int_fac = sub(16384, int_fac);         /* 1-k in Q14 */move16();

    /* ( Q14 * Q9 -> Q24 ) + Q24 -> Q24 */
    L_log_en_int = L_mac(L_log_en_int, int_fac, st->old_log_en);

    for (i = 0; i < M; i++)
    {
        /* Q14 + (Q14 * Q15 -> Q14) -> Q14 */
        isf[i] = add(isf[i], mult(int_fac, st->isf_old[i]));    move16();
        isf[i] = shl(isf[i], 1);           /* Q14 -> Q15 */move16();
    }

    /* If background noise is non-stationary, insert comfort noise dithering */
    if (st->CN_dith != 0)
    {
        CN_dithering(isf, &L_log_en_int, &st->dither_seed);
    }
    /* L_log_en_int corresponds to log2(E)+2 in Q24, i.e log2(gain)+1 in Q25 */
    /* Q25 -> Q16 */
    L_log_en_int = L_shr(L_log_en_int, 9);

    /* Find integer part  */
    log_en_int_e = extract_h(L_log_en_int);

    /* Find fractional part */
    log_en_int_m = extract_l(L_shr(L_sub(L_log_en_int, L_deposit_h(log_en_int_e)), 1));

    /* Subtract 2 from L_log_en_int in Q9, i.e divide the gain by 2 (energy by 4) */
    /* Add 16 in order to have the result of pow2 in Q16 */
    log_en_int_e = add(log_en_int_e, 16 - 1);

    /* level = (float)( pow( 2.0f, log_en ) );  */
    level32 = Pow2(log_en_int_e, log_en_int_m); /* Q16 */
    exp0 = norm_l(level32);
    level32 = L_shl(level32, exp0);        /* level in Q31 */
    exp0 = sub(15, exp0);
    level = extract_h(level32);            /* level in Q15 */

    /* generate white noise vector */
    for (i = 0; i < L_FRAME; i++)
    {
        exc2[i] = shr(Random(&(st->cng_seed)), 4);      move16();
    }

    /* gain = level / sqrt(ener) * sqrt(L_FRAME) */

    /* energy of generated excitation */
    ener32 = Dot_product12(exc2, exc2, L_FRAME, &exp);

    Isqrt_n(&ener32, &exp);

    gain = extract_h(ener32);

    gain = mult(level, gain);              /* gain in Q15 */

    exp = add(exp0, exp);

    /* Multiply by sqrt(L_FRAME)=16, i.e. shift left by 4 */
    exp = add(exp, 4);

    for (i = 0; i < L_FRAME; i++)
    {
        tmp = mult(exc2[i], gain);         /* Q0 * Q15 */
        exc2[i] = shl(tmp, exp);           move16();
    }

    test();
    if (sub(new_state, DTX_MUTE) == 0)
    {
        /* mute comfort noise as it has been quite a long time since last SID update  was performed                            */

        tmp_int_length = st->since_last_sid;    move16();
        test();
        if (sub(tmp_int_length, 32) > 0)
        {
            tmp_int_length = 32;           move16();
        }
        move16();
        st->true_sid_period_inv = div_s(1 << 10, shl(tmp_int_length, 10));

        st->since_last_sid = 0;            move16();
        st->old_log_en = st->log_en;       move16();
        /* subtract 1/8 in Q9 (energy), i.e -3/8 dB */
        st->log_en = sub(st->log_en, 64);  move16();
    }
    /* reset interpolation length timer if data has been updated.        */
    test();test();test();test();
    if ((st->sid_frame != 0) &&
        ((st->valid_data != 0) ||
            ((st->valid_data == 0) && (st->dtxHangoverAdded) != 0)))
    {
        st->since_last_sid = 0;            move16();
        st->data_updated = 1;              move16();
    }
    return 0;
}


void dtx_dec_activity_update(
     dtx_decState * st,
     Word16 isf[],
     Word16 exc[])
{
    Word16 i;

    Word32 L_frame_en;
    Word16 log_en_e, log_en_m, log_en;


    st->hist_ptr = add(st->hist_ptr, 1);   move16();
    test();
    if (sub(st->hist_ptr, DTX_HIST_SIZE) == 0)
    {
        st->hist_ptr = 0;                  move16();
    }
    Copy(isf, &st->isf_hist[st->hist_ptr * M], M);

    /* compute log energy based on excitation frame energy in Q0 */
    L_frame_en = 0;                        move32();
    for (i = 0; i < L_FRAME; i++)
    {
        L_frame_en = L_mac(L_frame_en, exc[i], exc[i]);
    }
    L_frame_en = L_shr(L_frame_en, 1);

    /* log_en = (float)log10(L_frame_en/(float)L_FRAME)/(float)log10(2.0f); */
    Log2(L_frame_en, &log_en_e, &log_en_m);

    /* convert exponent and mantissa to Word16 Q7. Q7 is used to simplify averaging in dtx_enc */
    log_en = shl(log_en_e, 7);             /* Q7 */
    log_en = add(log_en, shr(log_en_m, 15 - 7));

    /* Divide by L_FRAME = 256, i.e subtract 8 in Q7 = 1024 */
    log_en = sub(log_en, 1024);

    /* insert into log energy buffer */
    st->log_en_hist[st->hist_ptr] = log_en;move16();

    return;
}


/*
     Table of new SPD synthesis states

                           |     previous SPD_synthesis_state
     Incoming              |
     frame_type            | SPEECH       | DTX           | DTX_MUTE
     ---------------------------------------------------------------
     RX_SPEECH_GOOD ,      |              |               |
     RX_SPEECH_PR_DEGRADED | SPEECH       | SPEECH        | SPEECH
     ----------------------------------------------------------------
     RX_SPEECH_BAD,        | SPEECH       | DTX           | DTX_MUTE
     ----------------------------------------------------------------
     RX_SID_FIRST,         | DTX          | DTX/(DTX_MUTE)| DTX_MUTE
     ----------------------------------------------------------------
     RX_SID_UPDATE,        | DTX          | DTX           | DTX
     ----------------------------------------------------------------
     RX_SID_BAD,           | DTX          | DTX/(DTX_MUTE)| DTX_MUTE
     ----------------------------------------------------------------
     RX_NO_DATA,           | SPEECH       | DTX/(DTX_MUTE)| DTX_MUTE
     RX_SPARE              |(class2 garb.)|               |
     ----------------------------------------------------------------
*/

Word16 rx_dtx_handler(
     dtx_decState * st,                    /* i/o : State struct     */
     Word16 frame_type                     /* i   : Frame type       */
)
{
    Word16 newState;
    Word16 encState;

    /* DTX if SID frame or previously in DTX{_MUTE} and (NO_RX OR BAD_SPEECH) */
    test();test();test();
    test();test();test();
    test();test();
    if ((sub(frame_type, RX_SID_FIRST) == 0) ||
        (sub(frame_type, RX_SID_UPDATE) == 0) ||
        (sub(frame_type, RX_SID_BAD) == 0) ||
        (((sub(st->dtxGlobalState, DTX) == 0) ||
                (sub(st->dtxGlobalState, DTX_MUTE) == 0)) &&
            ((sub(frame_type, RX_NO_DATA) == 0) ||
                (sub(frame_type, RX_SPEECH_BAD) == 0) ||
                (sub(frame_type, RX_SPEECH_LOST) == 0))))
    {
        newState = DTX;                    move16();

        /* stay in mute for these input types */
        test();test();test();test();test();
        if ((sub(st->dtxGlobalState, DTX_MUTE) == 0) &&
            ((sub(frame_type, RX_SID_BAD) == 0) ||
                (sub(frame_type, RX_SID_FIRST) == 0) ||
                (sub(frame_type, RX_SPEECH_LOST) == 0) ||
                (sub(frame_type, RX_NO_DATA) == 0)))
        {
            newState = DTX_MUTE;           move16();
        }
        /* evaluate if noise parameters are too old                     */
        /* since_last_sid is reset when CN parameters have been updated */
        st->since_last_sid = add(st->since_last_sid, 1);        move16();

        /* no update of sid parameters in DTX for a long while */
        test();
        if (sub(st->since_last_sid, DTX_MAX_EMPTY_THRESH) > 0)
        {
            newState = DTX_MUTE;           move16();
        }
    } else
    {
        newState = SPEECH;                 move16();
        st->since_last_sid = 0;            move16();
    }

    /* reset the decAnaElapsed Counter when receiving CNI data the first time, to robustify counter missmatch
     * after handover this might delay the bwd CNI analysis in the new decoder slightly. */
    test();test();
    if ((st->data_updated == 0) &&
        (sub(frame_type, RX_SID_UPDATE) == 0))
    {
        st->decAnaElapsedCount = 0;        move16();
    }
    /* update the SPE-SPD DTX hangover synchronization */
    /* to know when SPE has added dtx hangover         */
    st->decAnaElapsedCount = add(st->decAnaElapsedCount, 1);    move16();
    st->dtxHangoverAdded = 0;              move16();

    test();test();test();test();
    if ((sub(frame_type, RX_SID_FIRST) == 0) ||
        (sub(frame_type, RX_SID_UPDATE) == 0) ||
        (sub(frame_type, RX_SID_BAD) == 0) ||
        (sub(frame_type, RX_NO_DATA) == 0))
    {
        encState = DTX;                    move16();
    } else
    {
        encState = SPEECH;                 move16();
    }

    test();
    if (sub(encState, SPEECH) == 0)
    {
        st->dtxHangoverCount = DTX_HANG_CONST;  move16();
    } else
    {
        test();test();
        if (sub(st->decAnaElapsedCount, DTX_ELAPSED_FRAMES_THRESH) > 0)
        {
            st->dtxHangoverAdded = 1;      move16();
            st->decAnaElapsedCount = 0;    move16();
            st->dtxHangoverCount = 0;      move16();
        } else if (test(), st->dtxHangoverCount == 0)
        {
            st->decAnaElapsedCount = 0;    move16();
        } else
        {
            st->dtxHangoverCount = sub(st->dtxHangoverCount, 1);        move16();
        }
    }
    test();
    if (sub(newState, SPEECH) != 0)
    {
        /* DTX or DTX_MUTE CN data is not in a first SID, first SIDs are marked as SID_BAD but will do
         * backwards analysis if a hangover period has been added according to the state machine above */

        st->sid_frame = 0;                 move16();
        st->valid_data = 0;                move16();

        test();test();test();
        if (sub(frame_type, RX_SID_FIRST) == 0)
        {
            st->sid_frame = 1;             move16();
        } else if (test(), sub(frame_type, RX_SID_UPDATE) == 0)
        {
            st->sid_frame = 1;             move16();
            st->valid_data = 1;            move16();
        } else if (test(), sub(frame_type, RX_SID_BAD) == 0)
        {
            st->sid_frame = 1;             move16();
            st->dtxHangoverAdded = 0;      /* use old data */move16();
        }
    }
    return newState;
    /* newState is used by both SPEECH AND DTX synthesis routines */
}

static void aver_isf_history(
     Word16 isf_old[],
     Word16 indices[],
     Word32 isf_aver[]
)
{
    Word16 i, j, k;
    Word16 isf_tmp[2 * M];
    Word32 L_tmp;

    /* Memorize in isf_tmp[][] the ISF vectors to be replaced by */
    /* the median ISF vector prior to the averaging               */
    for (k = 0; k < 2; k++)
    {
        test();
        if (add(indices[k], 1) != 0)
        {
            for (i = 0; i < M; i++)
            {
                isf_tmp[k * M + i] = isf_old[indices[k] * M + i];       move16();
                isf_old[indices[k] * M + i] = isf_old[indices[2] * M + i];      move16();
            }
        }
    }

    /* Perform the ISF averaging */
    for (j = 0; j < M; j++)
    {
        L_tmp = 0;                         move32();

        for (i = 0; i < DTX_HIST_SIZE; i++)
        {
            L_tmp = L_add(L_tmp, L_deposit_l(isf_old[i * M + j]));
        }
        isf_aver[j] = L_tmp;               move32();
    }

    /* Retrieve from isf_tmp[][] the ISF vectors saved prior to averaging */
    for (k = 0; k < 2; k++)
    {
        test();
        if (add(indices[k], 1) != 0)
        {
            for (i = 0; i < M; i++)
            {
                isf_old[indices[k] * M + i] = isf_tmp[k * M + i];       move16();
            }
        }
    }

    return;
}

static void find_frame_indices(
     Word16 isf_old_tx[],
     Word16 indices[],
     dtx_encState * st
)
{
    Word32 L_tmp, summin, summax, summax2nd;
    Word16 i, j, tmp;
    Word16 ptr;

    /* Remove the effect of the oldest frame from the column */
    /* sum sumD[0..DTX_HIST_SIZE-1]. sumD[DTX_HIST_SIZE] is    */
    /* not updated since it will be removed later.           */

    tmp = DTX_HIST_SIZE_MIN_ONE;           move16();
    j = -1;                                move16();
    for (i = 0; i < DTX_HIST_SIZE_MIN_ONE; i++)
    {
        j = add(j, tmp);
        st->sumD[i] = L_sub(st->sumD[i], st->D[j]);     move16();
        tmp = sub(tmp, 1);
    }

    /* Shift the column sum sumD. The element sumD[DTX_HIST_SIZE-1]    */
    /* corresponding to the oldest frame is removed. The sum of     */
    /* the distances between the latest isf and other isfs, */
    /* i.e. the element sumD[0], will be computed during this call. */
    /* Hence this element is initialized to zero.                   */

    for (i = DTX_HIST_SIZE_MIN_ONE; i > 0; i--)
    {
        st->sumD[i] = st->sumD[i - 1];     move32();
    }
    st->sumD[0] = 0;                       move32();

    /* Remove the oldest frame from the distance matrix.           */
    /* Note that the distance matrix is replaced by a one-         */
    /* dimensional array to save static memory.                    */

    tmp = 0;                               move16();
    for (i = 27; i >= 12; i = (Word16) (i - tmp))
    {
        tmp = add(tmp, 1);
        for (j = tmp; j > 0; j--)
        {
            st->D[i - j + 1] = st->D[i - j - tmp];      move32();
        }
    }

    /* Compute the first column of the distance matrix D            */
    /* (squared Euclidean distances from isf1[] to isf_old_tx[][]). */

    ptr = st->hist_ptr;                    move16();
    for (i = 1; i < DTX_HIST_SIZE; i++)
    {
        /* Compute the distance between the latest isf and the other isfs. */
        ptr = sub(ptr, 1);
        test();
        if (ptr < 0)
        {
            ptr = DTX_HIST_SIZE_MIN_ONE;   move16();
        }
        L_tmp = 0;                         move32();
        for (j = 0; j < M; j++)
        {
            tmp = sub(isf_old_tx[st->hist_ptr * M + j], isf_old_tx[ptr * M + j]);
            L_tmp = L_mac(L_tmp, tmp, tmp);
        }
        st->D[i - 1] = L_tmp;              move32();

        /* Update also the column sums. */
        st->sumD[0] = L_add(st->sumD[0], st->D[i - 1]); move32();
        st->sumD[i] = L_add(st->sumD[i], st->D[i - 1]); move32();
    }

    /* Find the minimum and maximum distances */
    summax = st->sumD[0];                  move32();
    summin = st->sumD[0];                  move32();
    indices[0] = 0;                        move16();
    indices[2] = 0;                        move16();
    for (i = 1; i < DTX_HIST_SIZE; i++)
    {
        test();
        if (L_sub(st->sumD[i], summax) > 0)
        {
            indices[0] = i;                move16();
            summax = st->sumD[i];          move32();
        }
        test();
        if (L_sub(st->sumD[i], summin) < 0)
        {
            indices[2] = i;                move16();
            summin = st->sumD[i];          move32();
        }
    }

    /* Find the second largest distance */
    summax2nd = -2147483647L;              move32();
    indices[1] = -1;                       move16();
    for (i = 0; i < DTX_HIST_SIZE; i++)
    {
        test();
        if ((L_sub(st->sumD[i], summax2nd) > 0) && (sub(i, indices[0]) != 0))
        {
            indices[1] = i;                move16();
            summax2nd = st->sumD[i];       move32();
        }
    }

    for (i = 0; i < 3; i++)
    {
        indices[i] = sub(st->hist_ptr, indices[i]);     move16();
        test();
        if (indices[i] < 0)
        {
            indices[i] = add(indices[i], DTX_HIST_SIZE);        move16();
        }
    }

    /* If maximum distance/MED_THRESH is smaller than minimum distance */
    /* then the median ISF vector replacement is not performed         */
    tmp = norm_l(summax);
    summax = L_shl(summax, tmp);
    summin = L_shl(summin, tmp);
    L_tmp = L_mult(round(summax), INV_MED_THRESH);
    test();
    if (L_sub(L_tmp, summin) <= 0)
    {
        indices[0] = -1;                   move16();
    }
    /* If second largest distance/MED_THRESH is smaller than     */
    /* minimum distance then the median ISF vector replacement is    */
    /* not performed                                                 */
    summax2nd = L_shl(summax2nd, tmp);
    L_tmp = L_mult(round(summax2nd), INV_MED_THRESH);
    test();
    if (L_sub(L_tmp, summin) <= 0)
    {
        indices[1] = -1;                   move16();
    }
    return;
}

static Word16 dithering_control(
     dtx_encState * st
)
{
    Word16 i, tmp, mean, CN_dith, gain_diff;
    Word32 ISF_diff;

    /* determine how stationary the spectrum of background noise is */
    ISF_diff = 0;
    for (i = 0; i < 8; i++)
    {
        ISF_diff = L_add(ISF_diff, st->sumD[i]);
    }
    if (L_shr(ISF_diff, 26) > 0)
    {
        CN_dith = 1;
    } else
    {
        CN_dith = 0;
    }

    /* determine how stationary the energy of background noise is */
    mean = 0;
    for (i = 0; i < DTX_HIST_SIZE; i++)
    {
        mean = add(mean, st->log_en_hist[i]);
    }
    mean = shr(mean, 3);
    gain_diff = 0;
    for (i = 0; i < DTX_HIST_SIZE; i++)
    {
        tmp = abs_s(sub(st->log_en_hist[i], mean));
        gain_diff = add(gain_diff, tmp);
    }
    if (sub(gain_diff, GAIN_THR) > 0)
    {
        CN_dith = 1;
    }
    return CN_dith;
}

static void CN_dithering(
     Word16 isf[M],
     Word32 * L_log_en_int,
     Word16 * dither_seed
)
{
    Word16 temp, temp1, i, dither_fac, rand_dith;
    Word16 rand_dith2;

    /* Insert comfort noise dithering for energy parameter */
    rand_dith = shr(Random(dither_seed), 1);
    rand_dith2 = shr(Random(dither_seed), 1);
    rand_dith = add(rand_dith, rand_dith2);
    *L_log_en_int = L_add(*L_log_en_int, L_mult(rand_dith, GAIN_FACTOR));
    if (*L_log_en_int < 0)
    {
        *L_log_en_int = 0;
    }
    /* Insert comfort noise dithering for spectral parameters (ISF-vector) */
    dither_fac = ISF_FACTOR_LOW;

    rand_dith = shr(Random(dither_seed), 1);
    rand_dith2 = shr(Random(dither_seed), 1);
    rand_dith = add(rand_dith, rand_dith2);
    temp = add(isf[0], mult_r(rand_dith, dither_fac));

    /* Make sure that isf[0] will not get negative values */
    if (sub(temp, ISF_GAP) < 0)
    {
        isf[0] = ISF_GAP;
    } else
    {
        isf[0] = temp;
    }

    for (i = 1; i < M - 1; i++)
    {
        dither_fac = add(dither_fac, ISF_FACTOR_STEP);

        rand_dith = shr(Random(dither_seed), 1);
        rand_dith2 = shr(Random(dither_seed), 1);
        rand_dith = add(rand_dith, rand_dith2);
        temp = add(isf[i], mult_r(rand_dith, dither_fac));
        temp1 = sub(temp, isf[i - 1]);

        /* Make sure that isf spacing remains at least ISF_DITH_GAP Hz */
        if (sub(temp1, ISF_DITH_GAP) < 0)
        {
            isf[i] = add(isf[i - 1], ISF_DITH_GAP);
        } else
        {
            isf[i] = temp;
        }
    }

    /* Make sure that isf[M-2] will not get values above 16384 */
    if (sub(isf[M - 2], 16384) > 0)
    {
        isf[M - 2] = 16384;
    }
    return;
}
