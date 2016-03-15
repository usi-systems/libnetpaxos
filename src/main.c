#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "learner.h"
#include "proposer.h"
#include "config.h"

int main(int argc, char* argv[]) {

    Config *conf = parse_conf(argv[1]);

    if (conf->role == 1) {
        // start proposer
        start_proposer(conf);
    } else if (conf->role == 2) {
        // start learner
        start_learner(conf);
    } else {
        printf("Role was not set.\n");
    }

    free(conf);
    return (EXIT_SUCCESS);
}