#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <hiredis/hiredis.h>
#include <stdlib.h>
#include <string.h>

#define RESET_COLOR "\033[0m"
#define BOLD "\033[1m"
#define RED_COLOR "\033[31m"
#define GREEN_COLOR "\033[32m"
#define BLUE_COLOR "\033[34m"
#define MAGENTA_COLOR "\033[35m"
#define GRAY_COLOR "\033[90m"
#define YELLOW_COLOR "\033[0;33m"
#define ORANGE_COLOR "\033[38;5;214m"
#define GREY "\033[38;5;245m"

redisContext *connect_redis();

void empty_input_buffer();

void clear();

void press_enter_to_continue();

#endif
