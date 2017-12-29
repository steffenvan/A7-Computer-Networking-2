#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "name_server.h"
#include "command.h"
#include "csapp.h"
#define PORT "8080"

#define ARGNUM 0 // TODO: Put the number of you want to take

struct user_node *findUser(char *username);

struct client_info {
  int socket_fd;
  struct sockaddr addr;
  socklen_t addr_len;
};

struct user_node {
  char *username;
  char *address;
  int port;
  struct client_info *client;
  struct user_node *next;
  struct user_node *previous;
};

struct user_node *users = NULL;
size_t users_count = 0;

bool rio_writei(int fd, int val) {
  char *digits = "0123456789";
  size_t part = val/10;
  size_t dig = val % 10;
  bool ok = true;
  if (part != 0) {
    ok &= rio_writei(fd, part);
  }
  ok &= rio_writen(fd, digits+dig, 1);
  return !ok;
}

struct user_node *createUser(char *username, struct client_info *client, char *address, int port) {
  if (findUser(username) != NULL) {
    return NULL;
  }
  struct user_node *result = malloc(sizeof(*result));
  result -> username = username;
  result -> client = client;
  result -> address = address;
  result -> port = port;
  result -> next = users;
  result -> previous = NULL;
  if (users != NULL) {
    users -> previous = result;
  }
  users = result;
  users_count++;

  return result;
}

struct user_node *findUser(char *username) {
  printf("username is: %s\n", username);
  for (struct user_node *user = users; user != NULL; user = user -> next) {
    if (strcmp((user -> username), username) == 0) {
      return user;
    }
  }
  return NULL;
}

void deleteUser(struct user_node *user) {

  if (user == users) {
    users = user -> next;
  }

  if (user -> previous != NULL) {
    user -> previous -> next = user -> next;
  }
  if (user -> next != NULL) {
    user -> next -> previous = user -> previous;
  }
  users_count--;
  Free(user -> username);
  Free(user);
}

void *thread (void *vargp);

int main(int argc, char**argv) {
  if (argc != ARGNUM + 1) {
      printf("%s expects %d arguments.\n", (argv[0]+2), ARGNUM);
      return(0);
  }

  int server_fd = Open_listenfd(PORT);

  pthread_t tid;

  while(1) {
    struct client_info *client = malloc(sizeof(*client));
    client -> addr_len = sizeof(client -> addr);
    client -> socket_fd = Accept(server_fd, &(client -> addr), &(client -> addr_len));
    printf("Received a connection, start typing\n");
    Pthread_create(&tid, NULL, thread, &(client -> socket_fd));

  }
  return 0;
}

bool sendUser(int fd, struct user_node *user) {

  bool writefailed = false;

  char *buffer = "user ";
  writefailed |= rio_writen(fd, buffer, strlen(buffer)) < 0;
  writefailed |= rio_writen(fd, user -> username, strlen(user -> username)) < 0;
  writefailed |= rio_writen(fd, " ", 1) < 0;
  writefailed |= rio_writen(fd, user -> address, strlen(user -> address)) < 0;
  writefailed |= rio_writen(fd, " ", 1) < 0;
  writefailed |= rio_writei(fd, user -> port);
  writefailed |= rio_writen(fd, "\n", 1) < 0;
  printf("%s\n", user -> username);
  Fputs(writefailed ? "true\n" : "false\n", stdout);
  return writefailed;

}

void cleanThread(struct client_info *client, struct command_stream_info *in) {

  Close(client -> socket_fd);
  destroyCommandStream(in);
  Free(client);
  Pthread_exit(NULL);
}

void *thread(void *vargp) {
    Pthread_detach(pthread_self()); //line:conc:echoservert:detach
    struct client_info *client = ((struct client_info*)vargp);
    struct command_stream_info *in = makeCommandStream(client -> socket_fd);
    char *command;
    getCommand(in);

    if (commandGetString (&command, in) != 0 ||
        strcmp(command, "login") != 0) {
      printf("Recieved malformed input, closing connection\n");
      cleanThread(client, in);
    }
    char *username = NULL;
    char *address = NULL;
    int port = 0;
    if (commandGetString (&username, in) != 0 ||
        commandGetString (&address, in) != 0 ||
        commandGetInt (&port, in) != 0 ||
        commandHasNext(in)) {
      printf("Recieved malformed login, closing connection\n");
      cleanThread(client, in);
    }
    //string dup, because getcommand deallocates all objects;
    username = strdup(username);
    address = strdup(address);
    struct user_node *user = createUser(username, client, address, port);
    if (user == NULL) {
      printf("User %s already created, exiting...\n", username);
      Free(username);
      cleanThread(client, in);
    }
    printf("Registered: %s\n", username);
    while (1) {
      if (getCommand(in) != 0) {
        break;
      }
      if (commandGetString(&command, in) != 0) {
        break;
      }
      bool writefailed = false;
      if (strcmp(command, "list") == 0) {
        char *buffer = "online ";
        writefailed |= rio_writen(client -> socket_fd, buffer, strlen(buffer)) < 0;
        writefailed |= rio_writei(client -> socket_fd, users_count);
        writefailed |= rio_writen(client -> socket_fd, "\n", 1) < 0;
        for (struct user_node *user = users; user != NULL; user = user -> next) {
          writefailed |= sendUser(client -> socket_fd, user);
        }
        buffer = "end\n";
        writefailed |= rio_writen(client -> socket_fd, buffer, strlen(buffer)) < 0;
      }
      else if(strcmp(command, "find") == 0) {
        char *username;
        if (commandGetString(&username, in) != 0) {
          printf("Malformed input\n");
          break;
        }
        struct user_node *user = findUser(username);
        if (user == NULL) {
          char *buffer = "no_user_found\n";
          writefailed |= rio_writen(client -> socket_fd, buffer, strlen(buffer)) < 0;
        }
        else {
          writefailed |= sendUser(client -> socket_fd, user);
        }
      }
      else if(strcmp(command, "exit") == 0) {
        break;
      }
      if (writefailed) {
        break;
      }
    }
    printf("%s left\n", username);
    deleteUser(user);
    cleanThread(client, in);
    return NULL;
}
