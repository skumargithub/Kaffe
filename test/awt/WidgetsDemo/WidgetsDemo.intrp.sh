#!/bin/bash -x

rm -f *.class

Kaffe.intrp -disableJIT pizza.compiler.Main DemoImageDict.java
Kaffe.intrp -disableJIT pizza.compiler.Main WidgetsDemo.java

Kaffe.intrp -disableJIT WidgetsDemo

rm -f *.class
