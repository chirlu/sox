#!/bin/sh
#
# startrec
#
#   Example idea from Douglas Held.

#   Allows you to start a recording session but only start writing
#   to disk once non-silence is detect. For example, use this to
#   start your favorite command line for recording and walk
#   over to your record player and start the song.  No periods
#   of silence will be recorded.
#
# This script is really just an example... Its just as easy to add
# the "silence 1 5 %2" to the end of the normal rec command.

rec $* silence 1 5 2%
