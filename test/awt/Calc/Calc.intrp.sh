#!/bin/bash -x

rm -f Calc.class
Kaffe.intrp -disableJIT pizza.compiler.Main Calc.java
Kaffe.intrp -disableJIT Calc
rm -f Calc.class
