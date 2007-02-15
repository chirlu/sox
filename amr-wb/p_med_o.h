/*--------------------------------------------------------------------------*
 *                         P_MED_O.H                                        *
 *--------------------------------------------------------------------------*
 *       Median open-loop lag search                                        *
 *--------------------------------------------------------------------------*/

Word16 Pitch_med_ol(                       /* output: open loop pitch lag                        */
     Word16 wsp[],                         /* input : signal used to compute the open loop pitch */
                                           /* wsp[-pit_max] to wsp[-1] should be known   */
     Word16 L_min,                         /* input : minimum pitch lag                          */
     Word16 L_max,                         /* input : maximum pitch lag                          */
     Word16 L_frame,                       /* input : length of frame to compute pitch           */
     Word16 L_0,                           /* input : old_ open-loop pitch                       */
     Word16 * gain,                        /* output: normalize correlation of hp_wsp for the Lag */
     Word16 * hp_wsp_mem,                  /* i:o   : memory of the hypass filter for hp_wsp[] (lg=9)   */
     Word16 * old_hp_wsp,                  /* i:o   : hypass wsp[]                               */
     Word16 wght_flg                       /* input : is weighting function used                 */
);
Word16 Med_olag(                           /* output : median of  5 previous open-loop lags       */
     Word16 prev_ol_lag,                   /* input  : previous open-loop lag                     */
     Word16 old_ol_lag[5]
);
void Hp_wsp(
     Word16 wsp[],                         /* i   : wsp[]  signal       */
     Word16 hp_wsp[],                      /* o   : hypass wsp[]        */
     Word16 lg,                            /* i   : lenght of signal    */
     Word16 mem[]                          /* i/o : filter memory [9]   */
);
