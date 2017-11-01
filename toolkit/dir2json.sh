#!/bin/bash
# Converts a directory of files to SrModLdr game metadata KnownSpaces format

wc -c **/* 2> /dev/null | # Count all files \
tr -s "[:blank:]" "," | # Convert varlen to comma delim \
grep -v "total" | # Remove total \
awk -F, '{ if ($2 > 0) print $2 "," $3 }' | # Remove 0-size files; remove leading comma \
jq -R 'split(",") | {"Start": 0, "End": .[0], "File": .[1], "UUID": .[1]}' | # Convert to many JSON objects \
jq -sM 'reduce .[] as $i ([]; . + [$i])' # Combine JSON objects into an array \
