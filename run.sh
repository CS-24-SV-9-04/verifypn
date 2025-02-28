#!/bin/bash
cp staging/verifypn-linux64-handleManyTokens-b0a5d53 verifypn-linux64 || exit
./create_jobs.py -o "handleManyTokens.b0a5d53" -m /nfs/petrinet/mcc/2024/colour/ -g -S RDFS
