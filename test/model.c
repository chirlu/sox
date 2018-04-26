/*
	model.c -- program to help test DSP filter and rate-conversion code.
	 
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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define FLOAT double

#ifndef LSAMPL
#	define SAMPL short
#	define MAXSAMPL 0x7fff
#	define MINSAMPL -0x8000
#else
#	define SAMPL long
#	define MAXSAMPL 0x7fffff
#	define MINSAMPL -0x800000
#endif

static struct _Env {
	int r;    /* rise   */
	int c;    /* center */
	int f;    /* fall   */
	double v; /* volume */
	double d; /* decay */
} Env = {0,0,0,0.5,1.0};

static void Usage(void)__attribute__((noreturn));

static void Usage(void)
{
	fprintf(stderr,"Usage: ./model rate0,rate1 [-l len] [-f freq] <in-file>\n"); exit(-1);
}

static int ReadN(int fd, SAMPL *v, int n)
{
	int r;

	do {
		r = read(fd, (char*)(v), n*sizeof(SAMPL));
	}while(r==-1 && errno==EINTR);
	if (r==-1 || r%sizeof(SAMPL)) {
		perror("Error reading input"); exit(-1);
	}
	return r/sizeof(SAMPL);
}

#define BSIZ 400000/*0x10000*/

static double bestfit(double sx, double sy,
								      double h11, double h12, double h22, 
											double *cx, double *cy)
{
	double a,su,uu,cu,w2;
	
	a=0; *cy=0;
	if (h22>1e-20*h11) {
		a = h12/h22;  /* u[] = x[] - a*y[] is orthogonal to y[] */
  	*cy = sy/h22; /* *cy*y[] is projection of s[] onto y[]  */
	}
	/* <s,x-ay> = sx -a*sy */
	su = sx - a*sy; /* su is iprod of s[] and u[] */
	/* <u,u> = <x-ay,x-ay> = h11 - 2a*h11 + a*a*h22 */
	uu = h11 - 2*a*h12 + a*a*h22;
	cu = 0;
	if (uu>1e-20*h11)
		cu = su/uu;  /* s[] = *cy * y[] + cu * u[] is orthogonal decomposition */
	w2  =  *cy * *cy * h22;
	w2 +=   cu *  cu *  uu;
	*cx = cu;
	*cy -= a*cu;
	return w2;
}

static double eCenter(const SAMPL *ibuff, int len)
{
	double v0,v1,v2;
	int n;

	v0 = v1 = v2 = 0;
	for (n=0; n<len; n++) {
		double w = ibuff[n];
		w = w*w;
		v0 += w;
		v1 += n*w;
		v2 += (n*n)*w;
	}
	v1 /= v0;
	v2 /= v0;
	v2 -= v1*v1;
	fprintf(stderr,"eCenter %.2f,  STD %.2f\n",v1,sqrt(v2));
	return v1;
}

static void
bigcalc(double Factor, double Freq1, const SAMPL *ibuff, int len)
{
	double c,del;
	double x,y;
	double thx,thy;
	double Len1;
	int k1,n;
	long double h11,h12,h22;
	long double sx,sy,ss;
	double cn,cx,cy;
	double s2=0, v2=0;
	const SAMPL *ip;

	inline void a_init(double frq)
	{
		x = 1;
		y = 0;
		thx = cos(frq*M_PI);
		thy = sin(frq*M_PI);
	}
	
	inline double a(double k, double L)
	{
		double a, l, u;
		l = L/2.0;
		u = k/l - 1; /* in (-1,+1) */
		a = 0.5*(1 + cos(M_PI * u));
		return a;
	}

	inline void a_post(int k)
	{
		double x1;
		x1 = x*thx - y*thy;
		y  = x*thy + y*thx;
		x = x1;
		/* update (x,y) for next tic */
		if ((k&0x0f)==0x0f) {  /* norm correction each 16 samples */
			x1 = 1/sqrt(x*x+y*y);
			x *= x1;
			y *= x1;
		}
	}

	c = eCenter(ibuff,len);
	Len1 = Env.c*Factor*0.6; /* 60% of original const-amplitude area */
	c += Env.c*Factor*0.15;  /* beginning after 30% */
	{
		double a;
		int b;
		a = c-Len1/2;
		b = ceil(a-0.001);
		ip = ibuff + b;
		del = b-a;
	}
	fprintf(stderr,"del %.3f\n", del);
	k1 = Len1-del;

	a_init(Freq1);
	h11 = h12 = h22 = 0;
	sx = sy = ss = 0;
	for(n=0; n<=k1; n++) {
		double f,s;
		double u,v;

		s = ip[n];    /* sigval at n      */
		
		f = 1; //a(n+del,Len1);
		u = f*x; v = f*y;
		sx += s*u;
		sy += s*v;
		ss += s*s;
		h11 += u*u;
		h12 += u*v;
		h22 += v*v;

		a_post(n);
	}
	//fprintf(stderr,"h12 %.10f\n", (double)h12/(sqrt(h11)*sqrt(h22)));
	
	v2 = ss/(k1+1);
	s2 = bestfit(sx,sy,h11,h12,h22,&cx,&cy)/(k1+1);
	cn = sqrt(cx*cx+cy*cy);
	fprintf(stderr,"amp %.1f, cx %.10f, cy %.10f\n", cn, cx/cn, cy/cn);

	fprintf(stderr,"[%.1f,%.1f]  s2max %.2f, v2max %.2f, rmserr %.2f\n",
				  c/Factor, c, sqrt(s2), sqrt(v2), (s2<=v2)? sqrt(v2-s2):-sqrt(s2-v2));

}

int main(int argct, char **argv)
{
	int optc;
	int fd1;
	int len;
	SAMPL *ibuff;
	int rate0, rate1;
	double Freq0=0, Freq1;   /* of nyquist */
	double Factor;
	char *fnam1;

	 /* Parse the options */
	while ((optc = getopt(argct, argv, "d:e:f:h")) != -1) {
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
				Freq0 = strtod(optarg,&p);
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
			case 'h':
			default:
				Usage();
		}
	}

	//fprintf(stderr,"optind=%d argct=%d\n",optind,argct);

	if (optind != argct-2) Usage();

	{
		char *p0, *p;
		p0 = argv[optind];
		rate0 = rate1 = strtol(p0,&p,10);
		if (*p) {
			if (*p != ':') {
				fprintf(stderr,"Invalid rate parameter (%s)\n", p0);
				Usage();
			}
			p0 = p+1;
			rate1 = strtol(p0,&p,10);
			if (*p) {
				fprintf(stderr,"Invalid 2nd rate parameter (%s)\n", p0);
				Usage();
			}
		}
		if (rate0<=0 || rate1<=0) {
			fprintf(stderr,"Invalid rate parameter (%s)\n", argv[optind]);
			Usage();
		}
		optind++;
	}

	Factor = (double)rate1 / (double)rate0; 
	Freq1 = Freq0/Factor;

	//if (optind != argct-1) Usage();

	fnam1=NULL; fd1=-1;
	fnam1=argv[optind++];
	fd1=open(fnam1,O_RDONLY);
	if (fd1<0) {
		fprintf(stderr,"Open: %s %s\n",fnam1,strerror(errno)); return(1);
	}

	//fprintf(stderr, "Files: %s %s\n",fnam1,fnam2);

	ibuff=(SAMPL*)malloc(BSIZ*sizeof(SAMPL));
	{
		int n,r;

		for (n=0; n<BSIZ; n+=r) {
			r = ReadN(fd1,ibuff+n,BSIZ-n);
			if (r==0) break;
		}
		len = n;
	}
	fprintf(stderr,"Read %d input samples\n",len);

	bigcalc(Factor, Freq1, ibuff, len);

	return 0;

}
