$   FVER = 'F$VERIFY(0)'
$ !
$ ! test files
$ ! SOX Test script.  This should run without access violating or printing
$ ! any messages.
$ !
$ ! VMS translation of tests.shr
$ !
$ ! Modification History
$ ! 14 Dec 1992, K. S. Kubo, Created
$ !
$ ! NOTES:
$ !	Does not support the voc test w/o checksum and rate byte.
$ !
$   FILE 	= "monkey"
$   PROC_PATH	= F$Environment("PROCEDURE")
$   SOX		= "$ " + F$Parse(PROC_PATH,,,"DEVICE") + -
	F$Parse(PROC_PATH,,,"DIRECTORY") + "SOX"
$ !
$ ! verbose option -- uncomment the following line
$ ! SOX		= SOX + " ""-V"""
$ !
$   ON ERROR THEN GOTO COM_EXIT
$   ON SEVERE THEN GOTO COM_EXIT
$   ON WARNING THEN CONTINUE
$   DELETE/NOLOG ub.raw;*, sb.raw;*, ub2.raw;*, ub2.voc;*, ub.au;*, ub2.sf;*
$   SOX 'FILE'.voc ub.raw
$   SOX -t raw -r 8196 -u -b -c 1 ub.raw -r 8196 -s -b sb.raw
$   SOX -t raw -r 8196 -s -b -c 1 sb.raw -r 8196 -u -b ub2.raw
$   SOX -r 8196 -u -b -c 1 ub2.raw -r 8196 ub2.voc 
$   DIFF/MODE=HEX ub.raw ub2.raw
$   DELETE/NOLOG ub.raw;*, sb.raw;*, ub2.raw;*, ub2.voc;*
$   SOX 'FILE'.au -u -r 8192 -u -b ub.raw
$   SOX -r 8192 -u -b ub.raw -U -b ub.au 
$   SOX ub.au -u ub2.raw 
$   SOX ub.au -w ub2.sf
$   DELETE/NOLOG ub.raw;*, ub.au;*, ub2.raw;*, ub2.sf;*
$ !
$ COM_EXIT:
$   FVER = F$VERIFY('FVER')
$   EXIT
