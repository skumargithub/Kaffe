#!/bin/bash -x

rm -f *.class

Kaffe.jit pizza.compiler.Main DemoImageDict.java
Kaffe.jit pizza.compiler.Main WidgetsDemo.java

Kaffe.jit WidgetsDemo

rm -f *.class
