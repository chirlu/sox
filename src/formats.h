/* libSoX static formats list   (c) 2006-8 Chris Bagwell and SoX contributors
 *
 * This library is free software; you can redistribute it and/or modify it
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

/* FIXME: generate this list automatically */

  FORMAT(aifc)
  FORMAT(aiff)
  FORMAT(al)
  FORMAT(au)
  FORMAT(avr)
  FORMAT(cdr)
  FORMAT(cvsd)
  FORMAT(cvu)
  FORMAT(dat)
  FORMAT(dvms)
  FORMAT(f4)
  FORMAT(f8)
  FORMAT(gsm)
  FORMAT(hcom)
  FORMAT(htk)
  FORMAT(ima)
  FORMAT(la)
  FORMAT(lpc10)
  FORMAT(lu)
  FORMAT(maud)
  FORMAT(nul)
  FORMAT(prc)
  FORMAT(raw)
  FORMAT(s1)
  FORMAT(s2)
  FORMAT(s3)
  FORMAT(s4)
  FORMAT(sf)
  FORMAT(smp)
  FORMAT(sounder)
  FORMAT(soundtool)
  FORMAT(sox)
  FORMAT(sphere)
  FORMAT(svx)
  FORMAT(txw)
  FORMAT(u1)
  FORMAT(u2)
  FORMAT(u3)
  FORMAT(u4)
  FORMAT(ul)
  FORMAT(voc)
  FORMAT(vox)
  FORMAT(wav)
  FORMAT(wve)
  FORMAT(xa)

#if defined HAVE_ALSA
  FORMAT(alsa)
#endif
#if defined HAVE_AMRNB
  FORMAT(amr_nb)
#endif
#if defined HAVE_AMRWB
  FORMAT(amr_wb)
#endif
#if defined HAVE_LIBAO
  FORMAT(ao)
#endif
#if defined HAVE_FFMPEG
  FORMAT(ffmpeg)
#endif
#if defined HAVE_FLAC
  FORMAT(flac)
#endif
#if defined(HAVE_MP3)
  FORMAT(mp3)
#endif
#if defined(HAVE_SYS_SOUNDCARD_H) || defined(HAVE_MACHINE_SOUNDCARD_H)
  FORMAT(oss)
#endif
#if defined HAVE_SNDFILE
  FORMAT(sndfile)
  #if defined HAVE_SNDFILE_1_0_12
  FORMAT(caf)
  #endif
  FORMAT(fap)
  FORMAT(mat4)
  FORMAT(mat5)
  FORMAT(paf)
  FORMAT(pvf)
  FORMAT(sd2)
  FORMAT(w64)
  FORMAT(xi)
#endif
#if defined(HAVE_SYS_AUDIOIO_H) || defined(HAVE_SUN_AUDIOIO_H)
  FORMAT(sunau)
#endif
#if defined(HAVE_COREAUDIO) || defined(HAVE_COREAUDIO)
  FORMAT(coreaudio)
#endif
#if defined HAVE_OGG_VORBIS
  FORMAT(vorbis)
#endif
#if defined HAVE_WAVPACK
  FORMAT(wavpack)
#endif
