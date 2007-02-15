/*------------------------------------------------------------------------*
 *                         HOMING.C                                       *
 *------------------------------------------------------------------------*
 * Performs the homing routines                                           *
 *------------------------------------------------------------------------*/

#include "typedef.h"
#include "cnst.h"
#include "basic_op.h"
#include "bits.h"

#include "homing.tab"

Word16 encoder_homing_frame_test(Word16 input_frame[])
{
    Word16 i, j = 0;
    
    /* check 320 input samples for matching EHF_MASK: defined in e_homing.h */
    for (i = 0; i < L_FRAME16k; i++)
    {
        j = (Word16) (input_frame[i] ^ EHF_MASK);
        
        if (j)
            break;
    }
    
    return (Word16) (!j);
}

static Word16 dhf_test(Word16 input_frame[], Word16 mode, Word16 nparms)
{
    Word16 i, j, tmp, shift;
    Word16 param[DHF_PARMS_MAX];
    Word16 *prms;
    
    prms = input_frame;
    j = 0;
    i = 0;
    
    if (sub(mode, MRDTX) != 0)
    {
        if (sub(mode, MODE_24k) != 0) 
        {
            /* convert the received serial bits */
            tmp = sub(nparms, 15);
            while (sub(tmp, j) > 0)
            {
                param[i] = Serial_parm(15, &prms);
                j = add(j, 15);
                i = add(i, 1);
            }
            tmp = sub(nparms, j);
            param[i] = Serial_parm(tmp, &prms);
            shift = sub(15, tmp);
            param[i] = shl(param[i], shift);
        }
        else 
        {
            /*If mode is 23.85Kbit/s, remove high band energy bits */
            for (i = 0; i < 10; i++)
            {
                param[i] = Serial_parm(15, &prms);
            }
            param[10] = Serial_parm(15, &prms) & 0x61FF;
            for (i = 11; i < 17; i++)
            {
                param[i] = Serial_parm(15, &prms);
            }
            param[17] = Serial_parm(15, &prms) & 0xE0FF;
            for (i = 18; i < 24; i++)
            {
                param[i] = Serial_parm(15, &prms);
            }
            param[24] = Serial_parm(15, &prms) & 0x7F0F;
            for (i = 25; i < 31; i++)
            {
                param[i] = Serial_parm(15, &prms);
            }
            tmp = Serial_parm(8, &prms);
            param[31] = shl(tmp,7);
            shift=0;
        }
        
        /* check if the parameters matches the parameters of the corresponding decoder homing frame */
        tmp = i;
        j = 0;
        for (i = 0; i < tmp; i++)
        {
            j = (Word16) (param[i] ^ dhf[mode][i]);
            if (j)
                break;
        }
        tmp = 0x7fff;
        tmp = shr(tmp, shift);
        tmp = shl(tmp, shift);
        tmp = (Word16) (dhf[mode][i] & tmp);
        tmp = (Word16) (param[i] ^ tmp);
        j = (Word16) (j | tmp);
        
    }
    else
    {
        j = 1;
    }
    
    return (Word16) (!j);
}


Word16 decoder_homing_frame_test(Word16 input_frame[], Word16 mode)
{
    /* perform test for COMPLETE parameter frame */
    return dhf_test(input_frame, mode, nb_of_bits[mode]);
}


Word16 decoder_homing_frame_test_first(Word16 input_frame[], Word16 mode)
{
    /* perform test for FIRST SUBFRAME of parameter frame ONLY */
    return dhf_test(input_frame, mode, prmnofsf[mode]);
}
