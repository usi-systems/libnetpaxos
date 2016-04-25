#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "acceptor.h"
#include "config.h"


int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("%s config id\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int acceptor_id = atoi(argv[2]);
    Config *conf = parse_conf(argv[1]);
    start_acceptor(conf, acceptor_id);
    free(conf);
    return (EXIT_SUCCESS);
}