#!/bin/bash -x

rm -f *.class

Kaffe pizza.compiler.Main DemoImageDict.java
Kaffe pizza.compiler.Main WidgetsDemo.java

Kaffe WidgetsDemo

rm -f *.class
