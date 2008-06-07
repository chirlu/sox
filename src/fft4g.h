/* This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

void cdft(int, int, double *, int *, double *);
void rdft(int, int, double *, int *, double *);
void ddct(int, int, double *, int *, double *);
void ddst(int, int, double *, int *, double *);
void dfct(int, double *, double *, int *, double *);
void dfst(int, double *, double *, int *, double *);

void cdft_f(int, int, float *, int *, float *);
void rdft_f(int, int, float *, int *, float *);
void ddct_f(int, int, float *, int *, float *);
void ddst_f(int, int, float *, int *, float *);
void dfct_f(int, float *, float *, int *, float *);
void dfst_f(int, float *, float *, int *, float *);

#define dft_br_len(l) (2 + (1 << (int)(log(l / 2 + .5) / log(2.)) / 2))
#define dft_sc_len(l) (l / 2)
