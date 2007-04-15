/*------------------------------------------------------------------------*
 *                         DEC_MAIN.C                                     *
 *------------------------------------------------------------------------*
 * Performs the main decoder routine                                      *
 *------------------------------------------------------------------------*/

/*___________________________________________________________________________
 |                                                                           |
 | Fixed-point C simulation of AMR WB ACELP coding algorithm with 20 ms      |
 | speech frames for wideband speech signals.                                |
 |___________________________________________________________________________|
*/


#include <stdio.h>
#include <stdlib.h>

#include "typedef.h"
#include "basic_op.h"
#include "oper_32b.h"
#include "cnst.h"
#include "acelp.h"
#include "dec_main.h"
#include "bits.h"
#include "count.h"
#include "math_op.h"
#include "main.h"


/* LPC interpolation coef {0.45, 0.8, 0.96, 1.0}; in Q15 */
static Word16 interpol_frac[NB_SUBFR] = {14746, 26214, 31457, 32767};

/* High Band encoding */
static const Word16 HP_gain[16] =
{
   3624, 4673, 5597, 6479, 7425, 8378, 9324, 10264,
   11210, 12206, 13391, 14844, 16770, 19655, 24289, 32728
};

/* isp tables for initialization */

static Word16 isp_init[M] =
{
   32138, 30274, 27246, 23170, 18205, 12540, 6393, 0,
   -6393, -12540, -18205, -23170, -27246, -30274, -32138, 1475
};

static Word16 isf_init[M] =
{
   1024, 2048, 3072, 4096, 5120, 6144, 7168, 8192,
   9216, 10240, 11264, 12288, 13312, 14336, 15360, 3840
};

static void synthesis(
     Word16 Aq[],                          /* A(z)  : quantized Az               */
     Word16 exc[],                         /* (i)   : excitation at 12kHz        */
     Word16 Q_new,                         /* (i)   : scaling performed on exc   */
     Word16 synth16k[],                    /* (o)   : 16kHz synthesis signal     */
     Word16 prms,                          /* (i)   : parameter                  */
     Word16 HfIsf[],
     Word16 nb_bits,
     Word16 newDTXState,
     Decoder_State * st,                   /* (i/o) : State structure            */
     Word16 bfi                            /* (i)   : bad frame indicator        */
);

/*-----------------------------------------------------------------*
 *   Funtion  init_decoder                                         *
 *            ~~~~~~~~~~~~                                         *
 *   ->Initialization of variables for the decoder section.        *
 *-----------------------------------------------------------------*/

void Init_decoder(void **spd_state)
{
    /* Decoder states */
    Decoder_State *st;

    *spd_state = NULL;

    /*-------------------------------------------------------------------------*
     * Memory allocation for coder state.                                      *
     *-------------------------------------------------------------------------*/

    test();
    if ((st = (Decoder_State *) malloc(sizeof(Decoder_State))) == NULL)
    {
        printf("Can not malloc Decoder_State structure!\n");
        return;
    }
    st->dtx_decSt = NULL;
    dtx_dec_init(&st->dtx_decSt, isf_init);

    Reset_decoder((void *) st, 1);

    *spd_state = (void *) st;

    return;
}

void Reset_decoder(void *st, int reset_all)
{
    Word16 i;

    Decoder_State *dec_state;

    dec_state = (Decoder_State *) st;

    Set_zero(dec_state->old_exc, PIT_MAX + L_INTERPOL);
    Set_zero(dec_state->past_isfq, M);

    dec_state->old_T0_frac = 0;            move16();  /* old pitch value = 64.0 */
    dec_state->old_T0 = 64;                move16();
    dec_state->first_frame = 1;            move16();
    dec_state->L_gc_thres = 0;             move16();
    dec_state->tilt_code = 0;              move16();

    Init_Phase_dispersion(dec_state->disp_mem);

    /* scaling memories for excitation */
    dec_state->Q_old = Q_MAX;              move16();
    dec_state->Qsubfr[3] = Q_MAX;          move16();
    dec_state->Qsubfr[2] = Q_MAX;          move16();
    dec_state->Qsubfr[1] = Q_MAX;          move16();
    dec_state->Qsubfr[0] = Q_MAX;          move16();

    if (reset_all != 0)
    {
        /* routines initialization */

        Init_D_gain2(dec_state->dec_gain);
        Init_Oversamp_16k(dec_state->mem_oversamp);
        Init_HP50_12k8(dec_state->mem_sig_out);
        Init_Filt_6k_7k(dec_state->mem_hf);
        Init_Filt_7k(dec_state->mem_hf3);
        Init_HP400_12k8(dec_state->mem_hp400);
        Init_Lagconc(dec_state->lag_hist);

        /* isp initialization */

        Copy(isp_init, dec_state->ispold, M);
        Copy(isf_init, dec_state->isfold, M);
        for (i = 0; i < L_MEANBUF; i++)
            Copy(isf_init, &dec_state->isf_buf[i * M], M);
        /* variable initialization */

        dec_state->mem_deemph = 0;         move16();

        dec_state->seed = 21845;           move16();  /* init random with 21845 */
        dec_state->seed2 = 21845;          move16();
        dec_state->seed3 = 21845;          move16();

        dec_state->state = 0;              move16();
        dec_state->prev_bfi = 0;           move16();

        /* Static vectors to zero */

        Set_zero(dec_state->mem_syn_hf, M16k);
        Set_zero(dec_state->mem_syn_hi, M);
        Set_zero(dec_state->mem_syn_lo, M);

        dtx_dec_reset(dec_state->dtx_decSt, isf_init);
        dec_state->vad_hist = 0;           move16();

    }
    return;
}

void Close_decoder(void *spd_state)
{
    dtx_dec_exit(&(((Decoder_State *) spd_state)->dtx_decSt));
    free(spd_state);
    return;
}

/*-----------------------------------------------------------------*
 *   Funtion decoder                                               *
 *           ~~~~~~~                                               *
 *   ->Main decoder routine.                                       *
 *                                                                 *
 *-----------------------------------------------------------------*/

void decoder(
     int mode,                          /* input : used mode                     */
     Word16 prms[],                        /* input : parameter vector              */
     Word16 synth16k[],                    /* output: synthesis speech              */
     Word16 * frame_length,                /* output:  lenght of the frame          */
     void *spd_state,                      /* i/o   : State structure               */
     int  frame_type                     /* input : received frame type           */
)
{

    /* Decoder states */
    Decoder_State *st;

    /* Excitation vector */
    Word16 old_exc[(L_FRAME + 1) + PIT_MAX + L_INTERPOL];
    Word16 *exc;

    /* LPC coefficients */

    Word16 *p_Aq;                          /* ptr to A(z) for the 4 subframes      */
    Word16 Aq[NB_SUBFR * (M + 1)];         /* A(z)   quantized for the 4 subframes */
    Word16 ispnew[M];                      /* immittance spectral pairs at 4nd sfr */
    Word16 isf[M];                         /* ISF (frequency domain) at 4nd sfr    */
    Word16 code[L_SUBFR];                  /* algebraic codevector                 */
    Word16 code2[L_SUBFR];                 /* algebraic codevector                 */
    Word16 exc2[L_FRAME];                  /* excitation vector                    */

    Word16 fac, stab_fac, voice_fac, Q_new = 0;
    Word32 L_tmp, L_gain_code;

    /* Scalars */

    Word16 i, j, i_subfr, index, ind[8], max, tmp;
    Word16 T0, T0_frac, pit_flag, T0_max, select, T0_min = 0;
    Word16 gain_pit, gain_code, gain_code_lo;
    Word16 newDTXState, bfi, unusable_frame, nb_bits;
    Word16 vad_flag;
    Word16 pit_sharp;
    Word16 excp[L_SUBFR];
    Word16 isf_tmp[M];
    Word16 HfIsf[M16k];

    Word16 corr_gain = 0;

    st = (Decoder_State *) spd_state;

    /* mode verification */

    nb_bits = nb_of_bits[mode];            move16();

    *frame_length = L_FRAME16k;            move16();

    /* find the new  DTX state  SPEECH OR DTX */
    newDTXState = rx_dtx_handler(st->dtx_decSt, frame_type);

    test();
    if (sub(newDTXState, SPEECH) != 0)
    {
        dtx_dec(st->dtx_decSt, exc2, newDTXState, isf, &prms);
    }
    /* SPEECH action state machine  */
    test();test();
    if ((sub(frame_type, RX_SPEECH_BAD) == 0) ||
        (sub(frame_type, RX_SPEECH_PROBABLY_DEGRADED) == 0))
    {
        /* bfi only for lsf, gains and pitch period */
        bfi = 1;                           move16();
        unusable_frame = 0;                move16();
    } else if ((sub(frame_type, RX_NO_DATA) == 0) ||
               (sub(frame_type, RX_SPEECH_LOST) == 0))
    {
        /* bfi for all index, bits are not usable */
        bfi = 1;                           move16();
        unusable_frame = 1;                move16();
    } else
    {
        bfi = 0;                           move16();
        unusable_frame = 0;                move16();
    }
    test();
    if (bfi != 0)
    {
        st->state = add(st->state, 1);     move16();
        test();
        if (sub(st->state, 6) > 0)
        {
            st->state = 6;                 move16();
        }
    } else
    {
        st->state = shr(st->state, 1);     move16();
    }

    /* If this frame is the first speech frame after CNI period,     */
    /* set the BFH state machine to an appropriate state depending   */
    /* on whether there was DTX muting before start of speech or not */
    /* If there was DTX muting, the first speech frame is muted.     */
    /* If there was no DTX muting, the first speech frame is not     */
    /* muted. The BFH state machine starts from state 5, however, to */
    /* keep the audible noise resulting from a SID frame which is    */
    /* erroneously interpreted as a good speech frame as small as    */
    /* possible (the decoder output in this case is quickly muted)   */
    test();test();
    if (sub(st->dtx_decSt->dtxGlobalState, DTX) == 0)
    {
        st->state = 5;                     move16();
        st->prev_bfi = 0;                  move16();
    } else if (sub(st->dtx_decSt->dtxGlobalState, DTX_MUTE) == 0)
    {
        st->state = 5;                     move16();
        st->prev_bfi = 1;                  move16();
    }
    test();
    if (sub(newDTXState, SPEECH) == 0)
    {
        vad_flag = Serial_parm(1, &prms);
        test();
        if (bfi == 0)
        {
            test();
            if (vad_flag == 0)
            {
                st->vad_hist = add(st->vad_hist, 1);    move16();
            } else
            {
                st->vad_hist = 0;          move16();
            }
        }
    }
    /*----------------------------------------------------------------------*
     *                              DTX-CNG                                 *
     *----------------------------------------------------------------------*/
    test();
    if (sub(newDTXState, SPEECH) != 0)     /* CNG mode */
    {
        /* increase slightly energy of noise below 200 Hz */

        /* Convert ISFs to the cosine domain */
        Isf_isp(isf, ispnew, M);

        Isp_Az(ispnew, Aq, M, 1);

        Copy(st->isfold, isf_tmp, M);

        for (i_subfr = 0; i_subfr < L_FRAME; i_subfr += L_SUBFR)
        {
            j = shr(i_subfr, 6);
            for (i = 0; i < M; i++)
            {
                L_tmp = L_mult(isf_tmp[i], sub(32767, interpol_frac[j]));
                L_tmp = L_mac(L_tmp, isf[i], interpol_frac[j]);
                HfIsf[i] = roundL(L_tmp);   move16();
            }
            synthesis(Aq, &exc2[i_subfr], 0, &synth16k[i_subfr * 5 / 4], (short) 1, HfIsf, nb_bits, newDTXState, st, bfi);
        }

        /* reset speech coder memories */
        Reset_decoder(st, 0);

        Copy(isf, st->isfold, M);

        st->prev_bfi = bfi;                move16();
        st->dtx_decSt->dtxGlobalState = newDTXState;    move16();

        return;
    }
    /*----------------------------------------------------------------------*
     *                               ACELP                                  *
     *----------------------------------------------------------------------*/

    /* copy coder memory state into working space (internal memory for DSP) */

    Copy(st->old_exc, old_exc, PIT_MAX + L_INTERPOL);
    exc = old_exc + PIT_MAX + L_INTERPOL;  move16();

    /* Decode the ISFs */
    test();
    if (sub(nb_bits, NBBITS_7k) <= 0)
    {
        ind[0] = Serial_parm(8, &prms);    move16();
        ind[1] = Serial_parm(8, &prms);    move16();
        ind[2] = Serial_parm(7, &prms);    move16();
        ind[3] = Serial_parm(7, &prms);    move16();
        ind[4] = Serial_parm(6, &prms);    move16();

        Dpisf_2s_36b(ind, isf, st->past_isfq, st->isfold, st->isf_buf, bfi, 1);
    } else
    {
        ind[0] = Serial_parm(8, &prms);    move16();
        ind[1] = Serial_parm(8, &prms);    move16();
        ind[2] = Serial_parm(6, &prms);    move16();
        ind[3] = Serial_parm(7, &prms);    move16();
        ind[4] = Serial_parm(7, &prms);    move16();
        ind[5] = Serial_parm(5, &prms);    move16();
        ind[6] = Serial_parm(5, &prms);    move16();

        Dpisf_2s_46b(ind, isf, st->past_isfq, st->isfold, st->isf_buf, bfi, 1);
    }

    /* Convert ISFs to the cosine domain */

    Isf_isp(isf, ispnew, M);
    test();
    if (st->first_frame != 0)
    {
        st->first_frame = 0;               move16();
        Copy(ispnew, st->ispold, M);
    }
    /* Find the interpolated ISPs and convert to a[] for all subframes */
    Int_isp(st->ispold, ispnew, interpol_frac, Aq);

    /* update ispold[] for the next frame */
    Copy(ispnew, st->ispold, M);

    /* Check stability on isf : distance between old isf and current isf */

    L_tmp = 0;                             move32();
    for (i = 0; i < M - 1; i++)
    {
        tmp = sub(isf[i], st->isfold[i]);
        L_tmp = L_mac(L_tmp, tmp, tmp);
    }
    tmp = extract_h(L_shl(L_tmp, 8));
    tmp = mult(tmp, 26214);                /* tmp = L_tmp*0.8/256 */

    tmp = sub(20480, tmp);                 /* 1.25 - tmp */
    stab_fac = shl(tmp, 1);                /* Q14 -> Q15 with saturation */
    test();
    if (stab_fac < 0)
    {
        stab_fac = 0;                      move16();
    }
    Copy(st->isfold, isf_tmp, M);
    Copy(isf, st->isfold, M);

    /*------------------------------------------------------------------------*
     *          Loop for every subframe in the analysis frame                 *
     *------------------------------------------------------------------------*
     * The subframe size is L_SUBFR and the loop is repeated L_FRAME/L_SUBFR  *
     *  times                                                                 *
     *     - decode the pitch delay and filter mode                           *
     *     - decode algebraic code                                            *
     *     - decode pitch and codebook gains                                  *
     *     - find voicing factor and tilt of code for next subframe.          *
     *     - find the excitation and compute synthesis speech                 *
     *------------------------------------------------------------------------*/

    p_Aq = Aq;                             move16();  /* pointer to interpolated LPC parameters */

    for (i_subfr = 0; i_subfr < L_FRAME; i_subfr += L_SUBFR)
    {
        pit_flag = i_subfr;                move16();

        test();test();
        if ((sub(i_subfr, 2 * L_SUBFR) == 0) && (sub(nb_bits, NBBITS_7k) > 0))
        {
            pit_flag = 0;                  move16();
        }
        /*-------------------------------------------------*
         * - Decode pitch lag                              *
         * Lag indeces received also in case of BFI,       *
         * so that the parameter pointer stays in sync.    *
         *-------------------------------------------------*/
        test();
        if (pit_flag == 0)
        {
            test();
            if (sub(nb_bits, NBBITS_9k) <= 0)
            {
                index = Serial_parm(8, &prms);
                test();
                if (sub(index, (PIT_FR1_8b - PIT_MIN) * 2) < 0)
                {
                    T0 = add(PIT_MIN, shr(index, 1));
                    T0_frac = sub(index, shl(sub(T0, PIT_MIN), 1));
                    T0_frac = shl(T0_frac, 1);
                } else
                {
                    T0 = add(index, PIT_FR1_8b - ((PIT_FR1_8b - PIT_MIN) * 2));
                    T0_frac = 0;           move16();
                }
            } else
            {
                index = Serial_parm(9, &prms);
                test();test();
                if (sub(index, (PIT_FR2 - PIT_MIN) * 4) < 0)
                {
                    T0 = add(PIT_MIN, shr(index, 2));
                    T0_frac = sub(index, shl(sub(T0, PIT_MIN), 2));
                } else if (sub(index, (((PIT_FR2 - PIT_MIN) * 4) + ((PIT_FR1_9b - PIT_FR2) * 2))) < 0)
                {
                    index = sub(index, (PIT_FR2 - PIT_MIN) * 4);
                    T0 = add(PIT_FR2, shr(index, 1));
                    T0_frac = sub(index, shl(sub(T0, PIT_FR2), 1));
                    T0_frac = shl(T0_frac, 1);
                } else
                {
                    T0 = add(index, (PIT_FR1_9b - ((PIT_FR2 - PIT_MIN) * 4) - ((PIT_FR1_9b - PIT_FR2) * 2)));
                    T0_frac = 0;           move16();
                }
            }

            /* find T0_min and T0_max for subframe 2 and 4 */

            T0_min = sub(T0, 8);
            test();
            if (sub(T0_min, PIT_MIN) < 0)
            {
                T0_min = PIT_MIN;          move16();
            }
            T0_max = add(T0_min, 15);
            test();
            if (sub(T0_max, PIT_MAX) > 0)
            {
                T0_max = PIT_MAX;          move16();
                T0_min = sub(T0_max, 15);
            }
        } else
        {                                  /* if subframe 2 or 4 */
            test();
            if (sub(nb_bits, NBBITS_9k) <= 0)
            {
                index = Serial_parm(5, &prms);

                T0 = add(T0_min, shr(index, 1));
                T0_frac = sub(index, shl(sub(T0, T0_min), 1));
                T0_frac = shl(T0_frac, 1);
            } else
            {
                index = Serial_parm(6, &prms);

                T0 = add(T0_min, shr(index, 2));
                T0_frac = sub(index, shl(sub(T0, T0_min), 2));
            }
        }

        /* check BFI after pitch lag decoding */
        test();
        if (bfi != 0)                      /* if frame erasure */
        {
            lagconc(&(st->dec_gain[17]), st->lag_hist, &T0, &(st->old_T0), &(st->seed3), unusable_frame);
            T0_frac = 0;                   move16();
        }
        /*-------------------------------------------------*
         * - Find the pitch gain, the interpolation filter *
         *   and the adaptive codebook vector.             *
         *-------------------------------------------------*/

        Pred_lt4(&exc[i_subfr], T0, T0_frac, L_SUBFR + 1);

        test();
        if (unusable_frame)
        {
            select = 1;                    move16();
        } else
        {
            test();
            if (sub(nb_bits, NBBITS_9k) <= 0)
            {
                select = 0;                move16();
            } else
            {
                select = Serial_parm(1, &prms);
            }
        }

        test();
        if (select == 0)
        {
            /* find pitch excitation with lp filter */
            for (i = 0; i < L_SUBFR; i++)
            {
                L_tmp = L_mult(5898, exc[i - 1 + i_subfr]);
                L_tmp = L_mac(L_tmp, 20972, exc[i + i_subfr]);
                L_tmp = L_mac(L_tmp, 5898, exc[i + 1 + i_subfr]);
                code[i] = roundL(L_tmp);    move16();
            }
            Copy(code, &exc[i_subfr], L_SUBFR);
        }
        /*-------------------------------------------------------*
         * - Decode innovative codebook.                         *
         * - Add the fixed-gain pitch contribution to code[].    *
         *-------------------------------------------------------*/
        test();test();test();test();test();test();test();test();
        if (unusable_frame != 0)
        {
            /* the innovative code doesn't need to be scaled (see Q_gain2) */
            for (i = 0; i < L_SUBFR; i++)
            {
                code[i] = shr(Random(&(st->seed)), 3);  move16();
            }
        } else if (sub(nb_bits, NBBITS_7k) <= 0)
        {
            ind[0] = Serial_parm(12, &prms);    move16();
            DEC_ACELP_2t64_fx(ind[0], code);
        } else if (sub(nb_bits, NBBITS_9k) <= 0)
        {
            for (i = 0; i < 4; i++)
            {
                ind[i] = Serial_parm(5, &prms); move16();
            }
            DEC_ACELP_4t64_fx(ind, 20, code);
        } else if (sub(nb_bits, NBBITS_12k) <= 0)
        {
            for (i = 0; i < 4; i++)
            {
                ind[i] = Serial_parm(9, &prms); move16();
            }
            DEC_ACELP_4t64_fx(ind, 36, code);
        } else if (sub(nb_bits, NBBITS_14k) <= 0)
        {
            ind[0] = Serial_parm(13, &prms);    move16();
            ind[1] = Serial_parm(13, &prms);    move16();
            ind[2] = Serial_parm(9, &prms);move16();
            ind[3] = Serial_parm(9, &prms);move16();
            DEC_ACELP_4t64_fx(ind, 44, code);
        } else if (sub(nb_bits, NBBITS_16k) <= 0)
        {
            for (i = 0; i < 4; i++)
            {
                ind[i] = Serial_parm(13, &prms);        move16();
            }
            DEC_ACELP_4t64_fx(ind, 52, code);
        } else if (sub(nb_bits, NBBITS_18k) <= 0)
        {
            for (i = 0; i < 4; i++)
            {
                ind[i] = Serial_parm(2, &prms); move16();
            }
            for (i = 4; i < 8; i++)
            {
                ind[i] = Serial_parm(14, &prms);        move16();
            }
            DEC_ACELP_4t64_fx(ind, 64, code);
        } else if (sub(nb_bits, NBBITS_20k) <= 0)
        {
            ind[0] = Serial_parm(10, &prms);    move16();
            ind[1] = Serial_parm(10, &prms);    move16();
            ind[2] = Serial_parm(2, &prms);move16();
            ind[3] = Serial_parm(2, &prms);move16();
            ind[4] = Serial_parm(10, &prms);    move16();
            ind[5] = Serial_parm(10, &prms);    move16();
            ind[6] = Serial_parm(14, &prms);    move16();
            ind[7] = Serial_parm(14, &prms);    move16();
            DEC_ACELP_4t64_fx(ind, 72, code);
        } else
        {
            for (i = 0; i < 4; i++)
            {
                ind[i] = Serial_parm(11, &prms);        move16();
            }
            for (i = 4; i < 8; i++)
            {
                ind[i] = Serial_parm(11, &prms);        move16();
            }
            DEC_ACELP_4t64_fx(ind, 88, code);
        }

        tmp = 0;                           move16();
        Preemph(code, st->tilt_code, L_SUBFR, &tmp);

        tmp = T0;                          move16();
        test();
        if (sub(T0_frac, 2) > 0)
        {
            tmp = add(tmp, 1);
        }
        Pit_shrp(code, tmp, PIT_SHARP, L_SUBFR);

        /*-------------------------------------------------*
         * - Decode codebooks gains.                       *
         *-------------------------------------------------*/
        test();
        if (sub(nb_bits, NBBITS_9k) <= 0)
        {
            index = Serial_parm(6, &prms); /* codebook gain index */

            D_gain2(index, 6, code, L_SUBFR, &gain_pit, &L_gain_code, bfi, st->prev_bfi, st->state, unusable_frame, st->vad_hist, st->dec_gain);
        } else
        {
            index = Serial_parm(7, &prms); /* codebook gain index */

            D_gain2(index, 7, code, L_SUBFR, &gain_pit, &L_gain_code, bfi, st->prev_bfi, st->state, unusable_frame, st->vad_hist, st->dec_gain);
        }

        /* find best scaling to perform on excitation (Q_new) */

        tmp = st->Qsubfr[0];
        for (i = 1; i < 4; i++)
        {
            test();move16();
            if (sub(st->Qsubfr[i], tmp) < 0)
            {
                tmp = st->Qsubfr[i];       move16();
            }
        }

        /* limit scaling (Q_new) to Q_MAX: see cnst.h and syn_filt_32() */
        test();
        if (sub(tmp, Q_MAX) > 0)
        {
            tmp = Q_MAX;                   move16();
        }
        Q_new = 0;                         move16();
        L_tmp = L_gain_code;               move32();  /* L_gain_code in Q16 */

        test();test();
        while ((L_sub(L_tmp, 0x08000000L) < 0) && (sub(Q_new, tmp) < 0))
        {
            L_tmp = L_shl(L_tmp, 1);
            Q_new = add(Q_new, 1);
            test();test();
        }
        gain_code = roundL(L_tmp);          /* scaled gain_code with Qnew */

        Scale_sig(exc + i_subfr - (PIT_MAX + L_INTERPOL),
            PIT_MAX + L_INTERPOL + L_SUBFR, sub(Q_new, st->Q_old));
        st->Q_old = Q_new;                 move16();


        /*----------------------------------------------------------*
         * Update parameters for the next subframe.                 *
         * - tilt of code: 0.0 (unvoiced) to 0.5 (voiced)           *
         *----------------------------------------------------------*/

        test();
        if (bfi == 0)
        {
            /* LTP-Lag history update */
            for (i = 4; i > 0; i--)
            {
                st->lag_hist[i] = st->lag_hist[i - 1];  move16();
            }
            st->lag_hist[0] = T0;          move16();

            st->old_T0 = T0;               move16();
            st->old_T0_frac = 0;           move16();  /* Remove fraction in case of BFI */
        }
        /* find voice factor in Q15 (1=voiced, -1=unvoiced) */

        Copy(&exc[i_subfr], exc2, L_SUBFR);
        Scale_sig(exc2, L_SUBFR, -3);

        /* post processing of excitation elements */
        test();
        if (sub(nb_bits, NBBITS_9k) <= 0)
        {
            pit_sharp = shl(gain_pit, 1);
            test();
            if (sub(pit_sharp, 16384) > 0)
            {
                for (i = 0; i < L_SUBFR; i++)
                {
                    tmp = mult(exc2[i], pit_sharp);
                    L_tmp = L_mult(tmp, gain_pit);
                    L_tmp = L_shr(L_tmp, 1);
                    excp[i] = roundL(L_tmp);
                    move16();
                }
            }
        } else
        {
            pit_sharp = 0;                 move16();
        }

        voice_fac = voice_factor(exc2, -3, gain_pit, code, gain_code, L_SUBFR);

        /* tilt of code for next subframe: 0.5=voiced, 0=unvoiced */

        st->tilt_code = add(shr(voice_fac, 2), 8192);   move16();

        /*-------------------------------------------------------*
         * - Find the total excitation.                          *
         * - Find synthesis speech corresponding to exc[].       *
         *-------------------------------------------------------*/

        Copy(&exc[i_subfr], exc2, L_SUBFR);

        for (i = 0; i < L_SUBFR; i++)
        {
            L_tmp = L_mult(code[i], gain_code);
            L_tmp = L_shl(L_tmp, 5);
            L_tmp = L_mac(L_tmp, exc[i + i_subfr], gain_pit);
            L_tmp = L_shl(L_tmp, 1);
            exc[i + i_subfr] = roundL(L_tmp);    move16();
        }

        /* find maximum value of excitation for next scaling */

        max = 1;                           move16();
        for (i = 0; i < L_SUBFR; i++)
        {
            tmp = abs_s(exc[i + i_subfr]);
            test();
            if (sub(tmp, max) > 0)
            {
                max = tmp;                 move16();
            }
        }

        /* tmp = scaling possible according to max value of excitation */
        tmp = sub(add(norm_s(max), Q_new), 1);

        st->Qsubfr[3] = st->Qsubfr[2];     move16();
        st->Qsubfr[2] = st->Qsubfr[1];     move16();
        st->Qsubfr[1] = st->Qsubfr[0];     move16();
        st->Qsubfr[0] = tmp;               move16();

        /*------------------------------------------------------------*
         * phase dispersion to enhance noise in low bit rate          *
         *------------------------------------------------------------*/

        /* L_gain_code in Q16 */
        L_Extract(L_gain_code, &gain_code, &gain_code_lo);
        test();test();move16();
        if (sub(nb_bits, NBBITS_7k) <= 0)
            j = 0;                         /* high dispersion for rate <= 7.5 kbit/s */
        else if (sub(nb_bits, NBBITS_9k) <= 0)
            j = 1;                         /* low dispersion for rate <= 9.6 kbit/s */
        else
            j = 2;                         /* no dispersion for rate > 9.6 kbit/s */

        Phase_dispersion(gain_code, gain_pit, code, j, st->disp_mem);

        /*------------------------------------------------------------*
         * noise enhancer                                             *
         * ~~~~~~~~~~~~~~                                             *
         * - Enhance excitation on noise. (modify gain of code)       *
         *   If signal is noisy and LPC filter is stable, move gain   *
         *   of code 1.5 dB toward gain of code threshold.            *
         *   This decrease by 3 dB noise energy variation.            *
         *------------------------------------------------------------*/

        tmp = sub(16384, shr(voice_fac, 1));    /* 1=unvoiced, 0=voiced */
        fac = mult(stab_fac, tmp);

        L_tmp = L_gain_code;               move32();
        test();
        if (L_sub(L_tmp, st->L_gc_thres) < 0)
        {
            L_tmp = L_add(L_tmp, Mpy_32_16(gain_code, gain_code_lo, 6226));
            test();
            if (L_sub(L_tmp, st->L_gc_thres) > 0)
            {
                L_tmp = st->L_gc_thres;    move32();
            }
        } else
        {
            L_tmp = Mpy_32_16(gain_code, gain_code_lo, 27536);
            test();
            if (L_sub(L_tmp, st->L_gc_thres) < 0)
            {
                L_tmp = st->L_gc_thres;    move32();
            }
        }
        st->L_gc_thres = L_tmp;            move32();

        L_gain_code = Mpy_32_16(gain_code, gain_code_lo, sub(32767, fac));
        L_Extract(L_tmp, &gain_code, &gain_code_lo);
        L_gain_code = L_add(L_gain_code, Mpy_32_16(gain_code, gain_code_lo, fac));

        /*------------------------------------------------------------*
         * pitch enhancer                                             *
         * ~~~~~~~~~~~~~~                                             *
         * - Enhance excitation on voice. (HP filtering of code)      *
         *   On voiced signal, filtering of code by a smooth fir HP   *
         *   filter to decrease energy of code in low frequency.      *
         *------------------------------------------------------------*/

        tmp = add(shr(voice_fac, 3), 4096);/* 0.25=voiced, 0=unvoiced */

        L_tmp = L_deposit_h(code[0]);
        L_tmp = L_msu(L_tmp, code[1], tmp);
        code2[0] = roundL(L_tmp);
        move16();

        for (i = 1; i < L_SUBFR - 1; i++)
        {
            L_tmp = L_deposit_h(code[i]);
            L_tmp = L_msu(L_tmp, code[i + 1], tmp);
            L_tmp = L_msu(L_tmp, code[i - 1], tmp);
            code2[i] = roundL(L_tmp);
            move16();
        }

        L_tmp = L_deposit_h(code[L_SUBFR - 1]);
        L_tmp = L_msu(L_tmp, code[L_SUBFR - 2], tmp);
        code2[L_SUBFR - 1] = roundL(L_tmp);
        move16();

        /* build excitation */

        gain_code = roundL(L_shl(L_gain_code, Q_new));

        for (i = 0; i < L_SUBFR; i++)
        {
            L_tmp = L_mult(code2[i], gain_code);
            L_tmp = L_shl(L_tmp, 5);
            L_tmp = L_mac(L_tmp, exc2[i], gain_pit);
            L_tmp = L_shl(L_tmp, 1);       /* saturation can occur here */
            exc2[i] = roundL(L_tmp);
            move16();
        }

        if (sub(nb_bits, NBBITS_9k) <= 0)
        {
            if (sub(pit_sharp, 16384) > 0)
            {
                for (i = 0; i < L_SUBFR; i++)
                {
                    excp[i] = add(excp[i], exc2[i]);
                    move16();
                }
                agc2(exc2, excp, L_SUBFR);
                Copy(excp, exc2, L_SUBFR);
            }
        }
        if (sub(nb_bits, NBBITS_7k) <= 0)
        {
            j = shr(i_subfr, 6);
            for (i = 0; i < M; i++)
            {
                L_tmp = L_mult(isf_tmp[i], sub(32767, interpol_frac[j]));
                L_tmp = L_mac(L_tmp, isf[i], interpol_frac[j]);
                HfIsf[i] = roundL(L_tmp);
            }
        } else
        {
            Set_zero(st->mem_syn_hf, M16k - M);
        }

        if (sub(nb_bits, NBBITS_24k) >= 0)
        {
            corr_gain = Serial_parm(4, &prms);
            synthesis(p_Aq, exc2, Q_new, &synth16k[i_subfr * 5 / 4], corr_gain, HfIsf, nb_bits, newDTXState, st, bfi);
        } else
            synthesis(p_Aq, exc2, Q_new, &synth16k[i_subfr * 5 / 4], 0, HfIsf, nb_bits, newDTXState, st, bfi);

        p_Aq += (M + 1);                   /* interpolated LPC parameters for next subframe */
    }

    /*--------------------------------------------------*
     * Update signal for next frame.                    *
     * -> save past of exc[].                           *
     * -> save pitch parameters.                        *
     *--------------------------------------------------*/

    Copy(&old_exc[L_FRAME], st->old_exc, PIT_MAX + L_INTERPOL);

    Scale_sig(exc, L_FRAME, sub(0, Q_new));
    dtx_dec_activity_update(st->dtx_decSt, isf, exc);

    st->dtx_decSt->dtxGlobalState = newDTXState;        move16();

    st->prev_bfi = bfi;                    move16();

    return;
}



/*-----------------------------------------------------*
 * Function synthesis()                                *
 *                                                     *
 * Synthesis of signal at 16kHz with HF extension.     *
 *                                                     *
 *-----------------------------------------------------*/

static void synthesis(
     Word16 Aq[],                          /* A(z)  : quantized Az               */
     Word16 exc[],                         /* (i)   : excitation at 12kHz        */
     Word16 Q_new,                         /* (i)   : scaling performed on exc   */
     Word16 synth16k[],                    /* (o)   : 16kHz synthesis signal     */
     Word16 prms,                          /* (i)   : parameter                  */
     Word16 HfIsf[],
     Word16 nb_bits,
     Word16 newDTXState,
     Decoder_State * st,                   /* (i/o) : State structure            */
     Word16 bfi                            /* (i)   : bad frame indicator        */
)
{
    Word16 i, fac, tmp, exp;
    Word16 ener, exp_ener;
    Word32 L_tmp;

    Word16 synth_hi[M + L_SUBFR], synth_lo[M + L_SUBFR];
    Word16 synth[L_SUBFR];
    Word16 HF[L_SUBFR16k];                 /* High Frequency vector      */
    Word16 Ap[M16k + 1];
    Word16 HfA[M16k + 1];
    Word16 HF_corr_gain;
    Word16 HF_gain_ind;
    Word16 gain1, gain2;
    Word16 weight1, weight2;

    /*------------------------------------------------------------*
     * speech synthesis                                           *
     * ~~~~~~~~~~~~~~~~                                           *
     * - Find synthesis speech corresponding to exc2[].           *
     * - Perform fixed deemphasis and hp 50hz filtering.          *
     * - Oversampling from 12.8kHz to 16kHz.                      *
     *------------------------------------------------------------*/

    Copy(st->mem_syn_hi, synth_hi, M);
    Copy(st->mem_syn_lo, synth_lo, M);

    Syn_filt_32(Aq, M, exc, Q_new, synth_hi + M, synth_lo + M, L_SUBFR);

    Copy(synth_hi + L_SUBFR, st->mem_syn_hi, M);
    Copy(synth_lo + L_SUBFR, st->mem_syn_lo, M);

    Deemph_32(synth_hi + M, synth_lo + M, synth, PREEMPH_FAC, L_SUBFR, &(st->mem_deemph));

    HP50_12k8(synth, L_SUBFR, st->mem_sig_out);

    Oversamp_16k(synth, L_SUBFR, synth16k, st->mem_oversamp);

    /*------------------------------------------------------*
    * HF noise synthesis                                   *
    * ~~~~~~~~~~~~~~~~~~                                   *
    * - Generate HF noise between 5.5 and 7.5 kHz.         *
    * - Set energy of noise according to synthesis tilt.   *
    *     tilt > 0.8 ==> - 14 dB (voiced)                  *
    *     tilt   0.5 ==> - 6 dB  (voiced or noise)         *
    *     tilt < 0.0 ==>   0 dB  (noise)                   *
    *------------------------------------------------------*/

    /* generate white noise vector */
    for (i = 0; i < L_SUBFR16k; i++)
    {
        HF[i] = shr(Random(&(st->seed2)), 3);   move16();
    }
    /* energy of excitation */

    Scale_sig(exc, L_SUBFR, -3);
    Q_new = sub(Q_new, 3);

    ener = extract_h(Dot_product12(exc, exc, L_SUBFR, &exp_ener));
    exp_ener = sub(exp_ener, add(Q_new, Q_new));

    /* set energy of white noise to energy of excitation */

    tmp = extract_h(Dot_product12(HF, HF, L_SUBFR16k, &exp));
    test();
    if (sub(tmp, ener) > 0)
    {
        tmp = shr(tmp, 1);                 /* Be sure tmp < ener */
        exp = add(exp, 1);
    }
    L_tmp = L_deposit_h(div_s(tmp, ener)); /* result is normalized */
    exp = sub(exp, exp_ener);
    Isqrt_n(&L_tmp, &exp);
    L_tmp = L_shl(L_tmp, add(exp, 1));     /* L_tmp x 2, L_tmp in Q31 */
    tmp = extract_h(L_tmp);                /* tmp = 2 x sqrt(ener_exc/ener_hf) */
    for (i = 0; i < L_SUBFR16k; i++)
    {
        HF[i] = mult(HF[i], tmp);          move16();
    }
    /* find tilt of synthesis speech (tilt: 1=voiced, -1=unvoiced) */

    HP400_12k8(synth, L_SUBFR, st->mem_hp400);

    L_tmp = 1L;                            move32();
    for (i = 0; i < L_SUBFR; i++)
        L_tmp = L_mac(L_tmp, synth[i], synth[i]);

    exp = norm_l(L_tmp);
    ener = extract_h(L_shl(L_tmp, exp));   /* ener = r[0] */

    L_tmp = 1L;                            move32();
    for (i = 1; i < L_SUBFR; i++)
        L_tmp = L_mac(L_tmp, synth[i], synth[i - 1]);

    tmp = extract_h(L_shl(L_tmp, exp));    /* tmp = r[1] */
    test();
    if (tmp > 0)
    {
        fac = div_s(tmp, ener);
    } else
    {
        fac = 0;                           move16();
    }

    /* modify energy of white noise according to synthesis tilt */
    gain1 = sub(32767, fac);
    gain2 = mult(sub(32767, fac), 20480);
    gain2 = shl(gain2, 1);

    test();
    if (st->vad_hist > 0)
    {
        weight1 = 0;                       move16();
        weight2 = 32767;                   move16();
    } else
    {
        weight1 = 32767;                   move16();
        weight2 = 0;                       move16();
    }
    tmp = mult(weight1, gain1);
    tmp = add(tmp, mult(weight2, gain2));

    test();
    if (tmp != 0)
    {
        tmp = add(tmp, 1);
    }
    test();
    if (sub(tmp, 3277) < 0)
    {
        tmp = 3277;                        /* 0.1 in Q15 */
        move16();
    }
    test(); test();
    if ((sub(nb_bits, NBBITS_24k) >= 0 ) && (bfi == 0))
    {
        /* HF correction gain */
        HF_gain_ind = prms;
        HF_corr_gain = HP_gain[HF_gain_ind];

        /* HF gain */
        for (i = 0; i < L_SUBFR16k; i++)
        {
            HF[i] = shl(mult(HF[i], HF_corr_gain), 1);  move16();
        }
    } else
    {
        for (i = 0; i < L_SUBFR16k; i++)
        {
            HF[i] = mult(HF[i], tmp);      move16();
        }
    }

    test();test();
    if ((sub(nb_bits, NBBITS_7k) <= 0) && (sub(newDTXState, SPEECH) == 0))
    {
        Isf_Extrapolation(HfIsf);
        Isp_Az(HfIsf, HfA, M16k, 0);

        Weight_a(HfA, Ap, 29491, M16k);    /* fac=0.9 */
        Syn_filt(Ap, M16k, HF, HF, L_SUBFR16k, st->mem_syn_hf, 1);
    } else
    {
        /* synthesis of noise: 4.8kHz..5.6kHz --> 6kHz..7kHz */
        Weight_a(Aq, Ap, 19661, M);        /* fac=0.6 */
        Syn_filt(Ap, M, HF, HF, L_SUBFR16k, st->mem_syn_hf + (M16k - M), 1);
    }

    /* noise High Pass filtering (1ms of delay) */
    Filt_6k_7k(HF, L_SUBFR16k, st->mem_hf);

    test();
    if (sub(nb_bits, NBBITS_24k) >= 0)
    {
        /* Low Pass filtering (7 kHz) */
        Filt_7k(HF, L_SUBFR16k, st->mem_hf3);
    }
    /* add filtered HF noise to speech synthesis */
    for (i = 0; i < L_SUBFR16k; i++)
    {
        synth16k[i] = add(synth16k[i], HF[i]);  move16();
    }

    return;
}
