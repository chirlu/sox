include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckLibraryExists)
include(TestBigEndian)

macro(optional variable header library function source)
  check_include_files(${header} ${variable}1)
  if (${variable}1)
    check_library_exists(${library} ${function} "" ${variable})
    if (${variable})
      set(optional_srcs ${optional_srcs} ${source})
      set(optional_libs ${optional_libs} ${library})
    endif (${variable})
  endif (${variable}1)
endmacro(optional)

macro(optional3 variable header library1 function1 library2 function2 library3 function3 source)
  check_include_files(${header} ${variable}1)
  if (${variable}1)
    check_library_exists(${library1} ${function1} "" ${variable}2)
    if (${variable}2)
      check_library_exists(${library2} ${function2} "" ${variable}3)
      if (${variable}3)
	check_library_exists(${library3} ${function3} "" ${variable})
        if (${variable})
          set(optional_srcs ${optional_srcs} ${source})
          set(optional_libs ${optional_libs} ${library1} ${library2} ${library3})
        endif (${variable})
      endif (${variable}3)
    endif (${variable}2)
  endif (${variable}1)
endmacro(optional3)

check_include_files("byteswap.h"         HAVE_BYTESWAP_H)
check_include_files("inttypes.h"         HAVE_INTTYPES_H)
check_include_files("io.h"               HAVE_IO_H)
#check_include_files("ltdl.h"             HAVE_LTDL_H) # no plug-ins as yet
check_include_files("stdint.h"           HAVE_STDINT_H)
check_include_files("string.h"           HAVE_STRING_H)
check_include_files("sys/time.h"         HAVE_SYS_TIME_H)
check_include_files("sys/timeb.h"        HAVE_SYS_TIMEB_H)
check_include_files("unistd.h"           HAVE_UNISTD_H)

check_function_exists("fseeko"           HAVE_FSEEKO)
check_function_exists("getopt_long"      HAVE_GETOPT_LONG)
check_function_exists("gettimeofday"     HAVE_GETTIMEOFDAY)
check_function_exists("popen"            HAVE_POPEN)
check_function_exists("strcasecmp"       HAVE_STRCASECMP)
check_function_exists("strdup"           HAVE_STRDUP)
check_function_exists("strrstr"          HAVE_STRRSTR)
check_function_exists("vsnprintf"        HAVE_VSNPRINTF)

test_big_endian(WORDS_BIGENDIAN)

optional(NEED_LIBM math.h m pow "")
optional(EXTERNAL_GSM gsm/gsm.h gsm gsm_create "")
optional(HAVE_ALSA alsa/asoundlib.h asound snd_pcm_open alsa)
optional(HAVE_LIBAMRNB amrnb/sp_dec.h amrnb Decoder_Interface_init amr-nb)
optional(HAVE_LIBAMRWB amrwb/dec.h amrwb D_IF_init amr-wb)
optional(HAVE_LIBAO ao/ao.h ao ao_play ao)
optional(HAVE_LIBFLAC FLAC/all.h FLAC FLAC__stream_encoder_new flac)
optional(HAVE_LIBMAD mad.h mad mad_stream_buffer mp3)
optional(HAVE_LIBMP3LAME lame/lame.h mp3lame lame_init mp3)
optional(HAVE_SAMPLERATE_H samplerate.h samplerate src_simple rabbit)
optional(HAVE_SNDFILE_H sndfile.h sndfile sf_open sndfile)
optional(HAVE_SNDFILE_1_0_12 sndfile.h sndfile sf_open_virtual "")
optional(HAVE_SUN_AUDIOIO_H sun/audioio.h c ioctl sunaudio)
if (NOT HAVE_SUN_AUDIOIO_H)
  optional(HAVE_SYS_AUDIOIO_H sys/audioio.h c ioctl sunaudio)
endif (NOT HAVE_SUN_AUDIOIO_H)
optional(HAVE_SYS_SOUNDCARD_H sys/soundcard.h c ioctl oss)
if (NOT HAVE_SYS_SOUNDCARD_H)
  optional(HAVE_MACHINE_SOUNDCARD_H machine/soundcard.h ossaudio _oss_ioctl oss)
endif (NOT HAVE_SYS_SOUNDCARD_H)
optional3(HAVE_LIBVORBIS vorbis/codec.h vorbis vorbis_analysis_headerout vorbisfile ov_clear vorbisenc vorbis_encode_init_vbr vorbis)
optional3(HAVE_LIBAVPLUGIN ffmpeg/avformat.h avformat av_open_input_file avutil av_rescale_q avcodec avcodec_encode_audio ffmpeg)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}config.h)

#include_directories(${liblpc10_BINARY_DIR}/lpc10)
#link_directories(${liblpc10_SOURCE_DIR}/lpc10)

if(CMAKE_COMPILER_IS_GNUCC)
#  add_definitions(-Wconversion)
endif(CMAKE_COMPILER_IS_GNUCC)

if (NOT EXTERNAL_GSM)
  add_subdirectory(libgsm)
  set(optional_libs ${optional_libs} gsm)
endif (NOT EXTERNAL_GSM)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/soxstdint.h
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/soxstdint.h.cmake ${CMAKE_CURRENT_SOURCE_DIR}/soxstdint.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/soxstdint.h.cmake
  )

set(effects_srcs
  biquad
  biquads
  chorus
  compand
  compandt
  dcshift
  dither
  earwax
  echo
  echos
  effects
  fade
  FFT
  filter
  flanger
  mcompand
  mixer
  noiseprof
  noisered
  pad
  pan
  phaser
  pitch
  polyphas
  rate
  repeat
  resample
  reverb
  reverse
  silence
  skeleff
  speed
  stat
  stretch
  swap
  synth
  tremolo
  trim
  vibro
  vol
)
set(formats_srcs
  8svx
  adpcm
  adpcms
  aifc-fmt
  aiff
  aiff-fmt
  al-fmt
  au
  auto
  avr
  cdr
  cvsd
  cvsd-fmt
  dat
  dvms-fmt
  formats
  g711
  g721
  g723_24
  g723_40
  g72x
  gsm
  hcom
  ima-fmt
  ima_rw
  la-fmt
  lpc10
  lu-fmt
  maud
  nulfile
  prc
  raw
  raw-fmt
  s1-fmt
  s2-fmt
  s3-fmt
  s4-fmt
  sf
  skelform
  smp
  sndrtool
  sphere
  tx16w
  u1-fmt
  u2-fmt
  u3-fmt
  u4-fmt
  ul-fmt
  voc
  vox
  vox-fmt
  wav
  wve
  xa
)
add_library(lib${PROJECT_NAME}
  getopt
  getopt1
  misc
  soxio
  soxstdint
  util
  xmalloc
  ${effects_srcs}
  ${formats_srcs}
  ${optional_srcs}
)
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.c)
target_link_libraries(${PROJECT_NAME} lib${PROJECT_NAME} lpc10 ${optional_libs})
add_executable(sox_sample_test sox_sample_test.c)
add_custom_target(rec ALL ln -sf sox rec DEPENDS sox)
add_custom_target(play ALL ln -sf sox play DEPENDS sox)
find_program(CTAGS NAMES exuberant-ctags ctags)
add_custom_target(tags ${CTAGS} --recurse --extra=fq ${CMAKE_CURRENT_SOURCE_DIR})
