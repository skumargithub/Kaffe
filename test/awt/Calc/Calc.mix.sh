#!/bin/bash -x

rm -f Calc.class
Kaffe -mixJIT pizza.compiler.Main Calc.java
Kaffe -mixJIT Calc
rm -f Calc.class
