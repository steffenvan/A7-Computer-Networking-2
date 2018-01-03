#include "command.h"
#include "message_server.h"
#include <assert.h>
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

struct peer_info;

pthread_mutex_t senderlist_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
struct peer_node *senders = NULL;

struct peer_node *addSender(char *username, struct peer_info *peer_in) {
  assert(pthread_mutex_lock(&senderlist_mutex) == 0);
  if (lookupUser(username) != NULL) {
    assert(pthread_mutex_unlock(&senderlist_mutex) == 0);
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
  assert(pthread_mutex_unlock(&senderlist_mutex) == 0);
  return sender;
}

struct peer_node *lookupUser(char *username) {
  assert(pthread_mutex_lock(&senderlist_mutex) == 0);
  for (struct peer_node *sender = senders; sender != NULL; sender = sender -> next) {
    if (strcmp(sender -> username, username) == 0) {
      assert(pthread_mutex_unlock(&senderlist_mutex) == 0);
      return sender;
    }
  }
  assert(pthread_mutex_unlock(&senderlist_mutex) == 0);
  return NULL;
}

struct message {
  char *message;
  struct peer_node *sender;
};

#define MAX_MESSAGE_COUNT 1024
struct message messages[MAX_MESSAGE_COUNT];
int messageCount = 0;
pthread_mutex_t messagelist_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

int flushMessageFrom(struct peer_node *sender, bool show) {
  assert(pthread_mutex_lock(&messagelist_mutex) == 0);
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
  assert(pthread_mutex_unlock(&messagelist_mutex) == 0);
  return 0;
} 

int storeMessage(struct peer_node *sender, char *message) {
  assert(pthread_mutex_lock(&messagelist_mutex) == 0);
  if (messageCount >= MAX_MESSAGE_COUNT) {
    printf("Error: message buffer full, cannot receive messages\n");
    return -1;
  }
  messages[messageCount].message = message;
  messages[messageCount].sender = sender;
  messageCount++;
  assert(pthread_mutex_unlock(&messagelist_mutex) == 0);
  return 0;
}

int messageUser(struct peer_node *receiver, char *message) {
  assert(pthread_mutex_lock(&senderlist_mutex) == 0);
  char *buffer = "say ";
  rio_writen(receiver -> peer -> socket_fd, buffer, strlen(buffer));
  rio_writen(receiver -> peer -> socket_fd, message, strlen(message));
  rio_writen(receiver -> peer -> socket_fd, "\n", 1);
  assert(pthread_mutex_unlock(&senderlist_mutex) == 0);
  return 0;
}

void deleteSender(struct peer_node *sender) {
  assert(pthread_mutex_lock(&senderlist_mutex) == 0);
  flushMessageFrom(sender, false);

  if (sender == senders) {
    senders = sender -> next;
  }

  if (sender -> prev != NULL) {
    sender -> prev -> next = sender -> next;
  }

  if (sender -> next != NULL) {
    sender -> next -> prev = sender -> prev;
  }
  Free(sender -> username);
  Free(sender);
  assert(pthread_mutex_unlock(&senderlist_mutex) == 0);
}

void *receiverThread (void *vargp);
void *messageReceiverThread (void *vargp);

int openServer(char *port) {
  Pthread_create(&tid, NULL, receiverThread, port);
  return 0;
}

#define MAX_THREADS 64
pthread_t messageThreads[MAX_THREADS];
int threadCount = 0;
pthread_mutex_t threadlist_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

int server_fd;

void destroyThreads(void *vargp) {
  (void) vargp;
  assert(pthread_mutex_lock(&threadlist_mutex) == 0);
  for (int i = threadCount-1; i >= 0; i--) {
    pthread_cancel(messageThreads[i]);
  }
  assert(pthread_mutex_unlock(&threadlist_mutex) == 0);
  close(server_fd);
}

void *receiverThread (void *vargp) {
  char *port = (char*) vargp;
  server_fd = Open_listenfd(port);

  Pthread_detach(pthread_self());

  pthread_cleanup_push(destroyThreads, NULL);
  while(1) {
    struct thread_data *thread = malloc(sizeof(thread));
    struct peer_info *peer = malloc(sizeof(*peer));
    thread -> p_info = peer;
    thread -> p_node = NULL;
    peer -> addr_len = sizeof(peer -> sock_addr);
    peer -> socket_fd = Accept(server_fd, &(peer -> sock_addr), &(peer -> addr_len));
    printf("Received a connection, start typing\n");
    pthread_t receiver_tid;
    Pthread_create(&receiver_tid, NULL, messageReceiverThread, thread);
  }
  pthread_cleanup_pop(1);
}

void cleanReceiverThread(void *ptr) {
  assert(pthread_mutex_lock(&threadlist_mutex) == 0);
  int myIx = -1;
  for (int i = 0; i < threadCount; i++) {
    if (messageThreads[i] == pthread_self()) {
      myIx = i;
      break;
    }
  }
  if (myIx == -1) {
    printf("ERROR!!!\n");
    exit(-1);
  }
  for (int i = myIx + 1; i < threadCount; i++) {
    messageThreads[i - 1] = messageThreads[i];
  }
  threadCount--;
  assert(pthread_mutex_unlock(&threadlist_mutex) == 0);
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

  assert(pthread_mutex_lock(&threadlist_mutex) == 0);
  messageThreads[threadCount] = pthread_self();
  threadCount++;
  assert(pthread_mutex_unlock(&threadlist_mutex) == 0);
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
