#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "proposer.h"
#include "config.h"

void *result_cb(void *arg) {

}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("%s config node_id\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    Config *conf = parse_conf(argv[1]);
    conf->node_id = atoi(argv[2]);
    start_proposer(conf, result_cb);
    free(conf);
    return (EXIT_SUCCESS);
}