#!/bin/bash

# Compilar el archivo myshell.c con la librería libparser.a
gcc -Wall -Werror -Wetra myshell.c libparser.a -o myshell -static


