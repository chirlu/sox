# Microsoft Developer Studio Project File - Name="stlib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=stlib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "stlib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "stlib.mak" CFG="stlib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "stlib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "stlib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "stlib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x40d /d "NDEBUG"
# ADD RSC /l 0x40d /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "stlib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x40d /d "_DEBUG"
# ADD RSC /l 0x40d /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "stlib - Win32 Release"
# Name "stlib - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\8svx.c
# End Source File
# Begin Source File

SOURCE=.\adpcm.c
# End Source File
# Begin Source File

SOURCE=.\adpcm.h
# End Source File
# Begin Source File

SOURCE=.\aiff.c
# End Source File
# Begin Source File

SOURCE=.\alsa.c
# End Source File
# Begin Source File

SOURCE=.\au.c
# End Source File
# Begin Source File

SOURCE=.\auto.c
# End Source File
# Begin Source File

SOURCE=.\avg.c
# End Source File
# Begin Source File

SOURCE=.\avr.c
# End Source File
# Begin Source File

SOURCE=.\band.c
# End Source File
# Begin Source File

SOURCE=.\bandpass.c
# End Source File
# Begin Source File

SOURCE=.\breject.c
# End Source File
# Begin Source File

SOURCE=.\btrworth.c
# End Source File
# Begin Source File

SOURCE=.\cdr.c
# End Source File
# Begin Source File

SOURCE=.\chorus.c
# End Source File
# Begin Source File

SOURCE=.\compand.c
# End Source File
# Begin Source File

SOURCE=.\copy.c
# End Source File
# Begin Source File

SOURCE=.\cvsd.c
# End Source File
# Begin Source File

SOURCE=.\dat.c
# End Source File
# Begin Source File

SOURCE=.\dcshift.c
# End Source File
# Begin Source File

SOURCE=.\deemphas.c
# End Source File
# Begin Source File

SOURCE=.\earwax.c
# End Source File
# Begin Source File

SOURCE=.\echo.c
# End Source File
# Begin Source File

SOURCE=.\echos.c
# End Source File
# Begin Source File

SOURCE=.\fade.c
# End Source File
# Begin Source File

SOURCE=.\filter.c
# End Source File
# Begin Source File

SOURCE=.\flanger.c
# End Source File
# Begin Source File

SOURCE=.\g711.c
# End Source File
# Begin Source File

SOURCE=.\g711.h
# End Source File
# Begin Source File

SOURCE=.\g721.c
# End Source File
# Begin Source File

SOURCE=.\g723_16.c
# End Source File
# Begin Source File

SOURCE=.\g723_24.c
# End Source File
# Begin Source File

SOURCE=.\g723_40.c
# End Source File
# Begin Source File

SOURCE=.\g72x.c
# End Source File
# Begin Source File

SOURCE=.\getopt.c
# End Source File
# Begin Source File

SOURCE=.\gsm.c
# End Source File
# Begin Source File

SOURCE=.\handlers.c
# End Source File
# Begin Source File

SOURCE=.\hcom.c
# End Source File
# Begin Source File

SOURCE=.\highp.c
# End Source File
# Begin Source File

SOURCE=.\highpass.c
# End Source File
# Begin Source File

SOURCE=.\ima_rw.c
# End Source File
# Begin Source File

SOURCE=.\lowp.c
# End Source File
# Begin Source File

SOURCE=.\lowpass.c
# End Source File
# Begin Source File

SOURCE=.\map.c
# End Source File
# Begin Source File

SOURCE=.\mask.c
# End Source File
# Begin Source File

SOURCE=.\maud.c
# End Source File
# Begin Source File

SOURCE=.\mcompand.c
# End Source File
# Begin Source File

SOURCE=.\misc.c
# End Source File
# Begin Source File

SOURCE=.\mp3.c
# End Source File
# Begin Source File

SOURCE=.\nulfile.c
# End Source File
# Begin Source File

SOURCE=.\oss.c
# End Source File
# Begin Source File

SOURCE=.\pan.c
# End Source File
# Begin Source File

SOURCE=.\phaser.c
# End Source File
# Begin Source File

SOURCE=.\pitch.c
# End Source File
# Begin Source File

SOURCE=.\polyphas.c
# End Source File
# Begin Source File

SOURCE=.\prc.c
# End Source File
# Begin Source File

SOURCE=.\rate.c
# End Source File
# Begin Source File

SOURCE=.\raw.c
# End Source File
# Begin Source File

SOURCE=.\repeat.c
# End Source File
# Begin Source File

SOURCE=.\resample.c
# End Source File
# Begin Source File

SOURCE=.\reverb.c
# End Source File
# Begin Source File

SOURCE=.\reverse.c
# End Source File
# Begin Source File

SOURCE=.\sf.c
# End Source File
# Begin Source File

SOURCE=.\silence.c
# End Source File
# Begin Source File

SOURCE=.\skeleff.c
# End Source File
# Begin Source File

SOURCE=.\smp.c
# End Source File
# Begin Source File

SOURCE=.\sndrtool.c
# End Source File
# Begin Source File

SOURCE=.\speed.c
# End Source File
# Begin Source File

SOURCE=.\sphere.c
# End Source File
# Begin Source File

SOURCE=.\stat.c
# End Source File
# Begin Source File

SOURCE=.\stretch.c
# End Source File
# Begin Source File

SOURCE=.\sunaudio.c
# End Source File
# Begin Source File

SOURCE=.\swap.c
# End Source File
# Begin Source File

SOURCE=.\synth.c
# End Source File
# Begin Source File

SOURCE=.\trim.c
# End Source File
# Begin Source File

SOURCE=.\tx16w.c
# End Source File
# Begin Source File

SOURCE=.\util.c
# End Source File
# Begin Source File

SOURCE=.\vibro.c
# End Source File
# Begin Source File

SOURCE=.\voc.c
# End Source File
# Begin Source File

SOURCE=.\vol.c
# End Source File
# Begin Source File

SOURCE=.\vorbis.c
# End Source File
# Begin Source File

SOURCE=.\vox.c
# End Source File
# Begin Source File

SOURCE=.\wav.c
# End Source File
# Begin Source File

SOURCE=.\wve.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\btrworth.h
# End Source File
# Begin Source File

SOURCE=.\cvsdfilt.h
# End Source File
# Begin Source File

SOURCE=.\g72x.h
# End Source File
# Begin Source File

SOURCE=.\ima_rw.h
# End Source File
# Begin Source File

SOURCE=.\resampl.h
# End Source File
# Begin Source File

SOURCE=.\sfircam.h
# End Source File
# Begin Source File

SOURCE=.\st.h
# End Source File
# Begin Source File

SOURCE=.\st_i.h
# End Source File
# Begin Source File

SOURCE=.\ststdint.h
# End Source File
# Begin Source File

SOURCE=.\wav.h
# End Source File
# End Group
# End Target
# End Project
