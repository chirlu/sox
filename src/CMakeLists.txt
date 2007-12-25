configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}config.h)

#include_directories(${liblpc10_BINARY_DIR}/lpc10)
#link_directories(${liblpc10_SOURCE_DIR}/lpc10)

if(CMAKE_COMPILER_IS_GNUCC)
#  add_definitions(-Wconversion)
endif(CMAKE_COMPILER_IS_GNUCC)

if (NOT EXTERNAL_GSM)
  set(optional_libs ${optional_libs} gsm)
endif (NOT EXTERNAL_GSM)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/soxstdint.h
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/soxstdint.h.cmake ${CMAKE_CURRENT_SOURCE_DIR}/soxstdint.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/soxstdint.h.cmake
  )

# Format with: !xargs echo|tr ' ' '\n'|sort|column|expand|sed 's/^/  /'
set(effects_srcs
  biquad          echos           noiseprof       resample        synth
  biquads         effects         noisered        reverb          tempo
  chorus          fade            pad             reverse         tremolo
  compand         FFT             pan             silence         trim
  compandt        filter          phaser          skeleff         vibro
  dcshift         flanger         pitch           speed           vol
  dither          key             polyphas        stat
  earwax          mcompand        rate            stretch
  echo            mixer           repeat          swap
)
set(formats_srcs
  8svx            cvsd            hcom            s1-fmt          u2-fmt
  adpcm           cvsd-fmt        ima-fmt         s2-fmt          u3-fmt
  adpcms          dat             ima_rw          s3-fmt          u4-fmt
  aifc-fmt        dvms-fmt        la-fmt          s4-fmt          ul-fmt
  aiff            formats         lpc10.c         sf              voc
  aiff-fmt        g711            lu-fmt          skelform        vox
  al-fmt          g721            maud            smp             vox-fmt
  au              g723_24         nulfile         sndrtool        wav
  auto            g723_40         prc             sphere          wve
  avr             g72x            raw             tx16w           xa
  cdr             gsm             raw-fmt         u1-fmt
)
add_library(lib${PROJECT_NAME}
  ${effects_srcs}         misc                    util
  ${formats_srcs}         ${optional_srcs}        xmalloc
  getopt                  soxio
  getopt1                 soxstdint
)
set(optional_libs /usr/local/lib/libsndfile.so ${optional_libs})
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.c)
target_link_libraries(${PROJECT_NAME} lib${PROJECT_NAME} lpc10 ${optional_libs})
add_executable(sox_sample_test sox_sample_test.c)
add_executable(example1 example1.c)
target_link_libraries(example1 lib${PROJECT_NAME} lpc10 ${optional_libs})
find_program(LN ln)
if (LN)
  add_custom_target(rec ALL ${LN} -sf sox rec DEPENDS sox)
  add_custom_target(play ALL ${LN} -sf sox play DEPENDS sox)
endif (LN)
find_program(CTAGS NAMES exuberant-ctags ctags)
add_custom_target(tags ${CTAGS} --recurse --extra=fq ${CMAKE_CURRENT_SOURCE_DIR})
