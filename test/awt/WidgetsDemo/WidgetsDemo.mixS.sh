#!/bin/bash -x

rm -f *.class

Kaffe -mixSomeJIT pizza.compiler.Main DemoImageDict.java
Kaffe -mixSomeJIT pizza.compiler.Main WidgetsDemo.java

Kaffe -mixSomeJIT WidgetsDemo

rm -f *.class
