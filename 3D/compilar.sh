#!/bin/bash
# Compila o Sapo Surfista 3D.
# Uso:  ./compilar.sh     (depois:  ./sapo )
g++ sapo.cpp -o sapo -lGLEW -lGL -lGLU -lglut -lm -Wall && echo "OK! Rode com: ./sapo"
