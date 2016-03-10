#!/usr/bin/env python

import argparse
import sys
import pandas as pd
import numpy as np

def plot_scatter(input_file, output_file):
    df = pd.read_csv(input_file, header=None, names=['pps', 'latency'])
    ax = df.plot(x='pps', y='latency', kind='scatter', grid=True,
                xlim=(0, df['pps'].max()))
    fig = ax.get_figure()
    fig.savefig(output_file, format='pdf')


def plot_line(input_file, output_file):
    df = pd.read_csv(input_file, header=None, names=['pps', 'latency'])
    xticks = [x*1000 for x in range(13)]
    df['pps_group'] = pd.cut(df['pps'], bins=xticks, labels=False)
    del df['pps']
    g1 = df.groupby('pps_group')
    ag = g1.aggregate([np.mean, np.std])
    df2 = ag.reset_index()
    xmax = df2['pps_group'].max()*1.05
    ymax = df2['latency']['mean'].max()*1.2
    yerr = df2['latency']['std']
    ax = df2.plot(
                x='pps_group', y=('latency','mean'), yerr=yerr,
                kind='line', grid=True, legend=False,
                xlim=(-0.5 ,xmax), ylim=(0,ymax),
                title='NetPaxos latency versus packet-per-second',
                )
    ax.set_xlabel("packet per second")
    ax.set_ylabel("latency in microsecond")

    fig = ax.get_figure()
    fig.savefig(output_file, format='pdf')

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Plot NetPaxos experiment.')
    parser.add_argument('-t', '--type', default='line', help='the type of plot')
    parser.add_argument('-i', '--input', required=True, help='data input')
    parser.add_argument('-o', '--output', default='figure.pdf', help='figure name')
    args = parser.parse_args()
    if args.type == 'line':
        plot_line(args.input, args.output)
    if args.type == 'scatter':
        plot_scatter(args.input, args.output)
