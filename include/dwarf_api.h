#ifndef DWARF_API_H
#define DWARF_API_H

#include "libdw.h"

typedef struct {
    Dwarf *inner;
} dwarf_t;

typedef struct {
    Dwarf_Off off;
    size_t header_sizep;
} cu_t;

typedef struct {
    /* This is the stored offset to interate compile units */
    Dwarf_Off next_off;
    /* User should guarantee the lifetime of the reference dwarf */
    Dwarf *dwarf;
    bool finish;
} dwarf_iter_t;

typedef struct {
    bool start;
    bool finish;
    Dwarf_Die die;
} die_iter_t;

bool dwarf_init(dwarf_t *dwarf, char *file);

#endif
