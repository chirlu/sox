effect="$*"
t() {
	format=$1
	shift
	opts="$*"

	echo "Format: $format   Options: $opts"
	/usr/local/valgrind/bin/valgrind --tool=memcheck ./sox monkey.voc $opts /tmp/monkey.$format $effect
	/usr/local/valgrind/bin/valgrind --tool=memcheck ./sox $opts /tmp/monkey.$format /tmp/monkey1.voc  $effect
}
t 8svx
t aiff
t au 
t avr
t cdr
t cvs
t dat
t hcom -r 22050
t maud
t prc
t sf 
t smp
t sndt 
t txw
t ub -r 8130
t vms
t voc
t vox -r 8130
t wav
t wve
