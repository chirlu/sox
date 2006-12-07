/*
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

#include <stdio.h>
#include <assert.h>

#include "private.h"

#include "gsm.h"

/*
 *  4.2.11 .. 4.2.12 LONG TERM PREDICTOR (LTP) SECTION
 */


/*
 * This module computes the LTP gain (bc) and the LTP lag (Nc)
 * for the long term analysis filter.   This is done by calculating a
 * maximum of the cross-correlation function between the current
 * sub-segment short term residual signal d[0..39] (output of
 * the short term analysis filter; for simplification the index
 * of this array begins at 0 and ends at 39 for each sub-segment of the
 * RPE-LTP analysis) and the previous reconstructed short term
 * residual signal dp[ -120 .. -1 ].  A dynamic scaling must be
 * performed to avoid overflow.
 */

static void Calculation_of_the_LTP_parameters (
	register word	* d,		/* [0..39]	IN	*/
	register word	* dp,		/* [-120..-1]	IN	*/
	word		* bc_out,	/* 		OUT	*/
	word		* Nc_out	/* 		OUT	*/
)
{
	register int  	k, lambda;
	word		Nc, bc;
	word		wt[40];

	longword	L_max, L_power;
	word		R, S, dmax, scal;
	register word	temp;

	/*  Search of the optimum scaling of d[0..39].
	 */
	dmax = 0;

	for (k = 0; k <= 39; k++) {
		temp = d[k];
		temp = GSM_ABS( temp );
		if (temp > dmax) dmax = temp;
	}

	temp = 0;
	if (dmax == 0) scal = 0;
	else {
		assert(dmax > 0);
		temp = gsm_norm( (longword)dmax << 16 );
	}

	if (temp > 6) scal = 0;
	else scal = 6 - temp;

	assert(scal >= 0);

	/*  Initialization of a working array wt
	 */

	for (k = 0; k <= 39; k++) wt[k] = SASR( d[k], scal );

	/* Search for the maximum cross-correlation and coding of the LTP lag
	 */
	L_max = 0;
	Nc    = 40;	/* index for the maximum cross-correlation */

	for (lambda = 40; lambda <= 120; lambda++) {

# undef STEP
#		define STEP(k) 	(longword)wt[k] * dp[k - lambda]

		register longword L_result;

		L_result  = STEP(0)  ; L_result += STEP(1) ;
		L_result += STEP(2)  ; L_result += STEP(3) ;
		L_result += STEP(4)  ; L_result += STEP(5)  ;
		L_result += STEP(6)  ; L_result += STEP(7)  ;
		L_result += STEP(8)  ; L_result += STEP(9)  ;
		L_result += STEP(10) ; L_result += STEP(11) ;
		L_result += STEP(12) ; L_result += STEP(13) ;
		L_result += STEP(14) ; L_result += STEP(15) ;
		L_result += STEP(16) ; L_result += STEP(17) ;
		L_result += STEP(18) ; L_result += STEP(19) ;
		L_result += STEP(20) ; L_result += STEP(21) ;
		L_result += STEP(22) ; L_result += STEP(23) ;
		L_result += STEP(24) ; L_result += STEP(25) ;
		L_result += STEP(26) ; L_result += STEP(27) ;
		L_result += STEP(28) ; L_result += STEP(29) ;
		L_result += STEP(30) ; L_result += STEP(31) ;
		L_result += STEP(32) ; L_result += STEP(33) ;
		L_result += STEP(34) ; L_result += STEP(35) ;
		L_result += STEP(36) ; L_result += STEP(37) ;
		L_result += STEP(38) ; L_result += STEP(39) ;

		if (L_result > L_max) {

			Nc    = lambda;
			L_max = L_result;
		}
	}

	*Nc_out = Nc;

	L_max <<= 1;

	/*  Rescaling of L_max
	 */
	assert(scal <= 100 && scal >=  -100);
	L_max = L_max >> (6 - scal);	/* sub(6, scal) */

	assert( Nc <= 120 && Nc >= 40);

	/*   Compute the power of the reconstructed short term residual
	 *   signal dp[..]
	 */
	L_power = 0;
	for (k = 0; k <= 39; k++) {

		register longword L_temp;

		L_temp   = SASR( dp[k - Nc], 3 );
		L_power += L_temp * L_temp;
	}
	L_power <<= 1;	/* from L_MULT */

	/*  Normalization of L_max and L_power
	 */

	if (L_max <= 0)  {
		*bc_out = 0;
		return;
	}
	if (L_max >= L_power) {
		*bc_out = 3;
		return;
	}

	temp = gsm_norm( L_power );

	R = SASR( L_max   << temp, 16 );
	S = SASR( L_power << temp, 16 );

	/*  Coding of the LTP gain
	 */

	/*  Table 4.3a must be used to obtain the level DLB[i] for the
	 *  quantization of the LTP gain b to get the coded version bc.
	 */
	for (bc = 0; bc <= 2; bc++) if (R <= gsm_mult(S, gsm_DLB[bc])) break;
	*bc_out = bc;
}


/* 4.2.12 */

static void Long_term_analysis_filtering (
	word		bc,	/* 					IN  */
	word		Nc,	/* 					IN  */
	register word	* dp,	/* previous d	[-120..-1]		IN  */
	register word	* d,	/* d		[0..39]			IN  */
	register word	* dpp,	/* estimate	[0..39]			OUT */
	register word	* e	/* long term res. signal [0..39]	OUT */
)
/*
 *  In this part, we have to decode the bc parameter to compute
 *  the samples of the estimate dpp[0..39].  The decoding of bc needs the
 *  use of table 4.3b.  The long term residual signal e[0..39]
 *  is then calculated to be fed to the RPE encoding section.
 */
{
	register int      k;
	register longword ltmp;

#	undef STEP
#	define STEP(BP)					\
	for (k = 0; k <= 39; k++) {			\
		dpp[k]  = GSM_MULT_R( BP, dp[k - Nc]);	\
		e[k]	= GSM_SUB( d[k], dpp[k] );	\
	}

	switch (bc) {
	case 0:	STEP(  3277 ); break;
	case 1:	STEP( 11469 ); break;
	case 2: STEP( 21299 ); break;
	case 3: STEP( 32767 ); break; 
	}
}

void Gsm_Long_Term_Predictor ( 	/* 4x for 160 samples */

	struct gsm_state	* S,

	word	* d,	/* [0..39]   residual signal	IN	*/
	word	* dp,	/* [-120..-1] d'		IN	*/

	word	* e,	/* [0..39] 			OUT	*/
	word	* dpp,	/* [0..39] 			OUT	*/
	word	* Nc,	/* correlation lag		OUT	*/
	word	* bc	/* gain factor			OUT	*/
)
{
	assert( d  ); assert( dp ); assert( e  );
	assert( dpp); assert( Nc ); assert( bc );

        Calculation_of_the_LTP_parameters(d, dp, bc, Nc);

	Long_term_analysis_filtering( *bc, *Nc, dp, d, dpp, e );
}

/* 4.3.2 */
void Gsm_Long_Term_Synthesis_Filtering (
	struct gsm_state	* S,

	word			Ncr,
	word			bcr,
	register word		* erp,	   /* [0..39]		  	 IN */
	register word		* drp	   /* [-120..-1] IN, [-120..40] OUT */
)
/*
 *  This procedure uses the bcr and Ncr parameter to realize the
 *  long term synthesis filtering.  The decoding of bcr needs
 *  table 4.3b.
 */
{
	register longword	ltmp;	/* for ADD */
	register int 		k;
	word			brp, drpp, Nr;

	/*  Check the limits of Nr.
	 */
	Nr = Ncr < 40 || Ncr > 120 ? S->nrp : Ncr;
	S->nrp = Nr;
	assert(Nr >= 40 && Nr <= 120);

	/*  Decoding of the LTP gain bcr
	 */
	brp = gsm_QLB[ bcr ];

	/*  Computation of the reconstructed short term residual 
	 *  signal drp[0..39]
	 */
	assert(brp != MIN_WORD);

	for (k = 0; k <= 39; k++) {
		drpp   = GSM_MULT_R( brp, drp[ k - Nr ] );
		drp[k] = GSM_ADD( erp[k], drpp );
	}

	/*
	 *  Update of the reconstructed short term residual signal
	 *  drp[ -1..-120 ]
	 */

	for (k = 0; k <= 119; k++) drp[ -120 + k ] = drp[ -80 + k ];
}
