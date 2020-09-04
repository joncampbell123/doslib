#!/bin/bash
# crop 334x200 to 162x160 origin 80,8
convert "$1" -crop 180x180+76+6 +repage -brightness-contrast -1x8 "$1.cropped.png"
