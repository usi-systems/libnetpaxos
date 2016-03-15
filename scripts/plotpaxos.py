#!/usr/bin/env python

import argparse
import sys
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

def read_input(input_file):
    df = pd.read_csv(input_file, header=0, names=['throughput', 'latency'])
    return df

def plot_scatter(df, output_file):
    ax = df.plot(x='throughput', y='latency', kind='scatter',
                xlim=(0, df['throughput'].max()))
    fig = ax.get_figure()
    fig.savefig(output_file, format='pdf')


def process_input(df):
    xticks = [x*1000 for x in range(13)]
    g1 = df.groupby( pd.cut( df['throughput'], np.arange(0, df['throughput'].max(), 1000) ) )
    ag = g1.aggregate([np.mean, np.std]).reset_index()
    df2 = ag[ np.isfinite( ag['latency']['mean'] ) ]
    pps = pd.Series(df2['throughput']['mean'], name='throughput')
    lat = pd.Series(df2['latency']['mean'], name='latency')
    std = pd.Series(df2['latency']['std'], name='std')
    df = pd.concat([pps, lat, std], axis=1)
    return df


def plot_line(input_file, output_file):
    df = process_input(input_file)
    xmax = df['throughput'].max()*1.05
    ymax = df['latency'].max()*1.2
    ax = df.plot(
                x='throughput', y='latency', yerr=df['std'],
                kind='line', legend=False,
                xlim=(0,xmax), ylim=(0,ymax),
                title='Baseline latency versus packet-per-second',
                )
    ax.set_xlabel("packet per second")
    ax.set_ylabel("latency in microsecond")

    fig = ax.get_figure()
    fig.savefig(output_file, format='pdf')

def combine_dfs(prog, normal):
    prog['type'] = 'Programmable NIC'
    normal['type'] = 'Normal switch'
    df = pd.concat([prog, normal])
    return df

def plot_lines(df, output_file):
    xmax = df['throughput'].max()*1.05
    ymax = df['latency'].max()
    fig, ax = plt.subplots()
    for i, grp in df.groupby('type'):
        grp.plot(x='throughput', y='latency', 
        xlim=(0 ,xmax), ylim=(0,ymax),
        kind='line', label=i, ax=ax)
    # title='Baseline latency versus packet-per-second')
    ax.set_xlabel("packet per second")
    ax.set_ylabel("latency in microsecond")
    fig.savefig(output_file, format='pdf')

def plot_two_lines(input_file, input_file2, output_file):
    df1 = process_input(read_input(input_file))
    df2 = process_input(read_input(input_file2))
    df = combine_dfs(df1, df2)
    plot_lines(df, output_file)

def plot_lost(input_file, output_file):
    df = pd.read_csv(input_file, header=0, names=['rx', 'tx', 'throughput'])
    df['lost'] = (df['tx'] - df['rx']) / df['tx']
    ax = df.plot(x='throughput', y='lost', kind='line',
                xlim=(0, df['throughput'].max()),
                ylim=(0, df['lost'].max()*1.1)
                )
    ax.set_ylabel("percentage of packet loss")
    # manipulate
    vals = ax.get_yticks()
    print vals
    ax.set_yticklabels(['{:3.0f}%'.format(x*100) for x in vals])

    fig = ax.get_figure()
    fig.savefig(output_file, format='pdf')


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Plot NetPaxos experiment.')
    parser.add_argument('-t', '--type', default='line', help='the type of plot')
    parser.add_argument('-i', '--input', required=True, help='data input')
    parser.add_argument('-j', '--input2', help='data input')
    parser.add_argument('-o', '--output', default='figure.pdf', help='figure name')
    args = parser.parse_args()

    if args.type == 'line':
        if args.input2:
            plot_two_lines(args.input, args.input2, args.output)
        else:
            plot_line(args.input, args.output)
    if args.type == 'scatter':
        plot_scatter(args.input, args.output)
    if args.type == 'lost':
        plot_lost(args.input, args.output)