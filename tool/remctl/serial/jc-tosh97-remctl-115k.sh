#!/bin/bash
linux-host/remctlclient -sb 2 -b 115200 -s /dev/ttyUSB0 -c upload $*
