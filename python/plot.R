#!/usr/local/bin/Rscript

library(ggplot2)
library(tools)


# Input file should be named "device-trial.csv"

readLatency <- function(x) {
    df <- read.csv(x, col.names=c('latency'))
    df$latency <- df$latency * 10^6
    fname <- basename(file_path_sans_ext(x))
    title <- strsplit(fname, "-")
    device_name <- title[[1]][1]
    trial_time <-  title[[1]][2]
    df$device <- device_name
    df$trial <- trial_time
    return(df)
}

args <- commandArgs(TRUE)

data <- data.frame(device=character(), trial=character(),latency=numeric())
for (i in args) {
    df <- readLatency(i)
    print(summary(df))
    data <- rbind(data, df)
}


pdf("cumulative.pdf")
ggplot(data, aes(latency)) +
stat_ecdf(geom = "step") +
ylab("") +
ggtitle("CDF for latency")
dev.off()

pdf("boxplot.pdf")
ggplot(data, aes(device, latency)) +
geom_boxplot() +
ggtitle("boxplot latency")

dev.off()