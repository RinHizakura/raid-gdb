#include "gdb.h"
#define CMD "./bin/hello"

int main()
{
    gdb_t gdb;
    gdb_init(&gdb, CMD);
    return 0;
}
