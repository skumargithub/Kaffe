#!/bin/bash -x

rm -f Calc.class
Kaffe.jit pizza.compiler.Main Calc.java
Kaffe.jit Calc
