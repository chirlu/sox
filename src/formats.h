/* FIXME: generate this list automatically */

  FORMAT(aifc)
  FORMAT(aiff)
  FORMAT(al)
  FORMAT(au)
  FORMAT(auto)
  FORMAT(avr)
  FORMAT(cdr)
  FORMAT(cvsd)
  FORMAT(dat)
  FORMAT(dvms)
  FORMAT(gsm)
  FORMAT(hcom)
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
  FORMAT(sndrtool)
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
#if defined HAVE_LIBAMRNB
  FORMAT(amr_nb)
#endif
#if defined HAVE_LIBAMRWB
  FORMAT(amr_wb)
#endif
#if defined HAVE_LIBAO
  FORMAT(ao)
#endif
#if defined HAVE_LIBAVPLUGIN
  FORMAT(ffmpeg)
#endif
#if defined HAVE_LIBFLAC
  FORMAT(flac)
#endif
#if defined(HAVE_LIBMAD) || defined(HAVE_LIBMP3LAME)
  FORMAT(mp3)
#endif
#if defined(HAVE_SYS_SOUNDCARD_H) || defined(HAVE_MACHINE_SOUNDCARD_H)
  FORMAT(oss)
#endif
#if defined HAVE_SNDFILE_H
  FORMAT(sndfile)
#endif
#if defined(HAVE_SYS_AUDIOIO_H) || defined(HAVE_SUN_AUDIOIO_H)
  FORMAT(sun)
#endif
#if defined HAVE_LIBVORBIS
  FORMAT(vorbis)
#endif
