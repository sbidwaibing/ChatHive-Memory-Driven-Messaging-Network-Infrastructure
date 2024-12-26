# <p align="center">ChatHive: Memory Driven Messaging Network Infrastructure</p>
<p align="justify"> A robust distributed chat system featuring advanced IPC (pipes, shared memory, semaphores, FIFO), networking (TCP sockets), SQLite integration, and concurrency (multi-threading, select). Utilizes fork, file descriptors, and dynamic memory management for scalable, synchronized, real-time communication</p>

### <p align="justify">Note: 
<ol> <li>ChatHive is divided into five modules, each developed to showcase the modular implementation of intermediate computer science concepts, methods, and techniques.</li> <li>ChatHive originated as a coursework project for CS551: Systems Programming. Its development was guided by extensive discussions with Prof. Zerksis D. Umrigar, whose support and insights are sincerely appreciated.</li> <li>Each project module consists its separate README and steps to compile the project</li> </ol> </p>

### <p align="justify">The project has been developed in five iterations as outlined below:</p>

- Iteration 1: Advanced C Programming & ADT Development
<br/>Focus: Developing a chat ADT, dynamic memory management, and initial chat I/O functionality. 
<br/>Techniques: Manual memory management, ADT design, and modular programming. 

- Iteration 2: Process Creation and Anonymous Pipes
<br/>Focus: Implementing client-server communication using fork() and anonymous pipes for IPC, along with SQLite database integration. 
<br/>Techniques: Process creation, anonymous pipes, and basic concurrent programming. 

- Iteration 3: Named Pipes and Concurrent Servers
<br/>Focus: Migrating to named pipes (FIFOs) for IPC, introducing concurrent server processes using the double-fork technique, and client-server architecture enhancement. 
<br/>Techniques: Named pipes, daemon processes, and concurrency with worker processes. 

- Iteration 4: Shared Memory and POSIX Semaphores
<br/>Focus: Replacing pipes with shared memory and synchronizing communication using POSIX semaphores for efficient IPC. 
<br/>Techniques: Shared memory allocation, semaphores for synchronization, and advanced inter-process communication. 

- Iteration 5: Network Programming and TCP/IP
<br/>Focus: Transforming the project into a fully networked distributed chat system using TCP sockets for real-time messaging. 
<br/>Techniques: TCP/IP socket programming, multi-threading or select() for concurrency, and user notifications for scalable networking. Each part builds upon the previous one, progressively introducing new concepts and techniques to develop a fully functional, distributed, and scalable chat system.

#### Note: 
The code for the above-mentioned project iterations is provided in the projects directory labeled as prj_No.
