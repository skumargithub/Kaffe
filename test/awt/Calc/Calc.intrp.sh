#!/bin/bash -x

rm -f Calc.class
Kaffe -disableJIT pizza.compiler.Main Calc.java
Kaffe -disableJIT Calc
rm -f Calc.class
