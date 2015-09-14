redo-ifchange config.sh

. ./config.sh

CFLAGS="$CFLAGS -D_POSIX_C_SOURCE=200809L"
CFLAGS="$CFLAGS -D_ISOC11_SOURCE"

INC="$INC -I src/"

chmod +x $3

cat <<-EOF
	#!/bin/sh
	exec $CC -c -std=c11 $CFLAGS $INC \$@
EOF