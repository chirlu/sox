/*
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

#ifndef	GSM_H
#define	GSM_H


/*
 *	Interface
 */

typedef struct gsm_state * 	gsm;
typedef short		   	gsm_signal;		/* signed 16 bit */
typedef unsigned char		gsm_byte;
typedef gsm_byte 		gsm_frame[33];		/* 33 * 8 bits	 */

#define	GSM_MAGIC		0xD		  	/* 13 kbit/s RPE-LTP */

#define	GSM_PATCHLEVEL		10
#define	GSM_MINOR		0
#define	GSM_MAJOR		1

#define	GSM_OPT_VERBOSE		1
#define	GSM_OPT_WAV49		4
#define	GSM_OPT_FRAME_INDEX	5
#define	GSM_OPT_FRAME_CHAIN	6

extern gsm  gsm_create(void);
extern void gsm_destroy(gsm);	

extern int  gsm_option(gsm, int, int *);

extern void gsm_encode(gsm, gsm_signal *, gsm_byte  *);
extern int  gsm_decode(gsm, gsm_byte   *, gsm_signal *);

#endif	/* GSM_H */
