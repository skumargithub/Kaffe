#!/bin/bash -x

rm -f *.class

Kaffe -disableJIT pizza.compiler.Main DemoImageDict.java
Kaffe -disableJIT pizza.compiler.Main WidgetsDemo.java

Kaffe -disableJIT WidgetsDemo

rm -f *.class
