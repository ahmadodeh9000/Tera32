#include "cpu.h"


#include <stdio.h>


/* ---------------- file mode ---------------- */

// Load a flat binary at address 0 and run it until it halts.
static int run_file(const char* path) {
    static CPU c;
    cpu_reset(&c);                       // reset first: it clears memory

    FILE* f = fopen(path, "rb");
    if (!f) { perror(path); return 1; }

    size_t n     = fread(c.mem, 1, MEM_SIZE, f);
    int    extra = fgetc(f);             // anything left means the file doesn't fit
    int    err   = ferror(f);
    fclose(f);

    if (err)          { fprintf(stderr, "%s: read error\n", path); return 1; }
    if (n == 0)       { fprintf(stderr, "%s: empty file\n", path); return 1; }
    if (extra != EOF) { fprintf(stderr, "%s: larger than memory (%d bytes)\n",
                                path, (int)MEM_SIZE); return 1; }

    c.regs[2] = MEM_SIZE;                // sp = top of memory (needed for C programs later)
    cpu_run(&c);
    fflush(stdout);
    return c.exit_code;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s [program.bin]\n", argv[0]);
        return 1;
    }
    if (argc == 2) return run_file(argv[1]);
    
}