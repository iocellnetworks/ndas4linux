#!/bin/bash

export NDAS_KERNEL_PATH=/usr/src/linux-headers-2.6.20.3-ubuntu1-lock-dbg
make linux64-clean linux64-devall
