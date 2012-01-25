configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}config.h)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

if(CMAKE_COMPILER_IS_GNUCC)
  execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion
    OUTPUT_VARIABLE ver)
  string(REGEX REPLACE "([0-9]+)\\.([0-9]+).*" "\\1" major "${ver}")
  string(REGEX REPLACE "([0-9]+)\\.([0-9]+).*" "\\2" minor "${ver}")
  math(EXPR ver "100 * ${major} + ${minor}")
  if(${ver} LESS 403)
    add_definitions(-Wconversion)
  else(${ver} LESS 403)
    add_definitions(-Wtraditional-conversion)
  endif(${ver} LESS 403)
  #add_definitions(-Werror)
endif(CMAKE_COMPILER_IS_GNUCC)

if (NOT EXTERNAL_GSM)
  set(optional_libs ${optional_libs} gsm)
endif (NOT EXTERNAL_GSM)

set(effects_srcs
  bend
  biquad
  biquads
  chorus
  compand
  compandt
  contrast
  dcshift
  delay
  dft_filter
  dither
  divide
  downsample
  earwax
  echo
  echos
  fade
  fft4g
  fir
  firfit
  flanger
  gain
  hilbert
  input
  loudness
  mcompand
  mixer
  noiseprof
  noisered
  output
  overdrive
  pad
  phaser
  rate
  remix
  repeat
  reverb
  reverse
  silence
  sinc
  skeleff
  speed
  splice
  stat
  stats
  stretch
  swap
  synth
  tempo
  tremolo
  trim
  upsample
  vad
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
  avr
  cdr
  cvsd
  cvsd-fmt
  dat
  dvms-fmt
  f4-fmt
  f8-fmt
  g711
  g721
  g723_24
  g723_40
  g72x
  gsm
  gsrt
  hcom
  htk
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
  sounder
  soundtool
  sox-fmt
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

# Uncomment for bit-rot detection on linux
#set(formats_srcs ${formats_srcs} coreaudio sndio sunaudio waveaudio)
#add_definitions(-Isrc/bit-rot)

add_library(lib${PROJECT_NAME}
  effects                 formats_i               libsox_i
  effects_i               ${formats_srcs}         ${optional_srcs}
  effects_i_dsp           getopt
  ${effects_srcs}         util
  formats                 libsox                  xmalloc
)
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.c)
target_link_libraries(${PROJECT_NAME} lib${PROJECT_NAME} lpc10 ${optional_libs})
add_executable(sox_sample_test sox_sample_test.c)
add_executable(example0 example0.c)
target_link_libraries(example0 lib${PROJECT_NAME} lpc10 ${optional_libs})
add_executable(example1 example1.c)
target_link_libraries(example1 lib${PROJECT_NAME} lpc10 ${optional_libs})
add_executable(example2 example2.c)
target_link_libraries(example2 lib${PROJECT_NAME} lpc10 ${optional_libs})
add_executable(example3 example3.c)
target_link_libraries(example3 lib${PROJECT_NAME} lpc10 ${optional_libs})
add_executable(example4 example4.c)
target_link_libraries(example4 lib${PROJECT_NAME} lpc10 ${optional_libs})
add_executable(example5 example5.c)
target_link_libraries(example5 lib${PROJECT_NAME} lpc10 ${optional_libs})
find_program(LN ln)
if (LN)
  add_custom_target(rec ALL ${LN} -sf sox rec DEPENDS sox)
  add_custom_target(play ALL ${LN} -sf sox play DEPENDS sox)
  add_custom_target(soxi ALL ${LN} -sf sox soxi DEPENDS sox)
endif (LN)
find_program(CTAGS NAMES exuberant-ctags ctags)
add_custom_target(tags ${CTAGS} --recurse --extra=fq ${CMAKE_CURRENT_SOURCE_DIR})
