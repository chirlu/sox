/*--------------------------------------------------------------------------*
 *                         DEC_MAIN.H                                       *
 *--------------------------------------------------------------------------*
 *       Static memory in the decoder                                       *
 *--------------------------------------------------------------------------*/

#include "cnst.h"                          /* coder constant parameters */
#include "dtx.h"

typedef struct
{
    Word16 old_exc[PIT_MAX + L_INTERPOL];  /* old excitation vector */
    Word16 ispold[M];                      /* old isp (immittance spectral pairs) */
    Word16 isfold[M];                      /* old isf (frequency domain) */
    Word16 isf_buf[L_MEANBUF * M];         /* isf buffer(frequency domain) */
    Word16 past_isfq[M];                   /* past isf quantizer */
    Word16 tilt_code;                      /* tilt of code */
    Word16 Q_old;                          /* old scaling factor */
    Word16 Qsubfr[4];                      /* old maximum scaling factor */
    Word32 L_gc_thres;                     /* threshold for noise enhancer */
    Word16 mem_syn_hi[M];                  /* modified synthesis memory (MSB) */
    Word16 mem_syn_lo[M];                  /* modified synthesis memory (LSB) */
    Word16 mem_deemph;                     /* speech deemph filter memory */
    Word16 mem_sig_out[6];                 /* hp50 filter memory for synthesis */
    Word16 mem_oversamp[2 * L_FILT];       /* synthesis oversampled filter memory */
    Word16 mem_syn_hf[M16k];               /* HF synthesis memory */
    Word16 mem_hf[2 * L_FILT16k];          /* HF band-pass filter memory */
    Word16 mem_hf2[2 * L_FILT16k];         /* HF band-pass filter memory */
    Word16 mem_hf3[2 * L_FILT16k];         /* HF band-pass filter memory */
    Word16 seed;                           /* random memory for frame erasure */
    Word16 seed2;                          /* random memory for HF generation */
    Word16 old_T0;                         /* old pitch lag */
    Word16 old_T0_frac;                    /* old pitch fraction lag */
    Word16 lag_hist[5];
    Word16 dec_gain[23];                   /* gain decoder memory */
    Word16 seed3;                          /* random memory for lag concealment */
    Word16 disp_mem[8];                    /* phase dispersion memory */
    Word16 mem_hp400[6];                   /* hp400 filter memory for synthesis */

    Word16 prev_bfi;
    Word16 state;
    Word16 first_frame;
    dtx_decState *dtx_decSt;
    Word16 vad_hist;

} Decoder_State;
