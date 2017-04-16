# Kaffe

This is the work I did as part of my Master's program.
I took the Kaffe JVM (http://kaffe.org/) and combined it's separate JIT and interpreter modes
to opporunistically JIT compile methods based on a heuristic.

I basically checked in kaffe.1.0.b2 into CVS and then made all modifications into that tree.

It was built to run with static libraries on linux x86.

The CVS has a few tags associated with it, which mark the different stages of development.

It *seems* that the heuristic that is in the code does a JIT compile the 2nd time a method is called. A more sophisticated technique is discussed in my M.Eng. paper. Please send me an email if you would like a copy. Or, you can download the paper as a set of 30 .jpg images (zip'd): Paper.zip

I remember perhaps 20% of the work I did (Oct '98), but am willing to help anyone out if they have questions etc.

This code is licensed under the GPL.

I posted the following to the mailing list when I quit working on this extremely interesting and fun project.

-- begin message --

Exceptions are not 100% there. I think this must be some subtle and hard to find bug somewhere in the stuff I have done.
Only works for i386-Linux and only with static libraries.
The timing measurements assume a Pentium (I have P-75). Apparently the pentium has two addresses wherein one can get the system clock ticks as a 64 bit thing. I used this to get 9-15 nanosecond accuracy.
Timing improvements are there only for trivial code. eg. the stuff I posted about compiling hello world. There are a lot of areas to further reduce timing. Verifying only once etc.
There is no doubt that mixing stuff helps in short lived programs. The real benefit of mixing is probably in inlining code and then JIT'ing it etc. after getting interesting info at runtime.
I think I have a flag called -stat which prints out interesting info on all the called methods. number of branch instructions, exceptions blocks, switch statements, time to interpret etc.
There is actually quite an interesting heuristic that I use to determine when to JIT compile some method. Perhaps I should write a latex doc to explain this thing.
This is a really cool area to work on and is probably an ideal M.S. thesis, because a lot of work can be done. I have barely scratched the surface. I am surprised that no one seems to be doing anything on this?? I can see the number of papers on could publish on this topic :-)

Some problem areas as far as performance is concerned.

The latency in switching from JIT -> intrp and vice versa adds up and contributes to the overall time. I tried to start on this, but felt too lazy.
It is my *belief* that this causes performace to degrade. This should be easy to verify.

The structure of the code needs to be modified with the view of having a mixture. Should make it more efficient. eg. verification etc.
Perhaps GC could be improved upon.
As a note to folks who might try to go further than I have gone, getting Kaffe to do mixture is actually quite trivial, provided one has a basic understanding of assembler and how the frame pointer is used etc. I never knew what the %bp was for in a PC. Now I know :-) The other interesting part was the trapoline. Once I figured this out the rest was just routine.

-- end message --
