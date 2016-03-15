#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "learner.h"
#include "proposer.h"
#include "config.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("%s config-file\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    Config *conf = parse_conf(argv[1]);
    start_learner(conf);
    free(conf);
    return (EXIT_SUCCESS);
}