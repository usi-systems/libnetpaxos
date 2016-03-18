#ifndef _CONFIG_H
#define _CONFIG_H

typedef struct Config {
    int role;
    int second;
    int microsecond;
    int verbose;
    int learner_port;
    int bufsize;
    int maxinst;
    int enable_paxos;
    int outstanding;
    char server[64];
} Config;

void print_config(Config *conf);
Config *parse_conf(char *config_file);
#endif