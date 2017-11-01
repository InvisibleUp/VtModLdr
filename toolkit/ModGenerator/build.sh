#!/bin/sh
clang++ -g json.cpp main.cpp -ljansson -lboost_system -lboost_filesystem -lbfd -liberty -ldl -lz -larchive -o modgen
 
