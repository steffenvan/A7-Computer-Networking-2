#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

struct command_stream_info;
struct command_stream_info *makeCommandStream(int fd);
int commandGetString(char **out, struct command_stream_info *streaminfo);

int commandGetInt(int *out, struct command_stream_info *streaminfo);
//int commandGetIp(sockaddr **out);
int commandGetIndicator(bool *out, struct command_stream_info *streaminfo);
int getCommand(struct command_stream_info *streaminfo);
bool commandHasNext(struct command_stream_info *streaminfo);
void destroyCommandStream(struct command_stream_info *streaminfo);
