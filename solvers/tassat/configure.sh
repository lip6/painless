#!/bin/sh
usage () {
cat << EOF
usage: configure.sh [-h][-g][--no-mems][--no-double]

-h   print this command line option summary

-g   compile with debugging, checking and logging support
     (compiles in '-c' and '-l' run time options for checking and logging)

--no-mems     disable computation of memory accesses
              (to measure the slow-down incurred by this feature)

--no-stats    disable computation of increment/decrement stats

-O            disables '-g' and enables '--no-mems --no-stats' and
EOF
}
die () {
  echo "*** configure.sh: $*" 1>&2
  exit 1
}
opt=no
mems=yes
stats=yes
debug=no
while [ $# -gt 0 ]
do
  case $1 in
    -h) usage; exit 0;;
    --no-mems) mems=no;;
    --no-stats) stats=no;;
    -O) opt=yes;;
    -g) debug=yes;;
    *) die "invalid command line option '$1'";;
  esac
  shift
done
if [ $opt = yes ]
then
  debug=no
  mems=no
  stats=no
fi
CC=gcc
if [ $debug = yes ]
then
  CFLAGS="-Wall -g3"
else
  CFLAGS="-Wall -DNDEBUG -O3"
fi
[ $mems = no ] && CFLAGS="$CFLAGS -DNYALSMEMS"
[ $stats = no ] && CFLAGS="$CFLAGS -DNYALSTATS"
LIBS="-lm"
echo "$CC $CFLAGS $LIBS"
rm -f makefile
sed \
  -e "s,@CC@,$CC," \
  -e "s,@CFLAGS@,$CFLAGS," \
  -e "s,@LIBS@,$LIBS," \
makefile.in > makefile
