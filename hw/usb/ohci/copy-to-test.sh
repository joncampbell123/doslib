#!/bin/bash
autosddexec mount rw ^TEST; mkdir -p /mnt/^TEST/tx/ohci; cp -Rfv dos* /mnt/^TEST/tx/ohci/; autosddexec mount off ^TEST

