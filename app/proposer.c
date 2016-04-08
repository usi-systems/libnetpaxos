#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "learner.h"
#include "proposer.h"
#include "config.h"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("%s config node_id\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    Config *conf = parse_conf(argv[1]);
    conf->node_id = atoi(argv[2]);
    start_proposer(conf);
    free(conf);
    return (EXIT_SUCCESS);
}