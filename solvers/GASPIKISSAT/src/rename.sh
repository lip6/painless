#!/bin/bash

for f in *.c; do
    mv -- "$f" "${f%.c}.cc"
done

