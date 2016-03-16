
library(ggplot2)

args <- commandArgs(TRUE)
df <- read.csv(args[1], col.names=c('latency'))
pdf("cumulative.pdf")
ggplot(df, aes(latency)) + stat_ecdf(geom = "step")

df$typ = 'normal'
pdf("boxplot.pdf")
ggplot(df, aes(factor(typ), latency)) + geom_boxplot()