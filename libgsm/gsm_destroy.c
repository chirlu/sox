/*
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

/* $Header: /cvsroot/sox/sox/libgsm/gsm_destroy.c,v 1.1 2007/09/06 16:50:55 cbagwell Exp $ */

#include "gsm.h"

#	include	<stdlib.h>

void gsm_destroy (gsm S)
{
	if (S) free((char *)S);
}
