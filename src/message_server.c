#include "peer.h"
#include "command.h"

pthread_t tid;
static pthread_key_t key;
static pthread_once_t once_key = PTRHEAD_ONCE_INIT;

struct clean_up_info {

  struct peer_info *peer;
  struct command_stream_info *peer_in;
};

struct peer_node {
  char *username;
  struct peer_info *peer;
  struct peer_node *next;
  struct peer_node *prev;
}

struct peer_info {
  int socket_fd;
  struct sockaddr sock_addr;
  socklen_t addr_len;
};

struct message {
  char *message;
  struct peer_node *sender;
};

struct message messages[1024];
int messageCount = 0;

struct user_node *addUser(char *username, struct peer_info *peer_in) {
  if (lookupUser(username) != NULL) {
    return NULL;
  }
  struct peer_node *user = malloc(sizeof(*user));
  user -> username = username;
  user -> peer = peer_in;
  user -> next = users;
  user -> prev = NULL;
  if(users != NULL) {
    users -> prev = user;
  }
  users = user;
  return user;
}

struct peer_node *users = NULL;

struct peer_node *lookupUser(char *username) {
  for (struct peer_node *user == users; user != NULL; user = user -> next) {
    if (strcmp(user -> username, username) == 0) {
      return user;
    }
  }
  return NULL;
}

int showMessageFrom(peer_node *user) {
  for (int i = 0, j = 0; i < messageCount; i++) {
    if (messages[i].sender == user) {
      outPutMessage();
    }
  }
}

int storeMessage(peer_node *user, char *message) {
  messages[messageCount].message = message;
  messages[messageCount].sender = user;
  messageCount++;
}

int openServer(char *port) {
  Pthread_create(&tid, NULL, thread, port);
}

void *thread (void *vargp) {
  server_fd = Open_listenfd(port);

  Pthread_detach(pthread_self());

  while(1) {
    struct peer_info *peer = malloc(sizeof(*peer));
    peer -> addr_len = sizeof(peer -> addr);
    peer -> socket_fd = Accept(server_fd, &(peer -> addr), &(peer -> addr_len));
    printf("Received a connection, start typing\n");
    Pthread_create(&tid, NULL, messageReceiverThread, &(peer -> socket_fd));

  }
}

void cleanThread(struct peer_info *peer, struct command_stream_info *peer_info) {

  Close(peer -> socket_fd);
  destroyCommandStream(peer_info);
  Free(peer);

  Pthread_exit(NULL);
}

void makeKey () {
  pthread_key_create(&key, cleanThread);
}

void *messageReceiverThread (void *vargp) {

  struct peer_info *sender = ((struct peer_info*)vargp);
  struct command_stream_info *sender_info = makeCommandStream(sender -> socket_fd);
  struct clean_up_info *clean_info = malloc(sizeof(*clean_info));
  clean_info -> peer = sender;
  clean_info -> peer_in = sender_info;

  pthread_once(&once_key, makeKey);

  getCommand(sender_info);
  char *command;

  if (commandGetString(&command, sender_info) != 0) {
    printf("Recieved malformed input, closing connection\n");
    pthread_cleanup_pop(1);
    return NULL;
  }

  if (strcmp(command, "connect") != 0) {
    printf("Recieved malformed command (%s), closing connection\n", command);
    pthread_cleanup_pop(1);
    return NULL;
  }

  char *username = NULL;
  if (commandGetString (&username, sender_info) != 0) {
    printf("Recieved malformed username, closing connection\n");
    pthread_cleanup_pop(1);
    return NULL;
  }
  // Using strdup because getcommand deallocates all objects
  username = strdup(username);
  while (1) {
    if (getCommand(sender_info) != 0) {
      break;
    }
    if(commandGetString(&command, sender_info) != 0) {
      break;
    }
    if (strcmp(command, "say") != 0) {
      printf("Received malformed command (%s)\n", command);
      cleanThread(client_info, sender_info);

    }
    char *message;
    if (commandGetString(&message, sender_info) != 0) {
      printf("Received malformed input (%s)\n", message);
    }
    // Using strdup because getcommand deallocates all objects
    message = strdup(message);
    storeMessage(sender_info, message);
  }
}
