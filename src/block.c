#include <stdio.h>

int main (int argc, char **argv)
{
    FILE *fd = fdopen (0, "r");

    printf ("--------------------------------------------------------------\n");
    printf ("The program has now finished, press Enter to close this window\n");

    getc(fd);
    fclose(fd);
    return 0;
}
