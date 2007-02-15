/*--------------------------------------------------------------------------*
 *                         DTX.H                                            *
 *--------------------------------------------------------------------------*
 *       Static memory, constants and frametypes for the DTX                *
 *--------------------------------------------------------------------------*/


#ifndef dtx_h
#define dtx_h

#define DTX_MAX_EMPTY_THRESH 50
#define DTX_HIST_SIZE 8
#define DTX_HIST_SIZE_MIN_ONE 7
#define DTX_ELAPSED_FRAMES_THRESH (24 + 7 -1)
#define DTX_HANG_CONST 7                   /* yields eight frames of SP HANGOVER  */
#define INV_MED_THRESH 14564
#define ISF_GAP  128                       /* 50 */
#define ONE_MINUS_ISF_GAP 16384 - ISF_GAP

#define ISF_GAP   128
#define ISF_DITH_GAP   448
#define ISF_FACTOR_LOW 256
#define ISF_FACTOR_STEP 2

#define GAIN_THR 180
#define GAIN_FACTOR 75

typedef struct
{
    Word16 isf_hist[M * DTX_HIST_SIZE];
    Word16 log_en_hist[DTX_HIST_SIZE];
    Word16 hist_ptr;
    Word16 log_en_index;
    Word16 cng_seed;

    /* DTX handler stuff */
    Word16 dtxHangoverCount;
    Word16 decAnaElapsedCount;
    Word32 D[28];
    Word32 sumD[DTX_HIST_SIZE];
} dtx_encState;

#define SPEECH 0
#define DTX 1
#define DTX_MUTE 2

#define TX_SPEECH 0
#define TX_SID_FIRST 1
#define TX_SID_UPDATE 2
#define TX_NO_DATA 3

#define RX_SPEECH_GOOD 0
#define RX_SPEECH_PROBABLY_DEGRADED 1
#define RX_SPEECH_LOST 2
#define RX_SPEECH_BAD 3
#define RX_SID_FIRST 4
#define RX_SID_UPDATE 5
#define RX_SID_BAD 6
#define RX_NO_DATA 7

/*****************************************************************************
 *
 * DEFINITION OF DATA TYPES
 *****************************************************************************/

typedef struct
{
    Word16 since_last_sid;
    Word16 true_sid_period_inv;
    Word16 log_en;
    Word16 old_log_en;
    Word16 level;
    Word16 isf[M];
    Word16 isf_old[M];
    Word16 cng_seed;

    Word16 isf_hist[M * DTX_HIST_SIZE];
    Word16 log_en_hist[DTX_HIST_SIZE];
    Word16 hist_ptr;

    Word16 dtxHangoverCount;
    Word16 decAnaElapsedCount;

    Word16 sid_frame;
    Word16 valid_data;
    Word16 dtxHangoverAdded;

    Word16 dtxGlobalState;                 /* contains previous state */
    /* updated in main decoder */

    Word16 data_updated;                   /* marker to know if CNI data is ever renewed */

    Word16 dither_seed;
    Word16 CN_dith;

} dtx_decState;

Word16 dtx_enc_init(dtx_encState ** st, Word16 isf_init[]);
Word16 dtx_enc_reset(dtx_encState * st, Word16 isf_init[]);
void dtx_enc_exit(dtx_encState ** st);

Word16 dtx_enc(
     dtx_encState * st,                    /* i/o : State struct                                         */
     Word16 isf[M],                        /* o   : CN ISF vector                                        */
     Word16 * exc2,                        /* o   : CN excitation                                        */
     Word16 ** prms
);

Word16 dtx_buffer(
     dtx_encState * st,                    /* i/o : State struct                    */
     Word16 isf_new[],                     /* i   : isf vector                      */
     Word32 enr,                           /* i   : residual energy (in L_FRAME)    */
     Word16 codec_mode
);

void tx_dtx_handler(dtx_encState * st,     /* i/o : State struct           */
     Word16 vad_flag,                      /* i   : vad decision           */
     Word16 * usedMode                     /* i/o : mode changed or not    */
);

void Qisf_ns(
     Word16 * isf1,                        /* input : ISF in the frequency domain (0..0.5) */
     Word16 * isf_q,                       /* output: quantized ISF                        */
     Word16 * indice                       /* output: quantization indices                 */
);


Word16 dtx_dec_init(dtx_decState ** st, Word16 isf_init[]);
Word16 dtx_dec_reset(dtx_decState * st, Word16 isf_init[]);
void dtx_dec_exit(dtx_decState ** st);

Word16 dtx_dec(
     dtx_decState * st,                    /* i/o : State struct                                          */
     Word16 * exc2,                        /* o   : CN excitation                                          */
     Word16 new_state,                     /* i   : New DTX state                                          */
     Word16 isf[],                         /* o   : CN ISF vector                                          */
     Word16 ** prms
);

void dtx_dec_activity_update(
     dtx_decState * st,
     Word16 isf[],
     Word16 exc[]);


Word16 rx_dtx_handler(
     dtx_decState * st,                    /* i/o : State struct     */
     Word16 frame_type                     /* i   : Frame type       */
);

void Disf_ns(
     Word16 * indice,                      /* input:  quantization indices                  */
     Word16 * isf_q                        /* input : ISF in the frequency domain (0..0.5)  */
);

#endif
