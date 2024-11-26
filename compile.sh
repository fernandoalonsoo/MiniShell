#!/bin/bash

# Compilar el archivo myshell.c con la librería libparser.a
gcc -Wall -Werror -Wextra myshell.c libparser.a -o myshell -static


