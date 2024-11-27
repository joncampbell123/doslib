#!/bin/bash
rsync -vrx --update --progress --delete-before --delete-excluded --exclude=\*.h --exclude=\*.c --exclude=\*.obj --exclude=\*.obo --exclude=\*.mak --exclude=\*.sh --exclude=\*.map --exclude=\*.cmd --exclude=sync-dos.sh /usr/src/doslib/testset/dispbmp/dos .
