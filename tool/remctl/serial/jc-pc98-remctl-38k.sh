#!/bin/bash
linux-host/remctlclient -sb 2 -b 38400 -s /dev/ttyUSB0 -c upload $*
