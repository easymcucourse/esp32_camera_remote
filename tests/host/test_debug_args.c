#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "debug_args.h"
int main(void)
{
    char *args[12]; char line[128] = "wifi set ssid \"My Camera AP\" password \"abc def 123\" channel 11";
    assert(debug_split_args(line, args, 12) == 8);
    assert(!strcmp(args[0], "wifi") && !strcmp(args[3], "My Camera AP") && !strcmp(args[5], "abc def 123") && !strcmp(args[7], "11"));
    strcpy(line, "  wifi\tshow  password "); assert(debug_split_args(line, args, 12) == 3);
    strcpy(line, "ssid \"\" password \"a\\\"b\\\\c def\""); assert(debug_split_args(line, args, 12) == 4);
    assert(!strcmp(args[1], "") && !strcmp(args[3], "a\"b\\c def"));
    strcpy(line, "ssid \"unfinished"); assert(debug_split_args(line, args, 12) == -1);
    strcpy(line, "ssid trailing\\"); assert(debug_split_args(line, args, 12) == -1);
    strcpy(line, "1 2 3"); assert(debug_split_args(line, args, 2) == -2);
    strcpy(line, "j"); assert(debug_split_args(line, args, 12) == 1 && !strcmp(args[0], "j"));
    strcpy(line, " "); assert(debug_split_args(line, args, 12) == 0);
    puts("bounded console quoting tests passed");
    strcpy(line, "log '*' info"); assert(debug_split_args(line, args, 12) == 3 && !strcmp(args[1], "*"));
    strcpy(line, "ssid 'My Camera' password \"it's ok\"");
    assert(debug_split_args(line, args, 12) == 4 && !strcmp(args[1], "My Camera") && !strcmp(args[3], "it's ok"));
    strcpy(line, "ssid 'unfinished"); assert(debug_split_args(line, args, 12) == -1);
}
