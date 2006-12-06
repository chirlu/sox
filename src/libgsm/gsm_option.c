/*
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

#include "private.h"

#include "gsm.h"

int gsm_option (gsm r, int opt, int * val)
{
	int 	result = -1;

	switch (opt) {
	case GSM_OPT_VERBOSE:
		result = r->verbose;
		if (val) r->verbose = *val;
		break;

	case GSM_OPT_FRAME_CHAIN:
		result = r->frame_chain;
		if (val) r->frame_chain = *val;
		break;

	case GSM_OPT_FRAME_INDEX:
		result = r->frame_index;
		if (val) r->frame_index = *val;
		break;

	case GSM_OPT_WAV49:
		result = r->wav_fmt;
		if (val) r->wav_fmt = !!*val;
		break;

	default:
		break;
	}
	return result;
}
