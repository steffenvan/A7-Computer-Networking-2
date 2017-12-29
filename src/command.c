#include "command.h"
#include "csapp.h"
#include <string.h>
#include <errno.h>

void ignoreWhiteSpace(struct command_stream_info *streaminfo);
bool isWhiteSpace(char c);
void deallocateObjects(struct command_stream_info *streaminfo);


struct command_stream_info {
  rio_t rp;
  int allocatedCount;
  int commandChar;
  char *currentCommand;
  void *allocatedObject[10];
};

// enables the server to use this interface to read from the socket.

struct command_stream_info *makeCommandStream (int fd) {
  struct command_stream_info *streaminfo = malloc(sizeof(*streaminfo));
  Rio_readinitb(&(streaminfo -> rp), fd);
  streaminfo -> currentCommand = malloc(RIO_BUFSIZE);
  streaminfo -> allocatedCount = 0;
  return streaminfo;
}

// Function that reads the message from the socket.
int getCommand(struct command_stream_info *streaminfo) {

  if (rio_readlineb(&(streaminfo -> rp), streaminfo -> currentCommand, RIO_BUFSIZE) <= 0) {
    return -1;
  }
  printf("Input is: %s\n", streaminfo -> currentCommand);

  // Resetting the struct command_stream_info
  streaminfo -> commandChar = 0;
  deallocateObjects(streaminfo);
  ignoreWhiteSpace(streaminfo);
  return 0;
}

// Function that indicates whether the input from the user is a command.
int commandGetIndicator(bool *out, struct command_stream_info *streaminfo) {
  if (streaminfo -> currentCommand[streaminfo -> commandChar] == '/') {
    streaminfo -> commandChar++;
    ignoreWhiteSpace(streaminfo);
    *out = true;
    return 0;
  }
  else {
    *out = false;
    return 0;
  }
}


int commandGetString(char **out, struct command_stream_info *streaminfo) {
  int count = 0;
  while (!isWhiteSpace(streaminfo -> currentCommand[streaminfo -> commandChar + count]) && streaminfo -> currentCommand[streaminfo -> commandChar + count] != '\0') {
    count++;
  }
  if (count == 0) {
    return -1;
  }
  *out = malloc(count + 1);
  for (int i = 0; i < count; i++) {
    (*out)[i] = streaminfo -> currentCommand[streaminfo -> commandChar];
    streaminfo -> commandChar++;
  }
  (*out)[count] = '\0';
  streaminfo -> allocatedObject[streaminfo -> allocatedCount] = *out;
  streaminfo -> allocatedCount++;
  ignoreWhiteSpace(streaminfo);
  return 0;
}

int commandGetInt(int *out, struct command_stream_info *streaminfo) {
  int current = 0;
  if (!commandHasNext(streaminfo)) {
    return -1;
  }
  while (!isWhiteSpace(streaminfo -> currentCommand[streaminfo -> commandChar]) && streaminfo -> currentCommand[streaminfo -> commandChar] != '\0') {
    if (streaminfo -> currentCommand[streaminfo -> commandChar] < '0' || streaminfo -> currentCommand[streaminfo -> commandChar] > '9') {
      return -1;
    }
    int dig = streaminfo -> currentCommand[streaminfo -> commandChar] - '0';
    current = current*10 + dig;
    streaminfo -> commandChar++;
  }
  *out = current;
  ignoreWhiteSpace(streaminfo);
  return 0;
}
void deallocateObjects(struct command_stream_info *streaminfo) {
  for (int i = 0; i < streaminfo -> allocatedCount; i++) {
    free(streaminfo -> allocatedObject[i]);
  }
  streaminfo -> allocatedCount = 0;
}

void destroyCommandStream (struct command_stream_info *streaminfo) {
  deallocateObjects(streaminfo);
  free(streaminfo -> currentCommand);
  free(streaminfo);
}

bool commandHasNext (struct command_stream_info *streaminfo) {
  return streaminfo -> currentCommand[streaminfo -> commandChar] != '\0';
}

bool isWhiteSpace(char c) {
  return c == ' ' || c == '\n' || c == '\r'||c == '\t';
}

void ignoreWhiteSpace(struct command_stream_info *streaminfo) {
  while(isWhiteSpace(streaminfo -> currentCommand[streaminfo -> commandChar])) {
    streaminfo -> commandChar++;
  }
}
