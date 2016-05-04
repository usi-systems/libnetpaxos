#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

void print_config(Config *conf) {
    printf("Role :%d, second: %d, microsecond: %d, verbose: %d, learner_addr: %s\n",
        conf->role, conf->second, conf->microsecond, conf->verbose, conf->learner_addr);
}

void init_conf(Config *conf) {
    conf->role = 0;
    conf->second = 0;
    conf->microsecond = 0;
    conf->verbose = 0;
    conf->learner_port = 0;
    conf->coordinator_port = 0;
    conf->proposer_port = 0;
    conf->acceptor_port = 0;
    conf->maxinst = 0;
    conf->enable_paxos = 0;
    conf->outstanding = 0;
    conf->paxos_msgtype = 0;
    conf->padsize = 0;
    conf->node_id = 0;
    conf->vlen = 0;
    conf->num_acceptors = 0;
    bzero(conf->learner_addr, 64);
    bzero(conf->proposer_addr, 64);
    bzero(conf->acceptor_addr, 64);
    bzero(conf->coordinator_addr, 64);
}

Config *parse_conf(char *config_file) {
    Config *conf = malloc(sizeof(Config));
    init_conf(conf);
    FILE *fp = fopen(config_file, "r");
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    if (fp == NULL)
        exit(EXIT_FAILURE);

    while ((read = getline(&line, &len, fp)) != -1) {
        char key[20];
        char value[256];
        sscanf(line, "%s %s", key, value);
        if (strcmp(key, "ROLE") == 0) {
            conf->role = atoi(value);
        }
        else if (strcmp(key, "//") == 0) {
            continue;
        }
        else if (strcmp(key, "VERBOSE") == 0) {
            conf->verbose = atoi(value);
        }
        else if (strcmp(key, "SECOND") == 0) {
            conf->second = atoi(value);
        }
        else if (strcmp(key, "MICROSECOND") == 0) {
            conf->microsecond = atoi(value);
        }
        else if (strcmp(key, "LEARNER_ADDR") == 0) {
            strcpy(conf->learner_addr, value);
        }
        else if (strcmp(key, "PROPOSER_ADDR") == 0) {
            strcpy(conf->proposer_addr, value);
        }
        else if (strcmp(key, "COORDINATOR_ADDR") == 0) {
            strcpy(conf->coordinator_addr, value);
        }
        else if (strcmp(key, "ACCEPTOR_ADDR") == 0) {
            strcpy(conf->acceptor_addr, value);
        }
        else if (strcmp(key, "LEARNER_PORT") == 0) {
            conf->learner_port = atoi(value);
        }
        else if (strcmp(key, "PROPOSER_PORT") == 0) {
            conf->proposer_port = atoi(value);
        }
        else if (strcmp(key, "COORDINATOR_PORT") == 0) {
            conf->coordinator_port = atoi(value);
        }
        else if (strcmp(key, "ACCEPTOR_PORT") == 0) {
            conf->acceptor_port = atoi(value);
        }
        else if (strcmp(key, "MAXINST") == 0) {
            conf->maxinst = atoi(value);
        }
        else if (strcmp(key, "ENABLE_PAXOS") == 0) {
            conf->enable_paxos = atoi(value);
        }
        else if (strcmp(key, "OUTSTANDING") == 0) {
            conf->outstanding = atoi(value);
        }
        else if (strcmp(key, "PAXOS_MSGTYPE") == 0) {
            conf->paxos_msgtype = atoi(value);
        }
        else if (strcmp(key, "PADSIZE") == 0) {
            conf->padsize = atoi(value);
        }
        else if (strcmp(key, "VLEN") == 0) {
            conf->vlen = atoi(value);
        }
        else if (strcmp(key, "NUM_ACCEPTORS") == 0) {
            conf->num_acceptors = atoi(value);
        }
    }
    fclose(fp);
    if (line)
        free(line);
    return conf;
}
