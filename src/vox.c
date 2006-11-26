/************************************************************************
 *                                   SOX                                *
 *                                                                      *
 *                       AUDIO FILE PROCESSING UTILITY                  *
 *                                                                      *
 * Project : SOX                                                        *
 * File    : vox.c                                                      *
 * Version : V12.17.4                                                   *
 *                                                                      *
 * Version History : V12.17.4 - Tony Seebregts                          *
 *                              5 May 2004                              *
 *                              1. Original                             *
 *                                                                      *
 * Description : SOX file format handler for Dialogic/Oki ADPCM VOX     *
 *               files.                                                 *
 *                                                                      *
 * Notes : 1. Based on the vox/devox code samples at:                   *
 *                                                                      *
 *              http://www.cis.ksu.edu/~tim/vox                         *
 *                                                                      *
 *         2. Coded from SOX skeleton code supplied with SOX source.    *
 *                                                                      *
 *         3. Tested under:                                             *
 *            - Windows 2000 SP3/Visual C++ V6.0                        *
 *            - Windows 2000 SP3/Digital Mars V7.51                     *
 *                                                                      *
 ************************************************************************/

  ///////////////////////////////////////////
 // ORIGINAL SOX COPYRIGHT AND DISCLAIMER //
///////////////////////////////////////////

/************************************************************************
 * July 5, 1991                                                         *
 *                                                                      *
 * Copyright 1991 Lance Norskog And Sundry Contributors                 *
 *                                                                      *
 * This source code is freely redistributable and may be used for any   *
 * purpose.  This copyright notice must be maintained.                  *
 *                                                                      *
 * Lance Norskog And Sundry Contributors are not responsible for the    *
 * consequences of using this software.                                 *
 *                                                                      *
 ************************************************************************/

  ///////////////////
 // INCLUDE FILES //
///////////////////

#include "st_i.h"

  //////////////
 // TYPEDEFS //
//////////////

typedef struct voxstuff { struct { short    last;                       // ADPCM codec state
                                   short    index;
                                 } state; 

                          struct { uint8_t  byte;                       // write store
                                   uint8_t  flag;
                                 } store;
                        } *vox_t;


  ///////////////
 // CONSTANTS //
///////////////

static short STEPSIZE[49] = { 16,  17,  19,  21,  23,  25,  28, 
                              31,  34,  37,  41,  45,  50,  55, 
                              60,  66,  73,  80,  88,  97,  107, 
                              118, 130, 143, 157, 173, 190, 209, 
                              230, 253, 279, 307, 337, 371, 408, 
                              449, 494, 544, 598, 658, 724, 796, 
                              876, 963, 1060,1166,1282,1411,1552 
                            };

static short STEPADJUST[8] = { -1,-1,-1,-1,2,4,6,8 };
  

  /////////////////////////
 // FUNCTION PROTOTYPES //
/////////////////////////

static uint8_t envox       (short,  vox_t);
static short   devox       (uint8_t,vox_t);


  ////////////////////
 // IMPLEMENTATION //
////////////////////

/******************************************************************************
 * Function   : st_voxstartread 
 * Description: Initialises the file parameters and ADPCM codec state.
 * Parameters : ft  - file info structure
 * Returns    : int - ST_SUCCESS
 *                    ST_EOF
 * Exceptions :
 * Notes      : 1. VOX file format is 4-bit OKI ADPCM that decodes to 
 *                 to 12 bit signed linear PCM.
 *              2. Dialogic only supports 6kHz, 8kHz and 11 kHz sampling
 *                 rates but the codecs allows any user specified rate. 
 ******************************************************************************/

int  st_voxstartread (ft_t ft) 
     { vox_t state = (vox_t) ft->priv;


           // ... setup file info

       ft->file.buf = (char *)malloc(ST_BUFSIZ);
    
       if (!ft->file.buf)
          { st_fail_errno (ft,ST_ENOMEM,"Unable to allocate internal buffer memory");
            
            return(ST_EOF);
          }

       ft->file.size     = ST_BUFSIZ;
       ft->file.count    = 0;
       ft->file.pos      = 0;
       ft->file.eof      = 0;

           ft->info.size     = ST_SIZE_WORD;
       ft->info.encoding = ST_ENCODING_SIGN2;
       ft->info.channels = 1;

       // ... initialise CODEC state

       state->state.last  = 0;
       state->state.index = 0;
       state->store.byte  = 0;
       state->store.flag  = 0;

           return (ST_SUCCESS);
    }


/******************************************************************************
 * Function   : st_voxread 
 * Description: Fills an internal buffer from the VOX file, converts the 
 *              OKI ADPCM 4-bit samples to 12-bit signed PCM and then scales 
 *              the samples to full range 16 bit PCM.
 * Parameters : ft     - file info structure
 *              buffer - output buffer
 *              length - size of output buffer
 * Returns    : int    - number of samples returned in buffer
 * Exceptions :
 * Notes      : 
 ******************************************************************************/

st_ssize_t st_voxread (ft_t ft,st_sample_t *buffer,st_ssize_t length) 
           { vox_t    state = (vox_t) ft->priv;
                 int      count = 0;
             int      N;
             uint8_t  byte;
             short    word;

             // ... round length down to nearest even number

             N  = length/2;
             N *=2;

             // ... loop until buffer full or EOF

             while (count < N) 
                   { // ... refill buffer 

                     if (ft->file.pos >= ft->file.count)
                        { ft->file.count = st_readbuf (ft,ft->file.buf,1,ft->file.size);
                          ft->file.pos   = 0;

                          if (ft->file.count == 0)
                             break;
                        }

                     // ... decode two nibbles stored as a byte

                     byte      = ft->file.buf[ft->file.pos++];

                     word      = devox ((uint8_t) ((byte >> 4) & 0x0F),state);
                     *buffer++ = ST_SIGNED_WORD_TO_SAMPLE (word * 16);

                     word      = devox ((uint8_t) (byte & 0x0F),state);
                     *buffer++ = ST_SIGNED_WORD_TO_SAMPLE (word * 16);

                     count += 2;
                       }
               
             return count;
           }

/******************************************************************************
 * Function   : st_voxstopread 
 * Description: Frees the internal buffer allocated in st_voxstartread.
 * Parameters : ft   - file info structure
 * Returns    : int  - ST_SUCCESS
 * Exceptions :
 * Notes      : 
 ******************************************************************************/

int  st_voxstopread (ft_t ft) 
     { free (ft->file.buf);
     
       return (ST_SUCCESS);
     }


/******************************************************************************
 * Function   : st_voxstartwrite
 * Description: Initialises the file parameters and ADPCM codec state.
 * Parameters : ft  - file info structure
 * Returns    : int - ST_SUCCESS
 *                    ST_EOF
 * Exceptions :
 * Notes      : 1. VOX file format is 4-bit OKI ADPCM that decodes to 
 *                 to 12 bit signed linear PCM.
 *              2. Dialogic only supports 6kHz, 8kHz and 11 kHz sampling
 *                 rates but the codecs allows any user specified rate. 
 ******************************************************************************/

int  st_voxstartwrite (ft_t ft) 
     { vox_t state = (vox_t) ft->priv;


           // ... setup file info

       ft->file.buf = (char *)malloc(ST_BUFSIZ);
    
       if (!ft->file.buf)
          { st_fail_errno (ft,ST_ENOMEM,"Unable to allocate internal buffer memory");
            
            return(ST_EOF);
          }

       ft->file.size     = ST_BUFSIZ;
       ft->file.count    = 0;
       ft->file.pos      = 0;
       ft->file.eof      = 0;

           ft->info.size     = ST_SIZE_WORD;
       ft->info.encoding = ST_ENCODING_SIGN2;
       ft->info.channels = 1;

       // ... initialise CODEC state

       state->state.last  = 0;
       state->state.index = 0;
       state->store.byte  = 0;
       state->store.flag  = 0;

           return (ST_SUCCESS);
    }

/******************************************************************************
 * Function   : st_voxwrite
 * Description: Converts the supplied buffer to 12 bit linear PCM and encodes
 *              to OKI ADPCM 4-bit samples (packed a two nibbles per byte).
 * Parameters : ft     - file info structure
 *              buffer - output buffer
 *              length - size of output buffer
 * Returns    : int    - ST_SUCCESS
 *                       ST_EOF
 * Exceptions :
 * Notes      : 
 ******************************************************************************/

st_ssize_t st_voxwrite (ft_t ft,const st_sample_t *buffer,st_ssize_t length) 
           { vox_t    state = (vox_t) ft->priv;
             int      count = 0;
             uint8_t  byte  = state->store.byte;
             uint8_t  flag  = state->store.flag;
             short    word;

             while (count < length)
                   { word   = ST_SAMPLE_TO_SIGNED_WORD (*buffer++);
                     word  /= 16;

                     byte <<= 4;
                     byte  |= envox (word,state) & 0x0F;

                     flag++;
                     flag %= 2;

                     if (flag == 0)
                        { ft->file.buf[ft->file.count++] = byte;

                          if (ft->file.count >= ft->file.size)
                             { st_writebuf (ft,ft->file.buf,1,ft->file.count);

                               ft->file.count = 0;
                             }
                        }

                     count++;
                   }

             // ... keep last byte across calls

             state->store.byte = byte;
             state->store.flag = flag;
        
             return (count);
           }

/******************************************************************************
 * Function   : st_voxstopwrite
 * Description: Flushes any leftover samples and frees the internal buffer 
 *              allocated in st_voxstartwrite.
 * Parameters : ft   - file info structure
 * Returns    : int  - ST_SUCCESS
 * Exceptions :
 * Notes      : 
 ******************************************************************************/

int  st_voxstopwrite (ft_t ft) 
     { vox_t    state = (vox_t) ft->priv;
       uint8_t  byte  = state->store.byte;
       uint8_t  flag  = state->store.flag;

       // ... flush remaining samples

       if (flag != 0)
          { byte <<= 4;
            byte  |= envox (0,state) & 0x0F;

            ft->file.buf[ft->file.count++] = byte;
          }

       if (ft->file.count > 0)
          st_writebuf (ft,ft->file.buf,1,ft->file.count);

       // ... free buffer

       free (ft->file.buf);
     
       return (ST_SUCCESS);
     }

/******************************************************************************
 * Function   : envox
 * Description: Internal utility routine to encode 12 bit signed PCM to 
 *              OKI ADPCM code
 * Parameters : sample  - 12 bit linear PCM sample
 *              state   - CODEC state
 * Returns    : uint8_t - ADPCM nibble (in low order nibble)
 * Exceptions :
 * Notes      : 
 ******************************************************************************/

static uint8_t envox (short sample,vox_t state)
        { uint8_t code;
          short   dn;
          short   ss;

          ss   = STEPSIZE[state->state.index];
          code = 0x00;

          if ((dn = sample - state->state.last) < 0)
             { code = 0x08;
               dn   = -dn;
             }
          
          if (dn >= ss) 
             { code = code | 0x04;
               dn  -= ss;
             }

          if (dn >= ss/2)
             { code = code | 0x02;
               dn  -= ss/2;
             }

          if (dn >= ss/4)
             { code = code | 0x01;
             }

          // ... use decoder to set the estimate of last sample and adjust the step index
    
          state->state.last = devox (code,state);
    
          return (code);
        }


/******************************************************************************
 * Function   : devox
 * Description: Internal utility routine to decode OKI ADPCM 4-bit samples to 
 *              12-bit signed PCM.
 * Parameters : code   - ADPCM code (nibble)
 *              state  - CODEC state
 * Returns    : short  - 12 bit signed PCM sample
 * Exceptions :
 * Notes      : 
 ******************************************************************************/

static short devox (uint8_t code,vox_t state) 
      { short dn;
        short ss;
        short sample;

        ss = STEPSIZE[state->state.index];
        dn = ss/8;

        if (code & 0x01)
           dn += ss/4;
    
        if (code & 0x02)
           dn += ss/2;
    
        if (code & 0x04)
           dn += ss;

        if (code & 0x08)
           dn = -dn;

        sample = state->state.last + dn;

        // ... clip to 12 bits

        if (sample > 2047)
           sample = 2047;

        if (sample < -2048)
           sample = -2048;

        // ... adjust step size

        state->state.last   = sample;
        state->state.index += STEPADJUST[code & 0x07];
    
        if (state->state.index < 0) 
           state->state.index = 0;

        if (state->state.index > 48) 
           state->state.index = 48;

        // ... done

        return (sample);
      }

static const char *voxnames[] = {
  "vox",
  NULL
};

static st_format_t st_vox_format = {
  voxnames,
  NULL,
  0,
  st_voxstartread,
  st_voxread,
  st_voxstopread,
  st_voxstartwrite,
  st_voxwrite,
  st_voxstopwrite,
  st_format_nothing_seek
};

const st_format_t *st_vox_format_fn(void)
{
    return &st_vox_format;
}
