configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}config.h)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

if(CMAKE_COMPILER_IS_GNUCC)
  add_definitions(-Wconversion -Werror)
  #add_definitions(-Wno-missing-field-initializers)
endif(CMAKE_COMPILER_IS_GNUCC)

if (NOT EXTERNAL_GSM)
  set(optional_libs ${optional_libs} gsm)
endif (NOT EXTERNAL_GSM)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/soxstdint.h
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/soxstdint.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/soxstdint.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/soxstdint.h.cmake
)

# Format with: !xargs echo|tr ' ' '\n'|sort|column|expand|sed 's/^/  /'
set(effects_srcs
  biquad          echo            noiseprof       repeat          swap
  biquads         echos           noisered        resample        synth
  chorus          fade            normalise       reverb          tempo
  compand         FFT             output          reverse         tremolo
  compandt        filter          pad             silence         trim
  contrast        flanger         pan             skeleff         vol
  dcshift         input           phaser          speed
  delay           key             pitch           splice
  dither          mcompand        polyphas        stat
  earwax          mixer           remix           stretch
)
set(formats_srcs
  8svx            cvsd-fmt        ima-fmt         s2-fmt          u2-fmt
  adpcm           dat             ima_rw          s3-fmt          u3-fmt
  adpcms          dvms-fmt        la-fmt          s4-fmt          u4-fmt
  aifc-fmt        g711            lpc10.c         sf              ul-fmt
  aiff            g721            lu-fmt          skelform        voc
  aiff-fmt        g723_24         maud            smp             vox
  al-fmt          g723_40         nulfile         sounder         vox-fmt
  au              g72x            prc             soundtool       wav
  avr             gsm.c           raw             sphere          wve
  cdr             hcom            raw-fmt         tx16w           xa
  cvsd            htk             s1-fmt          u1-fmt
)
add_library(lib${PROJECT_NAME}
  effects                 formats_i               ${optional_srcs}
  effects_i               ${formats_srcs}         soxstdint
  ${effects_srcs}         libsox                  util
  formats                 libsox_i                xmalloc
)
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.c getopt getopt1)
target_link_libraries(${PROJECT_NAME} lib${PROJECT_NAME} lpc10 ${optional_libs})
add_executable(sox_sample_test sox_sample_test.c)
add_executable(example0 example0.c)
target_link_libraries(example0 lib${PROJECT_NAME} lpc10 ${optional_libs})
add_executable(example1 example1.c)
target_link_libraries(example1 lib${PROJECT_NAME} lpc10 ${optional_libs})
add_executable(example2 example2.c)
target_link_libraries(example2 lib${PROJECT_NAME} lpc10 ${optional_libs})
find_program(LN ln)
if (LN)
  add_custom_target(rec ALL ${LN} -sf sox rec DEPENDS sox)
  add_custom_target(play ALL ${LN} -sf sox play DEPENDS sox)
  add_custom_target(soxi ALL ${LN} -sf sox soxi DEPENDS sox)
endif (LN)
find_program(CTAGS NAMES exuberant-ctags ctags)
add_custom_target(tags ${CTAGS} --recurse --extra=fq ${CMAKE_CURRENT_SOURCE_DIR})
