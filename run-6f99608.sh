#!/bin/bash
cp bins/verifypn-linux64-6f99608 verifypn-linux64
./create_jobs.py -o "6f99608" -m /nfs/petrinet/mcc/2024/colour/ -S RDFS --colored-successor-generator even -g
