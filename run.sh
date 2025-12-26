#!/bin/bash

gcc main.c play.c lmath.c eval.c uci.c -s -g "$@" -o chess