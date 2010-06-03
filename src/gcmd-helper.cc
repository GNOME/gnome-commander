#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

const char *progname;

void
error(const char *msg, const char *arg="")
{
    fprintf(stderr, "%s: %s%s%s\n", progname, msg, (*arg ? ": " : ""), arg);
    exit(1);
}

void
writestring(const char *s, FILE *f)
{
    size_t size = strlen(s)+1;
    if (fwrite(s, 1, size, f) != size)
        error("Cannot write output file, disk full?");
}

int
main(int argc, char **argv)
{
    progname = argv[0];
    if (argc != 2)
        error("One command line argument expected");

    const char *fname = argv[1];
    FILE *f = fopen(fname, "w");
    if (f==NULL)
        error(fname, strerror(errno));

    const size_t bufsize = 4096;
    char cwd[bufsize];
    if (getcwd(cwd, bufsize)==NULL)
        error("Cannot get current directory");

    writestring(cwd, f);

    for (char **pvar = environ; *pvar!=NULL; pvar++)
        writestring(*pvar, f);

    fclose(f);

    return 0;
}

