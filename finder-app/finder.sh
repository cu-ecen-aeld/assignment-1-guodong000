#!/bin/sh

if [ $# -lt 2 ]; then
    echo "Usage: $0 filesdir searchstr"
    exit 1
fi

if [ ! -d $1 ]; then
    echo "$1 is not a directory."
    echo "Usage: $0 filesdir searchstr"
    exit 1
fi

fcount=$(ls -1 "$1" | wc -l)
mcount=$(grep -r "$2" "$1" | wc -l)
echo "The number of files are $fcount and the number of matching lines are $mcount"

