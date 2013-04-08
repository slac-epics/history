#! /bin/bash

# Setup edm environment
source /reg/g/pcds/setup/epicsenv-3.14.12.sh

export SIG=LAS:R52:IOC:49:FilteredPhase
#edm  -eolc								\
#-m "SIG=$SIG"							\
#-m "DUR=LAST_N"							\
#historyScreens/emb-hist-viewer.edl		\
#historyScreens/histViewer.edl			\
#&

export SIG1=LAS:R52:IOC:49:PID:OutputValue
export SIG2=LAS:R52:IOC:49:PID:ITerm
export SIG3=LAS:R52:IOC:49:PID:PTerm
export SIG4=LAS:R52:IOC:49:PID:DTerm
edm  -eolc								\
-m "SIG1=$SIG1"							\
-m "SIG2=$SIG2"							\
-m "SIG3=$SIG3"							\
-m "SIG4=$SIG4"							\
-m "DUR=LAST_N"							\
-m "TITLE=Development"					\
historyScreens/emb-2-2-hist-viewer.edl	\
historyScreens/2-2-histViewer.edl		\
&

