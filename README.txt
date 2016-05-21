Notification Junction
--------------------
Notification Junction is an extendable linux based framework that will interface with multiple applications and multiple Notification Providers, abbreviated as NP(s). Notification Junction is conceptually analogous to an M x N multiplexer, there being M applications and N Notification Providers. Its purpose can be understood by considering the  “doorbell analogy”. In a house, one does not need to check periodically whether there’s someone at the door. One just needs to respond to the doorbell. Similarly, instead of polling periodically to identify changes in a system, CPU cycles can be saved by leveraging system change notifications provided by NPs. Applications will register with the Notification Junction for notifications they are interested in. The Notification Junction will interface with the NPs in turn using NP-specific libraries. The Notification Junction will use multiple threads to interact with the multiple NPs. The Notification Junction has two interfaces - 
	1. Application-side Interface (Command Line Interface)
	2. NP-side Interface (Libraries)
This framework will be  extendable, robust and modular. It will abstract away the differences between the notification acquisition methods of different NPs  and make leveraging notifications  simple and uniform across all types of notifications. C Programming language will be used for implementation.

To run a demo, do the following - 

cd to the directory including NJ code.

In nj.h, choose the datastructure that is needed (HASH, LIST, BOTH) in the preprocessor directive and save.

$make    clean

$make

Now, to register inotify, do

$./np_register    npname::inotify##dir::~/Desktop##flags::IN_CREATE*IN_DELETE*IN_MODIFY##TYPE::N##count::2

We have a code file which we call “demo_dummyapp.c”, which uses the nj_nonblocklib.so library, to write arguments to functions app_register, app_unregister and app_getnotify, on the socket for the NJ to read and process. Take a look at the arguments given through the library calls and change them if needed.

$./demo_dummyapp    <pathof directory to be monitored>

Below is the analysis of the non-blocking call, which happens by default in the demo_dummyapp.c :


The application carries on with its functionality (prints '*' all over the screen), which signifies that it doesn't have to poll for the notifications, which is the essence of Notification Junction.

We should now make an event occur.

Create a file in the directory to be monitored. 

$touch    <path to directory to be monitored>/<filename>

This is the kind of notification that demo_dummyapp has registered for, from inotify.

Now that the event has occured, NJ has sent a signal to the application, hence giving the control to the signal handler which waits for us to press a key. This has just been done to ensure legibility of the notification amidst the * that was being printed indefinitely.

After a key is entered, the control of the application passes to the main function of the dummyapp in which it prints * again. This signifies that the application has resumed its work, possibly on the notification that just arrived.

Meanwhile, the NJ is alive and ready for the next requests.
