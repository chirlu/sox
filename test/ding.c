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

struct _Env {
	struct _Env *next;
	int r;        /* rise   */
	int m;        /* middle */
	int f;        /* fall   */
	int e;        /* end    */
	FLOAT frq;    /* frequency  */
	FLOAT b;	    /* bend (not quite implemented) */
	FLOAT v;	    /* volume */
	FLOAT d;	    /* decay  */
	FLOAT x,y;	  /* current x,y */
	FLOAT phx,phy;/* per-sample phase multiplier cos(PI*frq),sin(PI*frq) */
};

const struct _Env EnvTemplate = {NULL,0,0,0,0,0.0,0.0,0.5,1.0,0.0,1.0,1.0,0.0};

struct _Env * Env0 = NULL; /* 1st  */
struct _Env * EnvL = NULL; /* last */

struct _Env *env_new(struct _Env *L)
{
	struct _Env *E;
	E = (struct _Env *) malloc(sizeof(struct _Env));
	if (E) {
		memcpy(E, (L)? L : &EnvTemplate, sizeof(struct _Env));
	}
	return E;
}
	
static void env_init(struct _Env *E)
{
	if (0<E->frq && E->frq<1) {
		E->x = 1; E->y = 0;
	} else {
		E->x = 0; E->y = 1;
	}
	E->phx = cos(E->frq*M_PI);
	E->phy = sin(E->frq*M_PI);
}
	
static void env_post(struct _Env *E, int k)
{
	double x1;
	x1   = E->x*E->phx - E->y*E->phy;
	E->y = E->x*E->phy + E->y*E->phx;
	E->x = x1;
	/* update (x,y) for next tic */
	if (!(k&0x0f)) {  /* norm correction each 16 samples */
		x1 = 1/sqrt(E->x*E->x+E->y*E->y);
		E->x *= x1;
		E->y *= x1;
	}
}

/* rise/fall function, monotonic [0,1] -> [1-0] */
static inline double ramp(double s)
{
	return 0.5*(1 + cos(M_PI * s));
}

static double env(struct _Env *Env, int k)
{
	double u = 0;
	//if (k >= Env->r && k < Env->e) {
		if (k<Env->m) {
			u = Env->v * ramp((double)(Env->m-k)/(Env->m-Env->r));
		}else if (k<Env->f) {
			u = Env->v;
			Env->v *= Env->d;
		}else{
			u = Env->v * ramp((double)(k-Env->f)/(Env->e-Env->f));
		}
		u *= Env->y;
		env_post(Env,k);
	//}
	return u;
}

static void Usage(void)__attribute__((noreturn));

static void Usage(void)
{
	fprintf(stderr, "Usage: ./ding [options] [<in-file>] <out-file>\n");
	fprintf(stderr, "  Options:\n");
	fprintf(stderr, "    -f <freq>      float, frequency = freq*nyquist_rate\n");
	fprintf(stderr, "    [-v <vol>]     float, volume, 1.00 is max\n");
	fprintf(stderr, "    [-d <decay>]   float, per-sample decay factor\n");
	fprintf(stderr, "    [-e start:attack:duration:mute]  ints \n");
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
	struct _Env *E;

	 /* Parse the options */
	E = NULL;
	len = 0;
	while ((optc = getopt(argct, argv, "d:e:f:v:h")) != -1) {
		char *p;
		switch(optc) {
			case 'd':
				if (!E) {
					fprintf(stderr,"option -f must precede -%c\n", optc);
					Usage();
				}
				E->d = strtod(optarg,&p);
				if (p==optarg || *p) {
					fprintf(stderr,"option -%c expects float value (%s)\n", optc, optarg);
					Usage();
				}
				break;
			case 'f':
				E = env_new(EnvL);
				if (EnvL) EnvL->next = E;
				EnvL = E;
				if (!Env0) Env0 = E;

				E->frq = strtod(optarg,&p);
				if (p==optarg || *p) {
					fprintf(stderr,"option -%c expects float value (%s)\n", optc, optarg);
					Usage();
				}
				break;
			case 'e':
				{
				int t[5], tmin=0;
				int i,ct=0;

				if (!E) {
					fprintf(stderr,"option -f must precede -%c\n", optc);
					Usage();
				}
				for (p=optarg,ct=0; ct<5; p++) {
					t[ct] = 0;
					if (*p && *p != ':') { 
						t[ct] = strtol(p,&p,10);
						if (t[ct]<tmin) tmin=t[ct];
					}
					ct++;
					if (*p != ':') break;
				}
				if (ct==4) t[ct++] = 0;

				if (*p || tmin<0 || ct!=5) {
					fprintf(stderr,"option -%c not valid (%s)\n", optc, optarg);
					Usage();
				}

				for (i=1; i<ct; i++)
					t[i] += t[i-1];

				E->r = t[0];
				E->m = t[1];
				E->f = t[2];
				E->e = t[3];
				if (len<t[4]) len=t[4];
				break;
				}
			case 'v':
				if (!E) {
					fprintf(stderr,"option -f must precede -%c\n", optc);
					Usage();
				}
				E->v = MAXSAMPL*strtod(optarg,&p);
				if (E->frq==0.0) E->v *= sqrt(0.5);
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

	//fprintf(stderr,"Vol0 %8.3f\n", Vol0);

	fnam1=NULL; fd1=-1;
	//fprintf(stderr,"optind=%d argct=%d\n",optind,argct);
	if (optind <= argct-2) {
		int ln1;
		fnam1=argv[optind++];
		fd1=open(fnam1,O_RDONLY);
		if (fd1<0) {
			fprintf(stderr,"Open: %s %s\n",fnam1,strerror(errno)); return(1);
		}
		ln1=lseek(fd1,0,SEEK_END)/2;
		lseek(fd1,0,SEEK_SET);
		if (len<ln1) len = ln1;
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

	for (E=Env0; (E); E=E->next) env_init(E);

	ibuff=(SAMPL*)malloc(BSIZ*sizeof(SAMPL));
	poflo=moflo=0;
	max =MINSAMPL; min=MAXSAMPL;
	for(st=0; st<len; ){
		int ct;
		SAMPL *ibp;

		ct = len - st; if (ct>BSIZ) ct=BSIZ;
		ReadN(fd1,ibuff,ct);
		for (ibp=ibuff; ibp<ibuff+ct; ibp++,st++) {
			double v = *ibp;

			if (max<*ibp) max=*ibp;
			else if (min>*ibp) min=*ibp;
			v *= Vol0;

			for (E=Env0; (E); E=E->next) {
				if (st>=E->r && st<E->e) {
					v += env(E, st);
				}
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
