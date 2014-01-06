#!/bin/bash
cd $(dirname $0)
PD=/usr/local/bin/pd
PDP=/home/tom/pdp
exec gdb -x example01.gdb -i=mi --args $PD \
-nogui -nrt -noprefs \
-path $PDP \
-path $PDP/abstractions \
-path $PDP/opengl \
-path $PDP/opengl/abstractions \
-lib pdp \
-lib pdp_opengl \
example01.pd
