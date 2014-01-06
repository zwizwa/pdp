#!/bin/bash
cd $(dirname $0)
exec gdb -x example01.gdb -i=mi --args /usr/local/bin/pd -nogui -nrt -lib pdp example01.pd
