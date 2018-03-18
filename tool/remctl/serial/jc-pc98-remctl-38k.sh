#!/bin/bash
linux-host/remctlclient -sb 2 -b 9600 -s /dev/ttyUSB0 -c upload $*
