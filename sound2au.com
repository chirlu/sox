$   FVER = 'F$Verify(0)'
$ !
$ ! Sound2Au.Com
$ ! translate a variety of sound formats into Sun .au format (compatible with
$ ! DECsound) via SOX and SOUND2SUN.
$ !
$ ! sound2sun was written by Rich Gopstein and Harris Corporation.
$ !
$ ! SOX is part of the Sound Tools package written and distributed by
$ ! 	Lance Norskog, et. al.
$ !
$ ! Usage
$ !	@sound2au file_name [frequency]
$ !
$ ! where:
$ !	file_name = filename template (may contain wildcards)
$ !	frequency = sampling frequency (default 11000 Hz)
$ !
$ ! Modification History
$ !	14 Dec 1992, K. S. Kubo, Created
$ !
$   If P1 .Eqs. "" THEN GOTO USAGE_EXIT
$ !
$   FTMPL 	= F$Parse(P1,"*.SND;")
$   If F$TrnLnm("SOX_DIR") .Nes. "" Then Goto MORE_DEFS
$   SDIR = F$Element(0, "]", F$Environment("PROCEDURE")) + "]"
$   Define/NoLog SOX_DIR 'SDIR'
$ MORE_DEFS:
$   SOX   	= "$ SOX_DIR:SOX"
$   SOUND2SUN	= "$ SOX_DIR:SOUND2SUN"
$   ONAME	= ""
$   If P2 .Nes. "" Then SOUND2SUN = SOUND2SUN + " -f ''P2'"
$ LOOP:
$   FNAME = F$Search(FTMPL)
$   If FNAME .Eqs. "" Then Goto REAL_EXIT
$   If FNAME .Eqs. ONAME Then Goto REAL_EXIT
$   ONAME = FNAME
$   VER   = F$Parse(FNAME,,,"VERSION")
$   FTYPE = F$Parse(FNAME,,,"TYPE")
$   FNAME = FNAME - VER - ";"	! strip version number off
$   BNAME = FNAME - FTYPE	! get the base name
$   SOX 'FNAME' -t .raw -u -b 'BNAME'.raw
$   SOUND2SUN 'BNAME'.raw 'BNAME'.au
$   Delete/NoLog 'BNAME'.raw;
$   Goto LOOP
$ !
$ USAGE_EXIT:
$   Write Sys$Output "Usage:  @SOUND2AU file_name"
$ !
$ REAL_EXIT:
$   FVER = F$Verify('FVER')
$   EXIT 
