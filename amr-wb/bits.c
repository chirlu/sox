/*------------------------------------------------------------------------*
 *                         BITS.C                                         *
 *------------------------------------------------------------------------*
 * Performs bit stream manipulation                                       *
 *------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include "typedef.h"
#include "basic_op.h"
#include "cnst.h"
#include "bits.h"
#include "acelp.h"
#include "count.h"
#include "dtx.h"

#include "mime_io.tab"

/*-----------------------------------------------------*
 * Write_serial -> write serial stream into a file     *
 *-----------------------------------------------------*/

Word16 Init_write_serial(TX_State ** st)
{
   TX_State *s;

   /* allocate memory */
    test();
    if ((s = (TX_State *) malloc(sizeof(TX_State))) == NULL)
    {
        fprintf(stderr, "write_serial_init: can not malloc state structure\n");
        return -1;
    }
    Reset_write_serial(s);
    *st = s;

    return 0;
}

Word16 Close_write_serial(TX_State *st)
{
   /* allocate memory */
    test();
    if (st != NULL)
    {
        free(st);
        st = NULL;
        return 0;
    }
    return 1;
}

void Reset_write_serial(TX_State * st)
{
    st->sid_update_counter = 3;
    st->sid_handover_debt = 0;
    st->prev_ft = TX_SPEECH;
}

void Write_serial(FILE * fp, Word16 prms[], Word16 coding_mode, Word16 mode, TX_State *st, Word16 bitstreamformat)
{
   Word16 i, frame_type;
   Word16 stream[SIZE_MAX];
   UWord8 temp;
   UWord8 *stream_ptr;

   if (coding_mode == MRDTX)
   {       
       st->sid_update_counter--;
       
       if (st->prev_ft == TX_SPEECH)
       {
           frame_type = TX_SID_FIRST;
           st->sid_update_counter = 3;
       } else
       {
           if ((st->sid_handover_debt > 0) &&
               (st->sid_update_counter > 2))
           {
               /* ensure extra updates are  properly delayed after a possible SID_FIRST */
               frame_type = TX_SID_UPDATE;
               st->sid_handover_debt--;
           } else
           {
               if (st->sid_update_counter == 0)
               {
                   frame_type = TX_SID_UPDATE;
                   st->sid_update_counter = 8;
               } else
               {
                   frame_type = TX_NO_DATA;
               }
           }
       }
   } else
   {
       st->sid_update_counter = 8;
       frame_type = TX_SPEECH;
   }
   st->prev_ft = frame_type;
   
      
   if(bitstreamformat == 0)             /* default file format */
   {
       stream[0] = TX_FRAME_TYPE;
       stream[1] = frame_type;
       stream[2] = mode;
       for (i = 0; i < nb_of_bits[coding_mode]; i++)
       {
           stream[3 + i] = prms[i];
       }
       
       fwrite(stream, sizeof(Word16), 3 + nb_of_bits[coding_mode], fp);

   } else
   {
       if (bitstreamformat == 1)        /* ITU file format */
       {
           stream[0] = 0x6b21;                          
          
           if(frame_type != TX_NO_DATA && frame_type != TX_SID_FIRST)
           {
               stream[1]=nb_of_bits[coding_mode];               
               for (i = 0; i < nb_of_bits[coding_mode]; i++)
               {
                   if(prms[i] == BIT_0){
                       stream[2 + i] = BIT_0_ITU;           
                   }
                   else{
                       stream[2 + i] = BIT_1_ITU;
                   }
               }
               fwrite(stream, sizeof(Word16), 2 + nb_of_bits[coding_mode], fp);    
           } else
           {
               stream[1] = 0;
               fwrite(stream, sizeof(Word16), 2, fp);      
           }
       } else                           /* MIME/storage file format */
       {
#define MRSID 9
           /* change mode index in case of SID frame */
           if (coding_mode == MRDTX)
           {
               coding_mode = MRSID;

               if (frame_type == TX_SID_FIRST)
               {
                   for (i = 0; i < NBBITS_SID; i++) prms[i] = BIT_0;
               }
           }

           /* we cannot handle unspecified frame types (modes 10 - 13) */
           /* -> force NO_DATA frame */
           if (coding_mode < 0 || coding_mode > 15 || (coding_mode > MRSID && coding_mode < 14))
           {
               coding_mode = 15;
           }

           /* mark empty frames between SID updates as NO_DATA frames */
           if (coding_mode == MRSID && frame_type == TX_NO_DATA)
           {
               coding_mode = 15;
           }

           /* set pointer for packed frame, note that we handle data as bytes */
           stream_ptr = (UWord8*)stream;

           /* insert table of contents (ToC) byte at the beginning of the packet */
           *stream_ptr = toc_byte[coding_mode];
           stream_ptr++;

           temp = 0;

           /* sort and pack AMR-WB speech or SID bits */
           for (i = 1; i < unpacked_size[coding_mode] + 1; i++)
           {
               if (prms[sort_ptr[coding_mode][i-1]] == BIT_1)
               {
                   temp++;
               }

               if (i % 8)
               {
                   temp <<= 1;
               }
               else
               {
                   *stream_ptr = temp;
                   stream_ptr++;
                   temp = 0;
               }
           }
           
           /* insert SID type indication and speech mode in case of SID frame */
           if (coding_mode == MRSID)
           {
               if (frame_type == TX_SID_UPDATE)
               {
                   temp++;
               }
               temp <<= 4;
               
               temp += mode & 0x000F;
           }

           /* insert unused bits (zeros) at the tail of the last byte */
           if (unused_size[coding_mode])
           {
               temp <<= (unused_size[coding_mode] - 1);
           }
           *stream_ptr = temp;

           /* write packed frame into file (1 byte added to cover ToC entry) */
           fwrite(stream, sizeof(UWord8), 1 + packed_size[coding_mode], fp);
       }
   }
   return;
}


/*-----------------------------------------------------*
 * Read_serial -> read serial stream into a file       *
 *-----------------------------------------------------*/

Word16 Init_read_serial(RX_State ** st)
{
   RX_State *s;

   /* allocate memory */
    test();
    if ((s = (RX_State *) malloc(sizeof(RX_State))) == NULL)
    {
        fprintf(stderr, "read_serial_init: can not malloc state structure\n");
        return -1;
    }
    Reset_read_serial(s);
    *st = s;

    return 0;
}

Word16 Close_read_serial(RX_State *st)
{
   /* allocate memory */
    test();
    if (st != NULL)
    {
        free(st);
        st = NULL;
        return 0;
    }
    return 1;
}

void Reset_read_serial(RX_State * st)
{
    st->prev_ft = RX_SPEECH_GOOD;
    st->prev_mode = 0;
}


Word16 Read_serial(FILE * fp, Word16 prms[], Word16 * frame_type, Word16 * mode, RX_State *st, Word16 bitstreamformat)
{
   Word16 n, n1, type_of_frame_type, coding_mode, datalen, i;
   UWord8 toc, q, temp, *packet_ptr, packet[64];

   if(bitstreamformat == 0)             /* default file format */
   {
       n = (Word16) fread(&type_of_frame_type, sizeof(Word16), 1, fp);
       n = (Word16) (n + fread(frame_type, sizeof(Word16), 1, fp));
       n = (Word16) (n + fread(mode, sizeof(Word16), 1, fp));
       coding_mode = *mode;
       if(*mode < 0 || *mode > NUM_OF_MODES-1)
       {
           fprintf(stderr, "Invalid mode received: %d (check file format).\n", *mode);
           exit(-1);
       }
       if (n == 3)
       {
           if (type_of_frame_type == TX_FRAME_TYPE)
           {
               switch (*frame_type)
               {
               case TX_SPEECH:
                   *frame_type = RX_SPEECH_GOOD;
                   break;
               case TX_SID_FIRST:
                   *frame_type = RX_SID_FIRST;
                   break;
               case TX_SID_UPDATE:
                   *frame_type = RX_SID_UPDATE;
                   break;
               case TX_NO_DATA:
                   *frame_type = RX_NO_DATA;
                   break;
               }
           } else if (type_of_frame_type != RX_FRAME_TYPE)
           {
               fprintf(stderr, "Wrong type of frame type:%d.\n", type_of_frame_type);
           }
           
           if ((*frame_type == RX_SID_FIRST) | (*frame_type == RX_SID_UPDATE) | (*frame_type == RX_NO_DATA) | (*frame_type == RX_SID_BAD))
           {
               coding_mode = MRDTX;
           }
           n = (Word16) fread(prms, sizeof(Word16), nb_of_bits[coding_mode], fp);
           if (n != nb_of_bits[coding_mode])
               n = 0;
       }
       return (n);
   } else
   {
       if (bitstreamformat == 1)        /* ITU file format */
       {
            n = (Word16) fread(&type_of_frame_type, sizeof(Word16), 1, fp);
            n = (Word16)(n+fread(&datalen, sizeof(Word16), 1, fp));

            if(n == 2)
            {
              if(type_of_frame_type == 0x6b20)        /* bad frame */
              {
                  *frame_type = RX_SPEECH_LOST;
                  *mode = st->prev_mode;
              }
              else if(type_of_frame_type == 0x6b21)   /* good frame */
              {
                  if(datalen == 0)                      /* RX_NO_DATA frame type */
                  {
                      if(st->prev_ft == RX_SPEECH_GOOD)
                      {
                          *frame_type = RX_SID_FIRST;
                      }
                      else 
                      {
                          *frame_type = RX_NO_DATA;
                      }
                      *mode = st->prev_mode;
                  }
                  else
                  {
                      coding_mode = -1;
                      for(i=NUM_OF_MODES-1; i>=0; i--)
                      {
                          if(datalen == nb_of_bits[i])
                          {
                              coding_mode = i;
                          }
                      }

                      if(coding_mode == -1)
                      {
                          fprintf(stderr, "\n\n ERROR: Invalid number of data bits received [%d]\n\n", datalen);
                          exit(-1);
                      }

                      if(coding_mode == NUM_OF_MODES-1)     /* DTX frame type */
                      {
                          *frame_type = RX_SID_UPDATE;
                          *mode = st->prev_mode;
                      }
                      else
                      {
                          *frame_type = RX_SPEECH_GOOD;
                          *mode = coding_mode;
                      }
                  }
                  st->prev_mode = *mode;
                  st->prev_ft = *frame_type;
              }
              else {
                  fprintf(stderr, "\n\n ERROR: Invalid ITU file format \n\n");
                  exit(-1);
              }
      }
            n1 = fread(prms, sizeof(Word16), datalen, fp);
            n += n1;
            for(i=0; i<n1; i++)
            {
                if(prms[i] <= BIT_0_ITU) prms[i] = BIT_0;
                else                        prms[i] = BIT_1;
            }          
            return(n);

       } else                           /* MIME/storage file format */
       {
           /* read ToC byte, return immediately if no more data available */
           if (fread(&toc, sizeof(UWord8), 1, fp) == 0)
           {
               return 0;
           }

           /* extract q and mode from ToC */
           q  = (toc >> 2) & 0x01;
           *mode = (toc >> 3) & 0x0F;

           /* read speech bits, return with empty frame if mismatch between mode info and available data */
           if ((Word16)fread(packet, sizeof(UWord8), packed_size[*mode], fp) != packed_size[*mode])
           {
               return 0;
           }

           packet_ptr = (UWord8*)packet;
           temp = *packet_ptr;
           packet_ptr++;
           
           /* unpack and unsort speech or SID bits */
           for (i = 1; i < unpacked_size[*mode] + 1; i++)
           {
               if (temp & 0x80) prms[sort_ptr[*mode][i-1]] = BIT_1;
               else             prms[sort_ptr[*mode][i-1]] = BIT_0;
               
               if (i % 8)
               {
                   temp <<= 1;
               }
               else
               {
                   temp = *packet_ptr;
                   packet_ptr++;
               }
           }

           /* set frame type */
           switch (*mode)
           {
           case MODE_7k:
           case MODE_9k:
           case MODE_12k:
           case MODE_14k:
           case MODE_16k:
           case MODE_18k:
           case MODE_20k:
           case MODE_23k:
           case MODE_24k:
               if (q)   *frame_type = RX_SPEECH_GOOD;
               else     *frame_type = RX_SPEECH_BAD;
               break;
           case MRSID:
               if (q)
               {
                   if (temp & 0x80) *frame_type = RX_SID_UPDATE;
                   else             *frame_type = RX_SID_FIRST;
               }
               else
               {
                   *frame_type = RX_SID_BAD;
               }

               /* read speech mode indication */
               coding_mode = (temp >> 3) & 0x0F;

               /* set mode index */
               *mode = st->prev_mode;
               break;
           case 14:     /* SPEECH_LOST */
               *frame_type = RX_SPEECH_LOST;
               *mode = st->prev_mode;
               break;
           case 15:     /* NO_DATA */
               *frame_type = RX_NO_DATA;
               *mode = st->prev_mode;
               break;
           default:     /* replace frame with unused mode index by NO_DATA frame */
               *frame_type = RX_NO_DATA;
               *mode = st->prev_mode;
               break;
           }

           st->prev_mode = *mode;

           /* return 1 to indicate succesfully parsed frame */
           return 1;
       }
#undef MRSID
   }

}


/*-----------------------------------------------------*
 * Parm_serial -> convert parameters to serial stream  *
 *-----------------------------------------------------*/

void Parm_serial(
     Word16 value,                         /* input : parameter value */
     Word16 no_of_bits,                    /* input : number of bits  */
     Word16 ** prms
)
{
    Word16 i, bit;

    *prms += no_of_bits;                   move16();

    for (i = 0; i < no_of_bits; i++)
    {
        bit = (Word16) (value & 0x0001);   logic16();  /* get lsb */
        test();move16();
        if (bit == 0)
            *--(*prms) = BIT_0;
        else
            *--(*prms) = BIT_1;
        value = shr(value, 1);             move16();
    }
    *prms += no_of_bits;                   move16();
    return;
}


/*----------------------------------------------------*
 * Serial_parm -> convert serial stream to parameters *
 *----------------------------------------------------*/

Word16 Serial_parm(                        /* Return the parameter    */
     Word16 no_of_bits,                    /* input : number of bits  */
     Word16 ** prms
)
{
    Word16 value, i;
    Word16 bit;

    value = 0;                             move16();
    for (i = 0; i < no_of_bits; i++)
    {
        value = shl(value, 1);
        bit = *((*prms)++);                move16();
        test();move16();
        if (bit == BIT_1)
            value = add(value, 1);
    }
    return (value);
}
