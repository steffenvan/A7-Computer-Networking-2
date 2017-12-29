#include "command.h"
#include "message_server.h"
#include <pthread.h>

pthread_t tid;
static pthread_key_t key;
static pthread_once_t once_key = PTHREAD_ONCE_INIT;

struct clean_up_info {

  struct peer_info *peer;
  struct command_stream_info *peer_in;
};

struct peer_node {
  char *username;
  struct peer_info *peer;
  struct peer_node *next;
  struct peer_node *prev;
};

struct peer_info {
  int socket_fd;
  struct sockaddr sock_addr;
  socklen_t addr_len;
};

struct peer_node *senders = NULL;
struct peer_node *addSender(char *username, struct peer_info *peer_in) {
  if (lookupUser(username) != NULL) {
    return NULL;
  }
  struct peer_node *sender = malloc(sizeof(*sender));
  sender -> username = username;
  sender -> peer = peer_in;
  sender -> next = senders;
  sender -> prev = NULL;
  if(senders != NULL) {
    senders -> prev = sender;
  }
  senders = sender;
  return sender;
}


struct peer_node *lookupUser(char *username) {
  for (struct peer_node *sender = senders; sender != NULL; sender = sender -> next) {
    if (strcmp(sender -> username, username) == 0) {
      return sender;
    }
  }
  return NULL;
}

struct message {
  char *message;
  struct peer_node *sender;
};

struct message messages[1024];
int messageCount = 0;
int showMessageFrom(struct peer_node *sender) {
  int j = 0;
  for (int i = 0; i < messageCount; i++) {
    if (messages[i].sender == sender) {
      //outPutMessage();
    }
  }
}

int storeMessage(struct peer_node *sender, char *message) {
  messages[messageCount].message = message;
  messages[messageCount].sender = sender;
  messageCount++;
}

void *receiverThread (void *vargp);
void *messageReceiverThread (void *vargp);

int openServer(char *port) {
  Pthread_create(&tid, NULL, receiverThread, port);
}

void *receiverThread (void *vargp) {
  char *port = (char*) vargp;
  int server_fd = Open_listenfd(port);

  Pthread_detach(pthread_self());

  while(1) {
    struct peer_info *peer = malloc(sizeof(*peer));
    peer -> addr_len = sizeof(peer -> sock_addr);
    peer -> socket_fd = Accept(server_fd, &(peer -> sock_addr), &(peer -> addr_len));
    printf("Received a connection, start typing\n");
    Pthread_create(&tid, NULL, messageReceiverThread, &(peer -> socket_fd));

  }
}

void cleanReceiverThread(void *ptr) {

  struct clean_up_info *clean_in = (struct clean_up_info*)ptr;
  Close(clean_in -> peer -> socket_fd);
  destroyCommandStream(clean_in -> peer_in);
  Free(clean_in -> peer);
  Pthread_exit(NULL);
}

void makeKey () {
  pthread_key_create(&key, cleanReceiverThread);
}

void *messageReceiverThread (void *vargp) {

  struct peer_info *sender = ((struct peer_info*)vargp);
  struct command_stream_info *sender_info = makeCommandStream(sender -> socket_fd);
  struct clean_up_info *clean_info = malloc(sizeof(*clean_info));
  clean_info -> peer = sender;
  clean_info -> peer_in = sender_info;

  pthread_once(&once_key, makeKey);
  pthread_setspecific(key, clean_info);

  getCommand(sender_info);
  char *command;

  if (commandGetString(&command, sender_info) != 0) {
    printf("Recieved malformed input, closing connection\n");
    return NULL;
  }

  if (strcmp(command, "connect") != 0) {
    printf("Recieved malformed command (%s), closing connection\n", command);
    return NULL;
  }

  char *username = NULL;
  if (commandGetString (&username, sender_info) != 0) {
    printf("Recieved malformed username, closing connection\n");
    return NULL;
  }
  // Using strdup because getcommand deallocates all objects
  username = strdup(username);
  struct peer_node *sender_node = addSender(username, sender);
  if (sender_node == NULL) {
    printf("Received malformed user\n");
    Free(username);
    return NULL;
  }

  while (1) {
    if (getCommand(sender_info) != 0) {
      break;
    }
    if(commandGetString(&command, sender_info) != 0) {
      break;
    }
    if (strcmp(command, "say") != 0) {
      printf("Received malformed command (%s)\n", command);
    }
    char *message;
    if (commandGetString(&message, sender_info) != 0) {
      printf("Received malformed input (%s)\n", message);
    }
    // Using strdup because getcommand deallocates all objects
    message = strdup(message);
    storeMessage(sender_node, message);
  }
}
