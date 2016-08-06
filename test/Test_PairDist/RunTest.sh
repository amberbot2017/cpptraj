#!/bin/bash

. ../MasterTest.sh

in=Pr.in
out=Pr.dat

CleanFiles $in $out

INPUT="-i $in"

cat > $in <<EOF
parm ../tz2.parm7
trajin ../tz2.crd

pairdist P0 out $out mask "*" delta 0.1 maxdist 36.25
EOF

RunCpptraj "PairDist Test."
DoTest ${out}.save $out

EndTest

exit 0
