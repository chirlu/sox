/*
	ding.c -- program to generate testpatterns for DSP code.
	 
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
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 
*/

#include <string.h>
// for open,read,write:
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define FLOAT double

#ifndef LSAMPL
#	define SAMPL short
#	define MAXSAMPL 0x7fff
#	define MINSAMPL -0x7fff
#else
#	define SAMPL long
#	define MAXSAMPL 0x7fffffff
#	define MINSAMPL -0x7fffffff
#endif

static void Usage(void)__attribute__((noreturn));

static void Usage(void)
{
	fprintf(stderr, "Usage: ./ding [options] [<in-file>] <out-file>\n");
	fprintf(stderr, "  Options:\n");
	fprintf(stderr, "    [-v <vol>]     float, volume, 1.00 is max\n");
	fprintf(stderr, "    [-f <freq>]    float, frequency = freq*nyquist_rate\n");
	fprintf(stderr, "    [-d <decay>]   float, per-sample decay factor\n");
	fprintf(stderr, "    [-o <n>]       int, zero-pad samples before tone\n");
	fprintf(stderr, "    [-e <n>]       int, length in samples of tone\n");
	fprintf(stderr, "    [-p <n>]       int, zero-pad samples after tone\n");
	exit(-1);
}

static int ReadN(int fd, SAMPL *v, int n)
{
	int r,m;
	static SAMPL frag=0;
	static int fraglen=0;
	char *q;

	q=(char*)v;
	if (fraglen)
		memcpy(q, (char*)&frag, fraglen);

	m = n*sizeof(SAMPL) - fraglen;
	q += fraglen;
	if (fd>=0) {
		do {
			r = read(fd, q, m);
		}while(r==-1 && errno==EINTR);
		if (r==-1) {
			perror("Error reading fd1"); exit(-1);
		}
	}else{
		bzero(q,m);
		r = m;
	}
	r += fraglen;
	fraglen = r%sizeof(SAMPL);
	if (fraglen)
		memcpy((char*)&frag, (char*)v, fraglen);
					
	return (r/sizeof(SAMPL));
}

#define BSIZ 0x10000

int main(int argct, char **argv)
{
	int optc;
	int fd1,fd2;
	char *fnam1,*fnam2;
	int len, st;
	SAMPL *ibuff,max,min;
	int poflo,moflo;
	FLOAT Vol0=1;
	FLOAT Freq=0;   /* of nyquist */
	int Offset=0;
	int Pad=0;
	struct _Env {
		int r;    /* rise   */
		int c;    /* center */
		int f;    /* fall   */
		FLOAT v; /* volume */
		FLOAT d; /* decay */
	} Env = {0,0,0,0.5,1.0};

	double x=1,y=0;
	double thx=1,thy=0;
				
	static inline void a_init(double frq)
	{
		if (frq != 0) {
			x = 1; y = 0;
		} else {
			x = 0; y = 1;
		}
		thx = cos(frq*M_PI);
		thy = sin(frq*M_PI);
	}
	
	static inline const double a(int k, int L)
	{
		double a, u;
		u = (double)k/L; /* in [0,1] */
		a = 0.5*(1 + cos(M_PI * u));
		//printf("a(%d,%d) = %.5f,  l=%.1f, u=%.6f\n",k,L,a,l,u); 
		return a;
	}

	static inline void a_post(int k)
	{
		double x1;
		x1 = x*thx - y*thy;
		y  = x*thy + y*thx;
		x = x1;
		/* update (x,y) for next tic */
		if (!(k&0x0f)) {  /* norm correction each 16 samples */
			x1 = 1/sqrt(x*x+y*y);
			x *= x1;
			y *= x1;
		}
	}

	 /* Parse the options */
	while ((optc = getopt(argct, argv, "d:o:e:t:p:f:v:h")) != -1) {
		char *p;
		switch(optc) {
			case 'd':
				Env.d = strtod(optarg,&p);
				if (p==optarg || *p) {
					fprintf(stderr,"option -%c expects float value (%s)\n", optc, optarg);
					Usage();
				}
				break;
			case 'f':
				Freq = strtod(optarg,&p);
				if (p==optarg || *p) {
					fprintf(stderr,"option -%c expects float value (%s)\n", optc, optarg);
					Usage();
				}
				break;
			case 'e':
				p = optarg;
				Env.c  = strtol(p,&p,10);
				if (*p++ == ':') {
					Env.r = Env.f = Env.c;
					Env.c = strtol(p,&p,10);
				}
				if (*p++ == ':') {
					Env.f = strtol(p,&p,10);
				}
				if (*p || Env.c<=0 || Env.r<0 || Env.f<0) {
					fprintf(stderr,"option -%c not valid (%s)\n", optc, optarg);
					Usage();
				}
				break;
			case 'o':
				Offset = strtol(optarg,&p,10);
				if (p==optarg || *p || Offset<0) {
					fprintf(stderr,"option -%c expects int value (%s)\n", optc, optarg);
					Usage();
				}
				break;
			case 'p':
				Pad = strtol(optarg,&p,10);
				if (p==optarg || *p || Pad<0) {
					fprintf(stderr,"option -%c expects int value (%s)\n", optc, optarg);
					Usage();
				}
				break;
			case 'v':
				Env.v = strtod(optarg,&p);
				if (p==optarg || *p) {
					fprintf(stderr,"option -%c expects float value (%s)\n", optc, optarg);
					Usage();
				}
				break;
			case 'h':
			default:
				Usage();
		}
	}

	Env.v *= MAXSAMPL;
	if (Freq==0.0) Env.v *= sqrt(0.5);
	//fprintf(stderr,"Vol0 %8.3f\n", Vol0);

	len = Offset+Env.r+Env.c+Env.f+Pad;
	fnam1=NULL; fd1=-1;
	//fprintf(stderr,"optind=%d argct=%d\n",optind,argct);
	if (optind <= argct-2) {
		fnam1=argv[optind++];
		fd1=open(fnam1,O_RDONLY);
		if (fd1<0) {
			fprintf(stderr,"Open: %s %s\n",fnam1,strerror(errno)); return(1);
		}
		len=lseek(fd1,0,SEEK_END)/2;
		lseek(fd1,0,SEEK_SET);
	}

	if (optind != argct-1) Usage();

	fd2 = 1; /* stdout */
	fnam2=argv[optind++];
	if (strcmp(fnam2,"-")) {
		fd2=open(fnam2,O_WRONLY|O_CREAT|O_TRUNC,0644);
		if (fd2<0) {
			fprintf(stderr,"Open: %s %s\n",fnam2,strerror(errno)); return(1);
		}
	}
	//fprintf(stderr, "Files: %s %s\n",fnam1,fnam2);

	a_init(Freq);

	ibuff=(SAMPL*)malloc(BSIZ*sizeof(SAMPL));
	poflo=moflo=0;
	max =MINSAMPL; min=MAXSAMPL;
	for(st=0; st<len; ){
		int ct;
		SAMPL *ibp;

		ct = len - st; if (ct>BSIZ) ct=BSIZ;
		ReadN(fd1,ibuff,ct);
		for (ibp=ibuff; ibp<ibuff+ct; ibp++,st++) {
			int k;
			double v = *ibp;

			if (max<*ibp) max=*ibp;
			else if (min>*ibp) min=*ibp;
			v *= Vol0;

			k = st-Offset;
			if (k>=0 && k<Env.r) {
				v += y*a(Env.r-k,Env.r)*Env.v;
				a_post(st);
			}else if (k-=Env.r, k>=0 && k<Env.c) {
				v += y*Env.v;
				a_post(st);
			}else if (k-=Env.c, k>=0 && k<Env.f) {
				v += y*a(k,Env.f)*Env.v;
				a_post(st);
			}

			if (v>MAXSAMPL) {
				poflo++;
				v=MAXSAMPL;
			} else if (v<MINSAMPL) {
				moflo++;
				v=MINSAMPL;
			}
			*ibp = rint(v);
		}

		write(fd2,(char*)ibuff,ct*sizeof(SAMPL));
	}

	fprintf(stderr,"input range: [%ld,%ld]  pos/neg oflos: %d/%d\n",min,max,poflo,moflo);
	return 0;

}
