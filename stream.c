#include <stdio.h>
#include <stdbool.h> // true, false
#include <stdlib.h> // atoi(), EXIT_*
#include <unistd.h> // getopt(), sleep()
#include <ctype.h> // isprint()
#include <errno.h> // perror()
#include <string.h> // memset()
#include <sys/time.h> // struct timeval, gettimeofday()

#define CHUNK_SIZE 1024

void
usage(void)
{
        printf("usage: stream [options]\n");
        printf("options:\n");
        printf("  -b N\t\tlimit streaming bitrate at N bytes/s (default: no limit)\n");
        printf("  -h\t\tdisplay usage info\n");
        printf("  -q\t\tquiet -- don't display ending stats\n");
        printf("  -s <file>\tsource file (\"-\" for stdin)\n");
}

int
main(int argc, char **argv)
{
        size_t          br, bc, bc_total;
        char            buf[CHUNK_SIZE];
        int             o_bitrate = 0;
        int             o_quiet = false;
        char            *o_sourcefile = NULL;
        FILE            *sourcefile_fd = NULL;
        FILE            *source = NULL;
        long int        timer = 0;
        struct timeval  t_start, t_end, t_diff;
        struct timeval  t_global_start, t_global_end, t_global_diff;

        gettimeofday(&t_global_start, NULL);

        // Don't print the default getopt error messages
        opterr = 0;

        int c;
        while((c = getopt(argc, argv, "b:hqs:")) != -1) {
                switch(c) {
                        case 'b':
                                o_bitrate = atoi(optarg);
                                break;
                        case 'h':
                                usage();
                                exit(EXIT_SUCCESS);
                        case 'q':
                                o_quiet = true;
                                break;
                        case 's':
                                o_sourcefile = strncmp(optarg, "-", 1) == 0 ? NULL : optarg;
                                break;
                        case '?':
                                if (isprint(optopt))
                                        fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                                else
                                        fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                        default:
                                usage();
                                exit(EXIT_FAILURE);
                }
        }

        if (o_sourcefile != NULL)
                if (!(sourcefile_fd = fopen(o_sourcefile, "r"))) {
                        perror("error: can't open source file");
                        exit(EXIT_FAILURE);
                }

        source = o_sourcefile != NULL ? sourcefile_fd : stdin;

        memset(buf, 0, CHUNK_SIZE);

        bc_total = 0;
        while (!feof(source)) {
                if (o_bitrate == 0) {
                        // If no bitrate specified, stream all the data in a row
                        br = fread(buf, 1, CHUNK_SIZE, source);
                        bc_total += fwrite(buf, 1, br, stdout);
                        fflush(stdout);
                } else {
                        // Else, stream as much data as we can at specified bitrate within 1s
                        gettimeofday(&t_start, NULL);

                        bc = 0;
                        timer = 0;
                        while (bc < o_bitrate) {
                                if ((br = fread(buf, 1, CHUNK_SIZE, source)) == 0)
                                        break;

                                bc += fwrite(buf, 1, br, stdout);
                                fflush(stdout);

                                gettimeofday(&t_end, NULL);
                                timersub(&t_end, &t_start, &t_diff);
                                timer = (t_diff.tv_sec * 1000000) + t_diff.tv_usec;

                                if (timer < 1000000)
                                        usleep(1000000 - timer);
                        }

                        bc_total += bc;
                }
        }

        if (sourcefile_fd)
                fclose(sourcefile_fd);

        gettimeofday(&t_global_end, NULL);
        timersub(&t_global_end, &t_global_start, &t_global_diff);

        if (!o_quiet)
                fprintf(stderr, "streamed %ld bytes in %ld.%lds\n", bc_total, t_global_diff.tv_sec, t_global_diff.tv_usec);

        return (EXIT_SUCCESS);
}
