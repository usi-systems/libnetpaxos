#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "coordinator.h"
#include "config.h"


int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("%s config\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    Config *conf = parse_conf(argv[1]);
    start_coordinator(conf);
    free(conf);
    return (EXIT_SUCCESS);
}