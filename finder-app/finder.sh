#!/bin/sh
filesdir=$1
searchstr=$2
if [ $# -ne 2 ]; then
	echo " Usage $0 <filesdir> <searchstring> "
	exit 1
fi
if [ ! -d "$filesdir" ]; then
	echo " ${filesdir} directory does not exists"
	exit 1
fi

filecount=$(find "$filesdir" -type f | wc -l)
linecount=$(grep -r "$searchstr" "$filesdir" | wc -l)
#output
echo "The number of files are $filecount and the number of matching lines are $linecount"

