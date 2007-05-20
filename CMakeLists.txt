project(sox)

if(CMAKE_COMPILER_IS_GNUCC)
  add_definitions(-Wall -W -Wmissing-prototypes -Wstrict-prototypes -pedantic)
endif(CMAKE_COMPILER_IS_GNUCC)

subdirs(src lpc10)
