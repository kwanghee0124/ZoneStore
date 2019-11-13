#!/bin/bash

ls $1 | while read line
do
    ./parser.sh $1/$line
done
