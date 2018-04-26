/*
  corr.c
    print correlation coeff's for 2 input files of 'short' values

	Copyright (C) 1999 Stanley J. Brooks <stabro@megsinet.net> 

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 
*/

#include <string.h>
// for open,read,write:
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
// for clock/time:
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define BLEN 0x20000
#define HLEN 0x1000

static u_int32_t dist[0x10000];
#define dis0 (dist+0x8000)

static short buff1[BLEN];
static short buff2[BLEN];
static short hist[HLEN];

inline static double square(double x) {return (x*x);}

static void readin(int fd, short *p, int bs)
{
	int r;
 	do {
		r=read(fd,(char*)p,2*bs);
	}while (r==-1 && errno==EINTR);
	if (r==-1 || r!=2*bs) {
		perror("Input error");
		exit(1);
	}
}

int main(int argct, char **argv)
{
	int fd1,fd2;
	int avgn=0,assay=0,code=0;
	int hx=0,hsum=0;
	int len,len1,len2,x,r;
	char *fnam1,*fnam2;
	double v1=0,v2=0,s1=0;

	/*
	 * Parse the command line arguments
	 */
	while (*++argv && **argv == '-')
		while (*(++*argv))
			switch (**argv) {
			case 'a':
				assay=1;
				break;
			case 'c':
				code=1;
				break;
			case 'h':
				fprintf(stderr,"./stat [-n avgn] <file>\n");
				break;
			case 'n':
				if (!*++*argv) { // end of this param string
					if (!argv[1]) break;
					++argv;
				}
				avgn=atoi(*argv);
				*argv += strlen(*argv)-1;	// skip to end-1 of this param string
				break;
			}

	if (avgn<=0) avgn=1; else if (avgn>HLEN) avgn=HLEN;
	bzero(hist,sizeof(hist));

	fnam1=*argv++;
	fd1=open(fnam1,O_RDWR);
	if (fd1<0) {
		fprintf(stderr,"Open: %s %s\n",fnam1,strerror(errno)); return(1);
	}
	len1=lseek(fd1,0,SEEK_END);
	r=lseek(fd1,0,SEEK_SET);

	fnam2=*argv++;
	fd2=open(fnam2,O_RDWR);
	if (fd2<0) {
		fprintf(stderr,"Open: %s %s\n",fnam2,strerror(errno)); return(1);
	}
	len2=lseek(fd2,0,SEEK_END);
	r=lseek(fd2,0,SEEK_SET);

	bzero(dist,sizeof(dist));
	len = (len1<len2)? len1:len2;
	len /= 2; /* now len is number of shorts data to read */
	for (x=0; x<len; ){
		int bs;
		int j;
		double c11=0, c12=0, c22=0;
		
		bs=len-x; if (bs>BLEN) bs=BLEN; /* number of shorts to read */

		readin(fd1,buff1,bs);
		readin(fd2,buff2,bs);
		
		for (j=0; j<bs; j++) {
			c11 += buff1[j]*buff1[j];
			c12 += buff1[j]*buff2[j];
			c22 += buff2[j]*buff2[j];
		}
		c11 /= bs;
		c12 /= bs;
		c22 /= bs;

		{
			double d11=(c11+2*c12+c22)/4;
			double d22=(c11-2*c12+c22)/4;
			double d12=(c11-c22)/4;

			printf("%8.1f%8.1f cf=%f",sqrt(c11),sqrt(c22),c12/sqrt(c11*c22));
			printf(" | %8.1f%8.1f cf=%f\n",sqrt(d11),sqrt(d22),d12/sqrt(d11*d22));
		}

		for (j=0; j<bs; j++) {
			int y;
			//y=(abs(buff1[j])<abs(buff2[j]))? buff1[j]:buff2[j];
			y=(buff1[j]+buff2[j]+1)/2;
			hsum -= hist[hx];
			hsum += y;
			hist[hx] = y;
			if (++hx == avgn) hx=0;
			s1 += abs(y);
			v1 += hsum;
			v2 += square(hsum);
			y = (hsum+avgn/2)/avgn;
			dis0[y]++;
		}
		
		x += bs;
	}
	v1 /= len;
	v2 /= len;
	printf("%8d %5d=n %10.2f=avg %10.2f=rms\n",len,avgn,v1/avgn,sqrt(v2-v1*v1)/avgn);

	if (code) {
		int32_t j,tot,cut;
		cut = len/2;
		for (tot=0,j=0; j<0x8000; j++) {
			int32_t tot1;
			tot1 = tot+dis0[j];
			if (j!=0) tot1 += dis0[-j];
			if (tot1>cut && tot<=cut) {
				printf(" |%d| %d of %d\n",j-1,tot,cut);
				cut += (len-cut)/2;
			}
			tot=tot1;
		}
	}

	if (assay)
		for (x=-0x8000; x<=0x7fff; x++) {
			printf("%6d %6d\n",x,dis0[x]);
		}
	
	return 0;
}

