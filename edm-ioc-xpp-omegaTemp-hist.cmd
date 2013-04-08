#! /bin/bash

# Setup edm environment
source /reg/g/pcds/setup/epicsenv-3.14.12.sh

edm -x -m "IOC=XPP:LOM:TCT:HIST,SIG1=XPP:LOM:TCT:01:DATA:IN,SIG2=XPP:LOM:TCT:01:SP1:RGET" -eolc historyScreens/2-histViewer.edl &