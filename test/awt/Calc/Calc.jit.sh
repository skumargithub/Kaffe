#!/bin/bash -x

rm -f Calc.class
Kaffe pizza.compiler.Main Calc.java
Kaffe Calc
rm -f Calc.class
