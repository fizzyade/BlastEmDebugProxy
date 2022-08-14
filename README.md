# On demand proxy for GDB debugging in BlastEm

Under macOS BlastEm uses a pipe for GDB, I was unable to get this to work properly directly with BlastEm as a remote target inside CLion.

There was a discussion on the BlastEm discord where we made some progress that got GDB debugging working better, but I could not get it working via CLion directly as a remote target over the pipe, CLion just failed.

I decided to try and see if a proxy application that started a BlastEm instance on demand on connection from GDB would work, and it did!  

So this application allows you to create server instances to run these proxies.

You create a server on a port which is bound to a specific ROM binary as we need to start GDB with the ROM image as GDB does not know about the ROM, for example you create a server instance on port "1234" and set the ROM to the raw ROM binary "my_game1.md", you can create multiple servers, so you could bind port "1235" to a different rom binary "my_ganme2.md" and so on.

Inside your IDE you can then tell it to use a GDB remote target, in my case I in CLion I set the remote target to :1234 and I can then set a breakpoint, start debugging in CLion and my proxy accepts the incoming GDB debug session, starts BlastEm in debug mode with the appropriate ROM and then passes the debug session back and forward.

