/* wav.h - various structures and defines used by WAV converter. */

#ifndef WAV_H_INCLUDED
#define WAV_H_INCLUDED

/* purloined from public Microsoft RIFF docs */

#define	WAVE_FORMAT_UNKNOWN		(0x0000)
#define	WAVE_FORMAT_PCM			(0x0001) 
#define	WAVE_FORMAT_ADPCM		(0x0002)
#define	WAVE_FORMAT_ALAW		(0x0006)
#define	WAVE_FORMAT_MULAW		(0x0007)
#define	WAVE_FORMAT_OKI_ADPCM		(0x0010)
#define WAVE_FORMAT_IMA_ADPCM		(0x0011)
#define	WAVE_FORMAT_DIGISTD		(0x0015)
#define	WAVE_FORMAT_DIGIFIX		(0x0016)
#define	IBM_FORMAT_MULAW         	(0x0101)
#define	IBM_FORMAT_ALAW			(0x0102)
#define	IBM_FORMAT_ADPCM         	(0x0103)

typedef struct MsState {
    LONG  index;	/* Index into step size table */
    ULONG bpred;	/* Most recent sample value */
    LONG  sample1;
    LONG  sample2;
} MsState_t;

typedef struct ImaState {
   int index;    	/* Index into step size table */
   int previousValue; 	/* Most recent sample value */
} ImaState_t;


#endif /* WAV_H_INCLUDED */
