#!/bin/bash

# Compilar el archivo myshell.c con la librería libparser.a
gcc -Wall -Wextra myshell.c libparser.a -o myshell -static