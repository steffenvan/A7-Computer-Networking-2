#include "command.h"
#include "peer.h"

struct peer_node;
struct peer_node *lookupUser(char *username);
struct peer_info {
  int socket_fd;
  struct sockaddr sock_addr;
  socklen_t addr_len;
};
struct thread_data {
  struct peer_info *p_info;
  struct peer_node *p_node;
};
struct peer_node *addSender(char *username, struct peer_info *peer_in);

int messageUser(struct peer_node *receiver, char* message);
int openServer(char *port);
int flushMessageFrom(struct peer_node *sender, bool show); 
void *messageReceiverThread(void *pargv);