#!/bin/bash -x

rm -f Calc.class
Kaffe -mixSomeJIT pizza.compiler.Main Calc.java
Kaffe -mixSomeJIT Calc
rm -f Calc.class
