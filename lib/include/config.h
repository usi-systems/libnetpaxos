#ifndef _CONFIG_H
#define _CONFIG_H

typedef struct Config {
    int role;
    int second;
    int microsecond;
    int verbose;
    int learner_port;
    int coordinator_port;
    int proposer_port;
    int acceptor_port;
    int maxinst;
    int enable_paxos;
    int outstanding;
    int paxos_msgtype;
    int padsize;
    int node_id;
    int vlen;
    char learner_addr[64];
    char proposer_addr[64];
    char acceptor_addr[64];
    char coordinator_addr[64];
    int num_acceptors;
} Config;

void print_config(Config *conf);
Config *parse_conf(char *config_file);
#endif