#!/bin/bash -x

rm -f *.class

Kaffe -mixJIT pizza.compiler.Main DemoImageDict.java
Kaffe -mixJIT pizza.compiler.Main WidgetsDemo.java

Kaffe -mixJIT WidgetsDemo

rm -f *.class
