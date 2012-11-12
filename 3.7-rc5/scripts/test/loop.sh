#!/bin/bash

for ((i=1;i<=$1;i++)); do echo Iteration $i ; $2; date; done

