#ifndef MESSAGE_H
#define MESSAGE_H

void handle_message_mach(struct mach_buffer* buffer);
bool message_loop_begin(char *socket_path);

#endif
