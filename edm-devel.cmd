#! /bin/bash

# Setup edm environment
source /reg/g/pcds/setup/epicsenv-3.14.12.sh

export SIG=LAS:R52:IOC:49:FilteredPhase
edm  -eolc								\
-m "SIG=$SIG"							\
-m "DUR=LAST_N"							\
historyScreens/emb-hist-viewer.edl		\
historyScreens/histViewer.edl			\
&

