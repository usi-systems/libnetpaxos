#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "learner.h"
#include "config.h"

void *deliver(char* value, void *arg) {
    printf("delivered %s\n", value);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("%s config-file node_id\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    Config *conf = parse_conf(argv[1]);
    conf->node_id = atoi(argv[2]);
    start_learner(conf, deliver, conf);
    free(conf);
    return (EXIT_SUCCESS);
}