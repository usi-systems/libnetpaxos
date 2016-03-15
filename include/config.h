#ifndef _CONFIG_H
#define _CONFIG_H

typedef struct Config {
    int role;
    int minute;
    int microsecond;
    int verbose;
    char server[64];
} Config;

void print_config(Config *conf);
Config *parse_conf(char *config_file);
#endif