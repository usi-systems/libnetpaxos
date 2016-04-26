#!/usr/local/bin/Rscript

library(ggplot2)
library(tools)
library(plyr)
library(reshape2)


plot_libpaxos_sys <- function(file) {
    df <- read.csv(file, header = TRUE, sep="")
    max_cpu = max(df$cpu, na.rm = TRUE)
    # scale CPU utilization
    df$cpu <- df$cpu*100/max_cpu
    pdf("cpu_mem_utilization.pdf")
    usage <- ddply(df, "role", summarise, cpu = mean(cpu), mem = mean(mem))
    mm = melt(usage)
    ggplot(mm, aes(x=role, y=value, fill=variable)) +
    scale_fill_grey() +
    theme_bw() +
    geom_bar(stat="identity", position=position_dodge()) +
    ylab('utilization (%)') +
    xlab('') +
    guides(fill=guide_legend(title=NULL)) +
    theme(axis.text = element_text(size = 13)) +
    ggtitle('CPU and Memory utilization of each Paxos role')
}

plot_libpaxos_net <- function(file) {
    df <- read.csv(file, header = TRUE, sep="")
    pdf("network_utilization.pdf")
    usage <- ddply(df, "role", summarise, receive = mean(rx), transmit = mean(tx))
    mm = melt(usage)
    ggplot(mm, aes(x=role, y=value, fill=variable)) +
    scale_fill_grey() +
    theme_bw() + geom_blank() +
    geom_bar(stat="identity", position=position_dodge()) +
    ylab('packets per second (x1000)') +
    xlab('') +
    guides(fill=guide_legend(title=NULL)) +
    theme(axis.text = element_text(size = 13)) +
    ggtitle('Network Receive and Transmit of each Paxos role')
}

plot_all_cpus_with_10_learners <- function(file) {
    df <- read.csv(file, header = TRUE, sep="")
    max_cpu = max(df$cpu, na.rm = TRUE)
    # scale CPU utilization
    df$cpu <- df$cpu*100/max_cpu
    pdf("figures/all_cpus_with_10_learners.pdf")
    usage <- ddply(df, "role", summarise, cpu = mean(cpu))
    mm = melt(usage)
    print(mm)
    ggplot(mm, aes(x=role, y=value, reorder(role, value))) +
    # scale_fill_grey() +
    theme_bw() +
    geom_bar(stat="identity", position=position_dodge()) +
    ylab('utilization (%)') +
    xlab('') +
    scale_x_discrete(limits=c('client', 'coordinator', 'acceptor', 'learner')) + 
    guides(fill=guide_legend(title=NULL)) +
    theme(axis.text = element_text(size = 13)) +
    ggtitle('CPU utilization of each Paxos role')
}

plot_cpus_vs_learners <- function(file) {
    df <- read.csv(file, header = TRUE, sep="")
    max_cpu = max(df$cpu, na.rm = TRUE)
    # scale CPU utilization
    df$cpu <- df$cpu*100/max_cpu
    pdf("figures/cpu_vs_learners.pdf")
    usage <- ddply(df, c("role", "nlearners"), summarise, cpu = mean(cpu))
    print(usage)
    # mm = melt(usage)
    # print(mm)
    ggplot(usage, aes(x=nlearners, y=cpu, group=role)) +
    geom_line(aes(linetype=role), size=1) +
    scale_fill_grey() +
    theme_bw() +
    geom_point(aes(shape=role), size = 3) +
    # geom_bar(stat="identity", position=position_dodge()) +
    ylab('utilization (%)') +
    xlab('number of learners') +
    # scale_x_discrete(limits=c('client', 'coordinator', 'acceptor', 'learner')) + 
    guides(fill=guide_legend(title=NULL)) +
    theme(axis.text = element_text(size = 13)) +
    ggtitle('CPU utilization of each Paxos role')
}

plot_all_cpus_with_10_learners('csv/all_cpus_with_10_learners.csv')
plot_cpus_vs_learners('csv/cpu_vs_learners.csv')

