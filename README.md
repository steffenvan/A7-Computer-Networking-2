# Chat Server
## This program was implemented in the course "Computer Systems" at University of Copenhagen 2017.

***

The program is mainly divided into 4 modules: `command`, `message_server`, `name_server`, and `peer`. 


## Usage

1. Navigate into the `src` folder. 
2. Compile and link the binaries by executing `make` in the terminal.
3. Open a new terminal windown and navigate into the `src` folder. 
4. In either of the terminal windows, start the name server by executing `./name_server`
5. In the other terminal start a client by `./peer`. 
6. The syntax for the login is `/login <username> <password> <ip> <port>`. For instance `login alice mypassword localhost 8081`.
