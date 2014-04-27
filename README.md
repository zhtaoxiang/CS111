CS111
=====

Operating Systems, Winter '13

####Lab 1: Time Travel Shell
The standard POSIX shell executes the code serially: it waits for the command in the first line to finish before starting the command in the second line. Your goal is to speed this up by running commands in parallel when it is safe to do so. In this example, it is safe to start running the second command before the first has finished, because the second command does not read any file that the first command writes. However, the third command cannot be started until the first command finishes, because the first command writes a file c that the third command reads. The goal is to write a prototype for a shell that runs code like the above considerably faster than standard shells do, by exploiting the abovementioned parallelism. 

####Lab 2: Ramdisk
Write a Linux kernel module that implements a ramdisk, an in-memory block device, that will support the usual read and write operations. One can read and write the ramdisk by reading and writing a file, for instance one named /dev/osprda. The ramdisk will also support a special read/write locking feature, where a process can gain exclusive access to the ramdisk. This, of course, leads to some interesting synchronization issues.

####Lab 3:
Write a file system driver for Linux by implementing the routines that handle free block management, changing file sizes, read/write to the file, reading a directory file, deleting symbolic links, creating files, and conditional symlinks.

####Lab 4: 
Distributed computing and security via defensive programming in P2P network.

####Partners:
Stanley Ku <br>
Valerie Runge
