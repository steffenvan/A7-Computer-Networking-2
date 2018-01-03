#include "command.h"
#include "message_server.h"
#include <pthread.h>

pthread_t tid;
static pthread_key_t key;
static pthread_once_t once_key = PTHREAD_ONCE_INIT;

struct clean_up_info {
  struct peer_node *user;
  struct peer_info *peer;
  struct command_stream_info *peer_in;
};

struct peer_node {
  char *username;
  struct peer_info *peer;
  struct peer_node *next;
  struct peer_node *prev;
};

struct thread_data {
  struct peer_info *p_info;
  struct peer_node *p_node;
};

struct peer_info;

pthread_mutex_t senderlist_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
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

int flushMessageFrom(struct peer_node *sender, bool show) {
  int j = 0;
  for (int i = 0; i < messageCount; i++) {
    if (sender == NULL || messages[i].sender == sender) {
      if (show) {
        printf("%s: %s\n", messages[i].sender->username, messages[i].message);
      }
      Free(messages[i].message);
    }
    else {
      messages[j] = messages[i];
      j++;
    }
  }
  messageCount = j;
  return 0;
} 

int storeMessage(struct peer_node *sender, char *message) {
  messages[messageCount].message = message;
  messages[messageCount].sender = sender;
  messageCount++;
  return 0;
}

int messageUser(struct peer_node *receiver, char *message) {

  char *buffer = "say ";
  rio_writen(receiver -> peer -> socket_fd, buffer, strlen(buffer));
  rio_writen(receiver -> peer -> socket_fd, message, strlen(message));
  rio_writen(receiver -> peer -> socket_fd, "\n", 1);
  return 0;
}

void deleteSender(struct peer_node *sender) {

  flushMessageFrom(sender, false);

  if (sender == senders) {
    senders -> next = sender;
  }

  if (sender -> prev != NULL) {
    sender -> prev -> next = sender -> next;
  }

  if (sender -> next != NULL) {
    sender -> next -> prev = sender -> prev;
  }
  Free(sender -> username);
  Free(sender);
}

void *receiverThread (void *vargp);
void *messageReceiverThread (void *vargp);

int openServer(char *port) {
  Pthread_create(&tid, NULL, receiverThread, port);
  return 0;
}

void *receiverThread (void *vargp) {
  char *port = (char*) vargp;
  int server_fd = Open_listenfd(port);

  Pthread_detach(pthread_self());

  while(1) {
    struct thread_data *thread = malloc(sizeof(thread));
    struct peer_info *peer = malloc(sizeof(*peer));
    thread -> p_info = peer;
    thread -> p_node = NULL;
    peer -> addr_len = sizeof(peer -> sock_addr);
    peer -> socket_fd = Accept(server_fd, &(peer -> sock_addr), &(peer -> addr_len));
    printf("Received a connection, start typing\n");
    Pthread_create(&tid, NULL, messageReceiverThread, thread);
  }
}

void cleanReceiverThread(void *ptr) {

  struct clean_up_info *clean_in = (struct clean_up_info*)ptr;
  if (clean_in -> user != NULL) {
    deleteSender(clean_in -> user);
  }
  Close(clean_in -> peer -> socket_fd);
  destroyCommandStream(clean_in -> peer_in);
  Free(clean_in -> peer);
  Pthread_exit(NULL);
}

void makeKey () {
  pthread_key_create(&key, cleanReceiverThread);
}

void *messageReceiverThread (void *vargp) {

  struct thread_data *init = (struct thread_data*)vargp;
  struct peer_info *sender = init -> p_info;
  struct peer_node *user = init -> p_node;
  Free(init);
  struct command_stream_info *sender_info = makeCommandStream(sender -> socket_fd);
  struct clean_up_info *clean_info = malloc(sizeof(*clean_info));
  clean_info -> peer = sender;
  clean_info -> peer_in = sender_info;
  clean_info -> user = user;

  pthread_once(&once_key, makeKey);
  pthread_setspecific(key, clean_info);
  char *command;
  if (user == NULL) {
    getCommand(sender_info);

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
    user = addSender(username, sender);
    clean_info -> user = user;
    if (user == NULL) {
      printf("Received malformed user\n");
      Free(username);
      return NULL;
    }
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
    storeMessage(user, message);
  }
  return NULL;
}
