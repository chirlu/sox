include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library(lpc10 analys bsynz chanwr dcbias decode deemp difmag dyptrk encode energy f2clib ham84 hp100 invert irc2pc ivfilt lpcdec lpcenc lpcini lpfilt median mload onset pitsyn placea placev preemp prepro random rcchk synths tbdm voicin vparms)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../src)
