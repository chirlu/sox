/*
 * May 19, 1992
 * Copyright 1992 Guido van Rossum And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Guido van Rossum And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * A meta-handler that recognizes most file types by looking in the
 * first part of the file.  The file must be seekable!
 * (IRCAM sound files are not recognized -- these don't seem to be
 * used any more -- but this is just laziness on my part.) 
 */

#include "st.h"
#include <string.h>

int st_autostartread(ft)
ft_t ft;
{
	char *type;
	char header[132];
	if (!ft->seekable)
	{
		st_fail("Type AUTO input must be a file, not a pipe");
		return(ST_EOF);
	}
	if (fread(header, 1, sizeof(header), ft->fp) != sizeof(header))
	{
		st_fail("Type AUTO detects short file");
		return(ST_EOF);
	}
	fseek(ft->fp, 0L - sizeof header, 1); /* Seek back */
	type = 0;
	if ((strncmp(header, ".snd", 4) == 0) ||
	    (strncmp(header, "dns.", 4) == 0) ||
	    ((header[0] == '\0') && (strncmp(header+1, "ds.", 3) == 0))) {
		type = "au";
	}
	else if (strncmp(header, "FORM", 4) == 0) {
		if (strncmp(header + 8, "AIFF", 4) == 0)
			type = "aiff";
		else if (strncmp(header + 8, "8SVX", 4) == 0)
			type = "8svx";
		else if (strncmp(header + 8, "MAUD", 4) == 0)
			type = "maud";
	}
	else if (strncmp(header, "RIFF", 4) == 0 &&
		 strncmp(header + 8, "WAVE", 4) == 0) {
		type = "wav";
	}
	else if (strncmp(header, "Creative Voice File", 19) == 0) {
		type = "voc";
	}
	else if (strncmp(header+65, "FSSD", 4) == 0 &&
		 strncmp(header+128, "HCOM", 4) == 0) {
		type = "hcom";
	}
	else if (strncmp(header, "SOUND", 5) == 0) {
		type = "sndt";
	}
	else if (strncmp(header, "2BIT", 4) == 0) {
		type = "avr";
	}
	else if (header[0] == 0 && header[1] == 0) {
		int rate = (header[2] & 0xff) + ((header[3] & 0xff) << 8);
		if (rate >= 4000 && rate <= 25000)
			type = "sndr";
	}
  	if (type == 0) {
  		printf("Type AUTO doesn't recognize this header\n");
                printf("Trying: -t raw -r 11000 -b -u\n\n");
                type = "raw";
                ft->info.rate = 11000;
                ft->info.size = ST_SIZE_BYTE;
                ft->info.encoding = ST_ENCODING_UNSIGNED;
                }
	st_report("Type AUTO changed to %s", type);
	ft->filetype = type;
	st_gettype(ft); /* Change ft->h to the new format */
	(* ft->h->startread)(ft);
	return(ST_SUCCESS);
}

int st_autostartwrite(ft) 
ft_t ft;
{
	st_fail("Type AUTO can only be used for input!");
	return(ST_EOF);
}
