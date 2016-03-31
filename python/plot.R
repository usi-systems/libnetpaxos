#!/usr/local/bin/Rscript

library(ggplot2)
library(tools)
library(reshape2)

# Input file should be named "device-trial.csv"

readLatency <- function(x) {
    print(x)
    df <- read.csv(x, col.names=c('latency'))
    df$latency <- df$latency * 10^6
    fname <- basename(file_path_sans_ext(x))
    title <- strsplit(fname, "-")
    device_name <- title[[1]][1]
    trial_time <-  title[[1]][2]
    df$device <- device_name
    df$trial <- trial_time
    print(head(df))
    return(df)
}

readThroughputLatency <- function(x) {
    print(x)
    df <- read.csv(x, col.names=c('throughput','latency'))
    df <- df[c('latency')]
    df$latency <- df$latency * 10^6  # Microsecond
    fname <- basename(file_path_sans_ext(x))
    title <- strsplit(fname, "-")
    device_name <- title[[1]][1]
    trial_time <-  title[[1]][2]
    df$device <- device_name
    df$trial <- trial_time

    print(head(df))
    return(df)
}

plot_cdf <- function(data) {
    pdf("cumulative.pdf")
    ggplot(data, aes(latency, color=device)) +
    stat_ecdf(geom = "step") +
    ylab("") +
    ggtitle("CDF for latency")
}

plot_box <- function(data) {
    pdf("boxplot.pdf")
    ggplot(data, aes(device, latency)) +
    geom_boxplot() +
    ylab("latency (us)") +
    ggtitle("boxplot latency")
}


readLatencyCycles <- function(x) {
    print(x)
    df <- read.csv(x, col.names=c('endhost', 'forwarding', 'coordinator', 'acceptor'))
    df$endhost <- df$endhost * 10^6  # Microsecond
    df$forwarding <- 20*df$forwarding
    df$coordinator <- 20*df$coordinator
    df$acceptor <- 20*df$acceptor
    return(df)
}

plot_cycles <- function(data) {
    pdf("hardware_latency.pdf")
    ggplot(data, aes(variable, value)) +
    geom_boxplot() +
    xlab("Process") +
    scale_y_continuous("Latency (ns)", breaks=seq(0,1000, 100), minor_breaks=seq(50, 950, 100)) +
    ggtitle("latencies of different processes")
}

plot_end_host_latency <- function(data) {
    pdf("endhost_latency.pdf")
    ggplot(data, aes(variable, value)) +
    geom_boxplot() +
    ylab("latencies (us)") +
    xlab('') +
    ggtitle("latencies of NetPaxos on the end host")
}

args <- commandArgs(TRUE)

# data <- data.frame(device=character(), trial=character(),latency=numeric())
# for (i in args) {
#     df <- readThroughputLatency(i)
#     data <- rbind(data, df)
# }

# plot_cdf(data)
# plot_box(data)

df <- readLatencyCycles(args[1])
mm = melt(df)
print(head(mm))
endhost <- melt ( df[ c('endhost') ] )
plot_end_host_latency(endhost)
# plot_cycles(mm)