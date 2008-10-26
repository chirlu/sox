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
  add_definitions(-Werror)
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
  bend            earwax          mixer           remix           stretch
  biquad          echo            noiseprof       repeat          swap
  biquads         echos           noisered        resample        synth
  chorus          fade            normalise       reverb          tempo
  compand         fft4g           output          reverse         tremolo
  compandt        filter          pad             silence         trim
  contrast        flanger         pan             skeleff         vol
  dcshift         input           phaser          speed
  delay           loudness        polyphas        splice
  dither          mcompand        rate            stat
)
set(formats_srcs
  8svx            dat             ima-fmt         s3-fmt          u3-fmt
  adpcm           dvms-fmt        ima_rw          s4-fmt          u4-fmt
  adpcms          f4-fmt          la-fmt          sf              ul-fmt
  aifc-fmt        f8-fmt          lpc10.c         skelform        voc
  aiff            g711            lu-fmt          smp             vox
  aiff-fmt        g721            maud            sounder         vox-fmt
  al-fmt          g723_24         nulfile         soundtool       wav
  au              g723_40         prc             sox-fmt         wve
  avr             g72x            raw             sphere          xa
  cdr             gsm.c           raw-fmt         tx16w
  cvsd            hcom            s1-fmt          u1-fmt
  cvsd-fmt        htk             s2-fmt          u2-fmt
)
add_library(lib${PROJECT_NAME}
  effects                 ${formats_srcs}         ${optional_srcs}
  effects_i               getopt                  soxstdint
  ${effects_srcs}         getopt1                 util
  formats                 libsox                  xmalloc
  formats_i               libsox_i
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
find_program(LN ln)
if (LN)
  add_custom_target(rec ALL ${LN} -sf sox rec DEPENDS sox)
  add_custom_target(play ALL ${LN} -sf sox play DEPENDS sox)
  add_custom_target(soxi ALL ${LN} -sf sox soxi DEPENDS sox)
endif (LN)
find_program(CTAGS NAMES exuberant-ctags ctags)
add_custom_target(tags ${CTAGS} --recurse --extra=fq ${CMAKE_CURRENT_SOURCE_DIR})
