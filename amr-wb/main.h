/*--------------------------------------------------------------------------*
 *                         MAIN.H                                           *
 *--------------------------------------------------------------------------*
 *       Main functions                                                     *
 *--------------------------------------------------------------------------*/

void Init_coder(void **spe_state);
void Close_coder(void *spe_state);

void coder(
     Word16 * mode,                        /* input :  used mode                             */
     Word16 speech16k[],                   /* input :  320 new speech samples (at 16 kHz)    */
     Word16 prms[],                        /* output:  output parameters           */
     Word16 * ser_size,                    /* output:  bit rate of the used mode   */
     void *spe_state,                      /* i/o   :  State structure                       */
     int allow_dtx                      /* input :  DTX ON/OFF                            */
);

void Init_decoder(void **spd_state);
void Close_decoder(void *spd_state);

void decoder(
     int mode,                          /* input : used mode                     */
     Word16 prms[],                        /* input : parameter vector                     */
     Word16 synth16k[],                    /* output: synthesis speech              */
     Word16 * frame_length,                /* output:  lenght of the frame         */
     void *spd_state,                      /* i/o   : State structure                      */
     int  frame_type                     /* input : received frame type           */
);

void Reset_encoder(void *st, int reset_all);

void Reset_decoder(void *st, int reset_all);

Word16 encoder_homing_frame_test(Word16 input_frame[]);

Word16 decoder_homing_frame_test(Word16 input_frame[], int mode);

Word16 decoder_homing_frame_test_first(Word16 input_frame[], int mode);
