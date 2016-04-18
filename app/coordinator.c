#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "coordinator.h"
#include "config.h"


int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("%s config port\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int port = atoi(argv[2]);
    Config *conf = parse_conf(argv[1]);
    start_coordinator(conf, port);
    free(conf);
    return (EXIT_SUCCESS);
}