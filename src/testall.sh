effect="$*"
t() {
	format=$1
	shift
	opts="$*"

	echo "Format: $format   Options: $opts"
	./sox monkey.wav $opts /tmp/monkey.$format $effect
	./sox $opts /tmp/monkey.$format /tmp/monkey1.wav  $effect
}
t 8svx
t aiff
t au 
t avr -u
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
