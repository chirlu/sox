/*-------------------------------------------------------------------*
 *                         QPISF_2S.C                                *
 *-------------------------------------------------------------------*
 * Coding/Decoding of ISF parameters  with prediction.               *
 *                                                                   *
 * The ISF vector is quantized using two-stage VQ with split-by-2    *
 * in 1st stage and split-by-5 (or 3)in the second stage.            *
 *-------------------------------------------------------------------*/


#include "typedef.h"
#include "basic_op.h"
#include "cnst.h"
#include "acelp.h"
#include "count.h"

#include "qpisf_2s.tab"                    /* Codebooks of isfs */

#define MU         10923                   /* Prediction factor   (1.0/3.0) in Q15 */
#define N_SURV_MAX 4                       /* 4 survivors max */
#define ALPHA      29491                   /* 0. 9 in Q15     */
#define ONE_ALPHA (32768-ALPHA)            /* (1.0 - ALPHA) in Q15 */

/* local functions */

static void VQ_stage1(
     Word16 * x,                           /* input : ISF residual vector           */
     Word16 * dico,                        /* input : quantization codebook         */
     Word16 dim,                           /* input : dimention of vector           */
     Word16 dico_size,                     /* input : size of quantization codebook */
     Word16 * index,                       /* output: indices of survivors          */
     Word16 surv                           /* input : number of survivor            */
);

/*-------------------------------------------------------------------*
 * Function   Qpisf_2s_46B()                                         *
 *            ~~~~~~~~~                                              *
 * Quantization of isf parameters with prediction. (46 bits)         *
 *                                                                   *
 * The isf vector is quantized using two-stage VQ with split-by-2 in *
 *  1st stage and split-by-5 in the second stage.                    *
 *-------------------------------------------------------------------*/


void Qpisf_2s_46b(
     Word16 * isf1,                        /* (i) Q15 : ISF in the frequency domain (0..0.5) */
     Word16 * isf_q,                       /* (o) Q15 : quantized ISF               (0..0.5) */
     Word16 * past_isfq,                   /* (io)Q15 : past ISF quantizer                   */
     Word16 * indice,                      /* (o)     : quantization indices                 */
     Word16 nb_surv                        /* (i)     : number of survivor (1, 2, 3 or 4)    */
)
{
    Word16 i, k, tmp_ind[5];
    Word16 surv1[N_SURV_MAX];              /* indices of survivors from 1st stage */
    Word32 temp, min_err, distance;
    Word16 isf[ORDER];
    Word16 isf_stage2[ORDER];


    for (i = 0; i < ORDER; i++)
    {
        /* isf[i] = isf1[i] - mean_isf[i] - MU*past_isfq[i] */
        isf[i] = sub(isf1[i], mean_isf[i]);move16();
        isf[i] = sub(isf[i], mult(MU, past_isfq[i]));   move16();
    }

    VQ_stage1(&isf[0], dico1_isf, 9, SIZE_BK1, surv1, nb_surv);

    distance = MAX_32;                     move32();

    for (k = 0; k < nb_surv; k++)
    {
        for (i = 0; i < 9; i++)
        {
            isf_stage2[i] = sub(isf[i], dico1_isf[i + surv1[k] * 9]);   move16();
        }

        tmp_ind[0] = Sub_VQ(&isf_stage2[0], dico21_isf, 3, SIZE_BK21, &min_err);        move16();
        temp = min_err;                    move32();
        tmp_ind[1] = Sub_VQ(&isf_stage2[3], dico22_isf, 3, SIZE_BK22, &min_err);        move16();
        temp = L_add(temp, min_err);
        tmp_ind[2] = Sub_VQ(&isf_stage2[6], dico23_isf, 3, SIZE_BK23, &min_err);        move16();
        temp = L_add(temp, min_err);

        test();
        if (L_sub(temp, distance) < (Word32) 0)
        {
            distance = temp;               move32();
            indice[0] = surv1[k];          move16();
            for (i = 0; i < 3; i++)
            {
                indice[i + 2] = tmp_ind[i];move16();
            }
        }
    }


    VQ_stage1(&isf[9], dico2_isf, 7, SIZE_BK2, surv1, nb_surv);

    distance = MAX_32;                     move32();

    for (k = 0; k < nb_surv; k++)
    {
        for (i = 0; i < 7; i++)
        {
            isf_stage2[i] = sub(isf[9 + i], dico2_isf[i + surv1[k] * 7]);       move16();
        }

        tmp_ind[0] = Sub_VQ(&isf_stage2[0], dico24_isf, 3, SIZE_BK24, &min_err);        move16();
        temp = min_err;                    move32();
        tmp_ind[1] = Sub_VQ(&isf_stage2[3], dico25_isf, 4, SIZE_BK25, &min_err);        move16();
        temp = L_add(temp, min_err);

        test();
        if (L_sub(temp, distance) < (Word32) 0)
        {
            distance = temp;               move32();
            indice[1] = surv1[k];          move16();
            for (i = 0; i < 2; i++)
            {
                indice[i + 5] = tmp_ind[i];move16();
            }
        }
    }

    Dpisf_2s_46b(indice, isf_q, past_isfq, isf_q, isf_q, 0, 0);

    return;
}

/*-------------------------------------------------------------------*
 * Function   Qpisf_2s_36B()                                         *
 *            ~~~~~~~~~                                              *
 * Quantization of isf parameters with prediction. (36 bits)         *
 *                                                                   *
 * The isf vector is quantized using two-stage VQ with split-by-2 in *
 *  1st stage and split-by-3 in the second stage.                    *
 *-------------------------------------------------------------------*/


void Qpisf_2s_36b(
     Word16 * isf1,                        /* (i) Q15 : ISF in the frequency domain (0..0.5) */
     Word16 * isf_q,                       /* (o) Q15 : quantized ISF               (0..0.5) */
     Word16 * past_isfq,                   /* (io)Q15 : past ISF quantizer                   */
     Word16 * indice,                      /* (o)     : quantization indices                 */
     Word16 nb_surv                        /* (i)     : number of survivor (1, 2, 3 or 4)    */
)
{
    Word16 i, k, tmp_ind[5];
    Word16 surv1[N_SURV_MAX];              /* indices of survivors from 1st stage */
    Word32 temp, min_err, distance;
    Word16 isf[ORDER];
    Word16 isf_stage2[ORDER];

    for (i = 0; i < ORDER; i++)
    {
        /* isf[i] = isf1[i] - mean_isf[i] - MU*past_isfq[i] */
        isf[i] = sub(isf1[i], mean_isf[i]);move16();
        isf[i] = sub(isf[i], mult(MU, past_isfq[i]));   move16();
    }

    VQ_stage1(&isf[0], dico1_isf, 9, SIZE_BK1, surv1, nb_surv);

    distance = MAX_32;                     move32();

    for (k = 0; k < nb_surv; k++)
    {
        for (i = 0; i < 9; i++)
        {
            isf_stage2[i] = sub(isf[i], dico1_isf[i + surv1[k] * 9]);   move16();
        }

        tmp_ind[0] = Sub_VQ(&isf_stage2[0], dico21_isf_36b, 5, SIZE_BK21_36b, &min_err);        move16();
        temp = min_err;                    move32();
        tmp_ind[1] = Sub_VQ(&isf_stage2[5], dico22_isf_36b, 4, SIZE_BK22_36b, &min_err);        move16();
        temp = L_add(temp, min_err);

        test();
        if (L_sub(temp, distance) < (Word32) 0)
        {
            distance = temp;               move32();
            indice[0] = surv1[k];          move16();
            for (i = 0; i < 2; i++)
            {
                indice[i + 2] = tmp_ind[i];move16();
            }
        }
    }


    VQ_stage1(&isf[9], dico2_isf, 7, SIZE_BK2, surv1, nb_surv);

    distance = MAX_32;                     move32();

    for (k = 0; k < nb_surv; k++)
    {
        for (i = 0; i < 7; i++)
        {
            isf_stage2[i] = sub(isf[9 + i], dico2_isf[i + surv1[k] * 7]);       move16();
        }

        tmp_ind[0] = Sub_VQ(&isf_stage2[0], dico23_isf_36b, 7, SIZE_BK23_36b, &min_err);        move16();
        temp = min_err;                    move32();

        test();
        if (L_sub(temp, distance) < (Word32) 0)
        {
            distance = temp;               move32();
            indice[1] = surv1[k];          move16();
            indice[4] = tmp_ind[0];        move16();
        }
    }

    Dpisf_2s_36b(indice, isf_q, past_isfq, isf_q, isf_q, 0, 0);

    return;
}

/*-------------------------------------------------------------------*
 * routine:   Disf_2s_46b()                                          *
 *            ~~~~~~~~~                                              *
 * Decoding of ISF parameters                                        *
 *-------------------------------------------------------------------*/

void Dpisf_2s_46b(
     Word16 * indice,                      /* input:  quantization indices                       */
     Word16 * isf_q,                       /* output: quantized ISF in frequency domain (0..0.5) */
     Word16 * past_isfq,                   /* i/0   : past ISF quantizer                    */
     Word16 * isfold,                      /* input : past quantized ISF                    */
     Word16 * isf_buf,                     /* input : isf buffer                                                        */
     Word16 bfi,                           /* input : Bad frame indicator                   */
     Word16 enc_dec
)
{
    Word16 ref_isf[M];
    Word16 i, j, tmp;
    Word32 L_tmp;

    test();
    if (bfi == 0)                          /* Good frame */
    {
        for (i = 0; i < 9; i++)
        {
            isf_q[i] = dico1_isf[indice[0] * 9 + i];    move16();
        }
        for (i = 0; i < 7; i++)
        {
            isf_q[i + 9] = dico2_isf[indice[1] * 7 + i];        move16();
        }

        for (i = 0; i < 3; i++)
        {
            isf_q[i] = add(isf_q[i], dico21_isf[indice[2] * 3 + i]);    move16();
        }
        for (i = 0; i < 3; i++)
        {
            isf_q[i + 3] = add(isf_q[i + 3], dico22_isf[indice[3] * 3 + i]);    move16();
        }
        for (i = 0; i < 3; i++)
        {
            isf_q[i + 6] = add(isf_q[i + 6], dico23_isf[indice[4] * 3 + i]);    move16();
        }
        for (i = 0; i < 3; i++)
        {
            isf_q[i + 9] = add(isf_q[i + 9], dico24_isf[indice[5] * 3 + i]);    move16();
        }
        for (i = 0; i < 4; i++)
        {
            isf_q[i + 12] = add(isf_q[i + 12], dico25_isf[indice[6] * 4 + i]);  move16();
        }

        for (i = 0; i < ORDER; i++)
        {
            tmp = isf_q[i];                move16();
            isf_q[i] = add(tmp, mean_isf[i]);   move16();
            isf_q[i] = add(isf_q[i], mult(MU, past_isfq[i]));   move16();
            past_isfq[i] = tmp;            move16();
        }

        test();
        if (enc_dec)
        {
            for (i = 0; i < M; i++)
            {
                for (j = (L_MEANBUF - 1); j > 0; j--)
                {
                    isf_buf[j * M + i] = isf_buf[(j - 1) * M + i];      move16();
                }
                isf_buf[i] = isf_q[i];     move16();
            }
        }
    } else
    {                                      /* bad frame */
        for (i = 0; i < M; i++)
        {
            L_tmp = L_mult(mean_isf[i], 8192);
            for (j = 0; j < L_MEANBUF; j++)
            {
                L_tmp = L_mac(L_tmp, isf_buf[j * M + i], 8192);
            }
            ref_isf[i] = roundL(L_tmp);     move16();
        }

        /* use the past ISFs slightly shifted towards their mean */
        for (i = 0; i < ORDER; i++)
        {
            isf_q[i] = add(mult(ALPHA, isfold[i]), mult(ONE_ALPHA, ref_isf[i]));        move16();
        }

        /* estimate past quantized residual to be used in next frame */

        for (i = 0; i < ORDER; i++)
        {
            tmp = add(ref_isf[i], mult(past_isfq[i], MU));      /* predicted ISF */
            past_isfq[i] = sub(isf_q[i], tmp);  move16();
            past_isfq[i] = shr(past_isfq[i], 1);        move16();  /* past_isfq[i] *= 0.5 */
        }
    }

    Reorder_isf(isf_q, ISF_GAP, ORDER);

    return;
}

/*-------------------------------------------------------------------*
 * routine:   Disf_2s_36b()                                          *
 *            ~~~~~~~~~                                              *
 * Decoding of ISF parameters                                        *
 *-------------------------------------------------------------------*/

void Dpisf_2s_36b(
     Word16 * indice,                      /* input:  quantization indices                       */
     Word16 * isf_q,                       /* output: quantized ISF in frequency domain (0..0.5) */
     Word16 * past_isfq,                   /* i/0   : past ISF quantizer                    */
     Word16 * isfold,                      /* input : past quantized ISF                    */
     Word16 * isf_buf,                     /* input : isf buffer                                                        */
     Word16 bfi,                           /* input : Bad frame indicator                   */
     Word16 enc_dec
)
{
    Word16 ref_isf[M];
    Word16 i, j, tmp;
    Word32 L_tmp;

    test();
    if (bfi == 0)                          /* Good frame */
    {
        for (i = 0; i < 9; i++)
        {
            isf_q[i] = dico1_isf[indice[0] * 9 + i];    move16();
        }
        for (i = 0; i < 7; i++)
        {
            isf_q[i + 9] = dico2_isf[indice[1] * 7 + i];        move16();
        }

        for (i = 0; i < 5; i++)
        {
            isf_q[i] = add(isf_q[i], dico21_isf_36b[indice[2] * 5 + i]);        move16();
        }
        for (i = 0; i < 4; i++)
        {
            isf_q[i + 5] = add(isf_q[i + 5], dico22_isf_36b[indice[3] * 4 + i]);        move16();
        }
        for (i = 0; i < 7; i++)
        {
            isf_q[i + 9] = add(isf_q[i + 9], dico23_isf_36b[indice[4] * 7 + i]);        move16();
        }

        for (i = 0; i < ORDER; i++)
        {
            tmp = isf_q[i];
            isf_q[i] = add(tmp, mean_isf[i]);   move16();
            isf_q[i] = add(isf_q[i], mult(MU, past_isfq[i]));   move16();
            past_isfq[i] = tmp;            move16();
        }

        test();
        if (enc_dec)
        {
            for (i = 0; i < M; i++)
            {
                for (j = (L_MEANBUF - 1); j > 0; j--)
                {
                    isf_buf[j * M + i] = isf_buf[(j - 1) * M + i];      move16();
                }
                isf_buf[i] = isf_q[i];     move16();
            }
        }
    } else
    {                                      /* bad frame */
        for (i = 0; i < M; i++)
        {
            L_tmp = L_mult(mean_isf[i], 8192);
            for (j = 0; j < L_MEANBUF; j++)
            {
                L_tmp = L_mac(L_tmp, isf_buf[j * M + i], 8192);
            }

            ref_isf[i] = roundL(L_tmp);     move16();
        }

        /* use the past ISFs slightly shifted towards their mean */
        for (i = 0; i < ORDER; i++)
        {
            isf_q[i] = add(mult(ALPHA, isfold[i]), mult(ONE_ALPHA, ref_isf[i]));        move16();
        }

        /* estimate past quantized residual to be used in next frame */

        for (i = 0; i < ORDER; i++)
        {
            tmp = add(ref_isf[i], mult(past_isfq[i], MU));      /* predicted ISF */
            past_isfq[i] = sub(isf_q[i], tmp);  move16();
            past_isfq[i] = shr(past_isfq[i], 1);        move16();  /* past_isfq[i] *= 0.5 */
        }
    }

    Reorder_isf(isf_q, ISF_GAP, ORDER);

    return;
}


/*--------------------------------------------------------------------------*
 * procedure  Reorder_isf()                                                 *
 *            ~~~~~~~~~~~~~                                                 *
 * To make sure that the  isfs are properly order and to keep a certain     *
 * minimum distance between consecutive isfs.                               *
 *--------------------------------------------------------------------------*
 *    Argument         description                     in/out               *
 *    ~~~~~~~~         ~~~~~~~~~~~                     ~~~~~~               *
 *     isf[]           vector of isfs                    i/o                *
 *     min_dist        minimum required distance         i                  *
 *     n               LPC order                         i                  *
 *--------------------------------------------------------------------------*/

void Reorder_isf(
     Word16 * isf,                         /* (i/o) Q15: ISF in the frequency domain (0..0.5) */
     Word16 min_dist,                      /* (i) Q15  : minimum distance to keep             */
     Word16 n                              /* (i)      : number of ISF                        */
)
{
    Word16 i, isf_min;

    isf_min = min_dist;                    move16();

    for (i = 0; i < n - 1; i++)
    {
        test();
        if (sub(isf[i], isf_min) < 0)
        {
            isf[i] = isf_min;              move16();
        }
        isf_min = add(isf[i], min_dist);
    }

    return;
}


Word16 Sub_VQ(                             /* output: return quantization index     */
     Word16 * x,                           /* input : ISF residual vector           */
     Word16 * dico,                        /* input : quantization codebook         */
     Word16 dim,                           /* input : dimention of vector           */
     Word16 dico_size,                     /* input : size of quantization codebook */
     Word32 * distance                     /* output: error of quantization         */
)
{
    Word16 i, j, index, temp, *p_dico;
    Word32 dist_min, dist;

    dist_min = MAX_32;                     move32();
    p_dico = dico;                         move16();

    index = 0;                             move16();
    for (i = 0; i < dico_size; i++)
    {
        dist = 0;                          move32();
        for (j = 0; j < dim; j++)
        {
            temp = sub(x[j], *p_dico++);
            dist = L_mac(dist, temp, temp);
        }

        test();
        if (L_sub(dist, dist_min) < (Word32) 0)
        {
            dist_min = dist;               move32();
            index = i;                     move16();
        }
    }

    *distance = dist_min;                  move32();

    /* Reading the selected vector */

    p_dico = &dico[index * dim];           move16();
    for (j = 0; j < dim; j++)
    {
        x[j] = *p_dico++;                  move16();
    }

    return index;
}


static void VQ_stage1(
     Word16 * x,                           /* input : ISF residual vector           */
     Word16 * dico,                        /* input : quantization codebook         */
     Word16 dim,                           /* input : dimention of vector           */
     Word16 dico_size,                     /* input : size of quantization codebook */
     Word16 * index,                       /* output: indices of survivors          */
     Word16 surv                           /* input : number of survivor            */
)
{
    Word16 i, j, k, l, temp, *p_dico;
    Word32 dist_min[N_SURV_MAX], dist;

    for (i = 0; i < surv; i++)
    {
        dist_min[i] = MAX_32;              move32();
        index[i] = i;                      move16();
    }
    p_dico = dico;                         move16();

    for (i = 0; i < dico_size; i++)
    {
        dist = 0;                          move32();
        for (j = 0; j < dim; j++)
        {
            temp = sub(x[j], *p_dico++);
            dist = L_mac(dist, temp, temp);
        }

        for (k = 0; k < surv; k++)
        {
            test();
            if (L_sub(dist, dist_min[k]) < (Word32) 0)
            {
                for (l = sub(surv, 1); l > k; l--)
                {
                    dist_min[l] = dist_min[l - 1];      move32();
                    index[l] = index[l - 1];    move16();
                }
                dist_min[k] = dist;        move32();
                index[k] = i;              move16();
                break;
            }
        }
    }

    return;
}
