#!/usr/local/bin/Rscript

library(ggplot2)
library(tools)


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

args <- commandArgs(TRUE)

data <- data.frame(device=character(), trial=character(),latency=numeric())
for (i in args) {
    df <- readThroughputLatency(i)
    data <- rbind(data, df)
}

plot_cdf(data)
plot_box(data)
