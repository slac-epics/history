#! /bin/bash

# Setup edm environment
source /reg/g/pcds/setup/epicsenv-3.14.12.sh

edm -x -m "IOC=LAS:IOC:HIST1,SIG=LAS:R52:IOC:49:FilteredPhase" -eolc historyScreens/histViewer.edl &
