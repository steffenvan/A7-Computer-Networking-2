#include <stdio.h>
#include "peer.h"
#include "command.h"
#define PORT 8080

#define ARGNUM 0 // TODO: Put the number of you want to take

int clientfd;
struct command_stream_info *client_in;
bool loggedIn = false;

int logoutUser() {
  if (!loggedIn) {
    printf("Not logged in. Use /login to log in.\n");
    return -1;
  }
  // no error handling, because if the response to errors would simply be to close the connection,
  // which is what we're currently doing
  char *buffer = "exit\n";
  rio_writen(clientfd, buffer, strlen(buffer));
  close(clientfd);
  destroyCommandStream(client_in);
  printf("You are now logged out.\n");
  loggedIn = false;
  return 0;
}

int connectClient(char *username, char *password, char *ip, char *port) {
  if (loggedIn) {
    printf("Already logged in. Use /logout to log out.\n");
    return -1;
  }
  clientfd = Open_clientfd("localhost", "8080");
  client_in = makeCommandStream(clientfd);
  char *buffer = "login ";
  Rio_writen(clientfd, buffer, strlen(buffer));
  Rio_writen(clientfd, username, strlen(username));
  Rio_writen(clientfd, " ", 1);
  Rio_writen(clientfd, password, strlen(password));
  Rio_writen(clientfd, " ", 1);
  Rio_writen(clientfd, ip, strlen(ip));
  Rio_writen(clientfd, " ", 1);
  Rio_writen(clientfd, port, strlen(port));
  Rio_writen(clientfd, "\n", 1);
  printf("Welcome %s\n", username);
  loggedIn = true;
  return 0;
}


// print
// int print_message()


int findAllUsers() {
  if (!loggedIn) {
    printf("Not logged in. Use /login to log in.\n");
    return -1;
  }
  char *buffer = "list\n";
  if (rio_writen(clientfd, buffer, strlen(buffer)) < 0 ||
      getCommand(client_in) != 0) {
    printf("Lost connection to server.\n");
    logoutUser();
    return -1;
  }
  char *command;
  if (commandGetString(&command, client_in) != 0 ||
      strcmp(command, "online") != 0) {
    printf("Server communication error.\n");
    logoutUser();
    return -1;
  }
  int online_users;
  if (commandGetInt(&online_users, client_in) != 0 ||
      commandHasNext(client_in)) {
    printf("Server communication error.\n");
    logoutUser();
    return -1;
  }
  printf("%d user(s) online. The list follows:\n", online_users);
  while(1) {
    if (getCommand(client_in) != 0) {
      printf("Lost connection to server.\n");
      logoutUser();
      return -1;
    }
    if(commandGetString(&command, client_in) != 0) {
      printf("Server communication error.\n");
      logoutUser();
      return -1;
    }
    if(strcmp(command, "end") == 0) {
      if (commandHasNext(client_in)) {
        printf("Server communication error.\n");
        logoutUser();
        return -1;
      }
      break;
    }
    else if(strcmp(command, "user") == 0) {
      char *username;
      char *address;
      char *port;
      if(commandGetString(&username, client_in) != 0 ||
         commandGetString(&address, client_in) != 0 ||
         commandGetString(&port, client_in) != 0 ||
         commandHasNext(client_in)) {
        printf("Server communication error");
        logoutUser();
        return -1;
      }
      printf("Nick: %s\n", username);
      printf("IP: %s\n", address);
      printf("Port: %s\n", port);
    }
    else {
      printf("Server communication error");
      logoutUser();
      return -1;
    }

  }
  return 0;
}

int findUser(char *username) {
  if (!loggedIn) {
    printf("Not logged in. Use /login to log in.\n");
    return -1;
  }
  char *buffer = "find ";
  if (rio_writen(clientfd, buffer, strlen(buffer)) < 0 ||
      rio_writen(clientfd, username, strlen(username)) < 0 ||
      rio_writen(clientfd, "\n", 1) < 0 ||
      getCommand(client_in) != 0) {
    printf("Lost connection to server.\n");
    logoutUser();
    return -1;
  }
  char *command;
  if (commandGetString(&command, client_in) != 0) {
    printf("Server communication error.\n");
    logoutUser();
    return -1;
  }
  if (strcmp(command, "user") == 0) {
    char *name;
    char *address;
    char *port;
    if(commandGetString(&name, client_in) != 0 ||
       commandGetString(&address, client_in) != 0 ||
       commandGetString(&port, client_in) != 0 ||
       commandHasNext(client_in)) {
      printf("Server communication error.\n");
      logoutUser();
      return -1;
    }
    printf("%s is online.\n", name);
    printf("IP: %s\n", address);
    printf("Port: %s\n", port);
  }
  else if (strcmp(command, "no_user_found") == 0) {
    printf("%s is not a valid user.\n", username);
  }
  else {
    printf("Server communication error.\n");
    logoutUser();
    return -1;
  }
  return 0;
}



struct peer_node *getUser(char *username) {
  struct peer_node *user = lookupUser(username);
  if (user == NULL) {
    user = connectToUser(username);
  }
  return user;
}

struct peer_node *connectToUser(char *username) {
  return NULL;
}

// int startMessageServer()

int main(int argc, char**argv) {
  if (argc != ARGNUM + 1) {
    printf("%s expects %d arguments.\n", (argv[0]+2), ARGNUM);
    return(0);
  }

  struct command_stream_info *in = makeCommandStream(fileno(stdin));

  while (1) {
    if (getCommand(in) != 0) {
      // No more user input, terminate
      break;
    }
    bool isCommand;
    // Check if the first character is /
    if (commandGetIndicator(&isCommand, in) != 0) {
      printf("Malformed input.\n");
      continue;
    }

    if (!isCommand) {
      printf("Expected command.\n");
    }
    else {
      char *command;
      if (commandGetString(&command, in) != 0) {
        printf("Missing command name\n");
        continue;
      }

      if (strcmp(command, "login") == 0)  {
        char *username = NULL;
        char *password = NULL;
        char *ip = NULL;
        char *port = NULL;
        if (commandGetString(&username, in) != 0 ||
            commandGetString(&password, in) != 0 ||
            commandGetString(&ip, in) != 0 ||
            commandGetString(&port, in) != 0 ||
            commandHasNext(in)) {
          printf("Login syntax: /login <username> <password> <ip> <port>\n");
          continue;
        }
        connectClient(username, password, ip, port);
      }
      else if (strcmp(command, "lookup") == 0 && !commandHasNext(in)) {
        findAllUsers();
      }
      else if(strcmp(command, "lookup") == 0) {
        char *username;
        if (commandGetString(&username, in) != 0 ||
            commandHasNext(in)) {
          printf("Lookup syntax: /lookup <username>, or /lookup\n");
          continue;
        }
        findUser(username);
      }

      else if (strcmp(command, "msg") == 0) {
        char *username;
        if (commandGetString(&username, in) != 0) {
          printf("Msg syntax: /msg <username> <message>\n");
          continue;
        }

      }


      else if (strcmp(command, "logout") == 0) {
        if (commandHasNext(in)) {
          printf("Logout syntax: /logout\n");
          continue;
        }
        logoutUser();
      }
      else if (strcmp(command, "exit") == 0) {
        if (commandHasNext(in)) {
          printf("Exit syntax: /exit\n");
          continue;
        }
        printf("Have a good day\n");
        break;
      }
    }
  }
  if (loggedIn) {
    logoutUser();
  }
  return 0;
}
