#include "common.h"
#include <stdio.h>
#include <hiredis/hiredis.h>
#include <stdlib.h>
#include <string.h>

redisContext *connect_redis()
{
    redisContext *c = redisConnect("127.0.0.1", 6379);
    if (c == NULL || c->err)
    {
        printf("%sError connecting to Redis: %s\n%s", RED_COLOR, c->errstr, RESET_COLOR);
        redisFree(c);
        exit(1);
    }
    return c;
}

void empty_input_buffer()
{
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF)
        ;
}

void clear()
{
    printf("\033[H\033[J");
}

void press_enter_to_continue()
{
    printf("\nPress Enter to continue...");
    empty_input_buffer();
}
