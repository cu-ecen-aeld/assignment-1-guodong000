#!/bin/sh

if [ $# -lt 2 ]; then
    echo "Usage: $0 filesdir searchstr"
    exit 1
fi

dir=$(dirname "$1")
if [ ! -d $dir ]; then
    mkdir -p $dir
fi
echo "$2" > "$1"
if [ $? -ne 0 ]; then
    echo "file $1 could not be created."
    exit 1
fi


