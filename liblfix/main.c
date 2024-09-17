#include <assert.h>
#include <errno.h>

#include "lfix.h"

int
main(int argc, const char** argv)
{
    LFIXEngine engine;
    bool b = lfix_init(&engine);
    if (!b) {
        fprintf(stderr, "error initializing: %s\n", lfi_strerror());
        return 1;
    }

    if (argc <= 1) {
        fprintf(stderr, "no input file\n");
        return 1;
    }

    FILE* f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "error opening %s: %s\n", argv[1], strerror(errno));
        return 1;
    }

    LFIXProc* p = lfix_proc_newfile(&engine, fileno(f), argc - 1, &argv[1]);
    if (!p) {
        fprintf(stderr, "error creating process: %s\n", lfi_strerror());
        return 1;
    }

    fclose(f);

    uint64_t r = lfi_proc_start(p->l_proc);
    printf("exited: %ld\n", r);

    return 0;
}
