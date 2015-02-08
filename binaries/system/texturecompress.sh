#!/bin/bash

# MALI etcpack script

set -e

ETCPACKTOOL=`which etcpack`

if [ $ETCPACKTOOL == '' ]; then
    echo "Can't find etcpack in your path";
    exit 1
fi

INFILE=''
OUTFILE=''
FORMAT=''
MIPMAPS=''
while [ $# -gt 0 ]; do
    case $1 in
        -in )
                shift
                INFILE=`realpath "$1"`
                ;;
        -out )
                shift
                OUTFILE="$1"
                ;;
        -f )
                shift
                FORMAT="$1"
                ;;
        -mipmaps )
                MIPMAPS="$1"
                ;;
        * )
                exit 1
     esac
     shift
done

if [ "$INFILE" = "" ]; then
    echo "Missing -in parameter"
    exit 1
fi

if [ "$OUTFILE" = "" ]; then
    echo "Missing -out parameter"
    exit 1
fi

if [ "$FORMAT" = "" ]; then
    echo "Missing -f parameter"
    exit 1
fi

INFILEDIR=$(dirname "$INFILE")
KTXFILENAME=$(basename "$INFILE")
KTXFILENAME="$INFILEDIR/${KTXFILENAME%.*}.ktx"

if [ "$MIPMAPS" = "" ]; then
    $ETCPACKTOOL "$INFILE" "$INFILEDIR" -c etc2 -ktx -f "$FORMAT"
else
    $ETCPACKTOOL "$INFILE" "$INFILEDIR" -c etc2 -ktx -f "$FORMAT" "$MIPMAPS"
fi

if [ ! -d $(dirname "$OUTFILE") ]; then
    mkdir -p $(dirname "$OUTFILE")
fi

mv "$KTXFILENAME" "$OUTFILE"

exit 0
