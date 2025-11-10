#include "ui.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    const char *start = argc > 1 ? argv[1] : ".";
    if (fm_ui_run(start) != 0) {
        fprintf(stderr, "Error running UI\n");
        return 1;
    }
    return 0;
}
