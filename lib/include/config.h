#ifndef _CONFIG_H
#define _CONFIG_H

typedef struct Config {
    int role;
    int second;
    int microsecond;
    int verbose;
    int learner_port;
    int proposer_port;
    int maxinst;
    int enable_paxos;
    int outstanding;
    int paxos_msgtype;
    int padsize;
    int node_id;
    char server[64];
} Config;

void print_config(Config *conf);
Config *parse_conf(char *config_file);
#endif