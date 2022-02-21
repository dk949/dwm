#!/bin/sh

HEIGHT=$(xrandr | awk '/ connected/ {print $3}' | cut -d'+' -f 1 | cut -d'x' -f 2)

printf "" > gen_conf.h

echo "static const unsigned int borderpx = $HEIGHT / 540; /* border pixel of windows */" >> gen_conf.h

echo "static const unsigned int gappx = $HEIGHT / 180;   /* gaps between windows */" >> gen_conf.h

echo "static const unsigned int snap = $HEIGHT / 67;    /* snap pixel */" >> gen_conf.h
