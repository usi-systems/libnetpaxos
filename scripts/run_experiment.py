#!/usr/bin/env python
import argparse
import subprocess
import shlex
import os
from threading import Timer
import time
import pandas as pd
import numpy as np
import plotpaxos

def start_proposer(user, host, path, config, output, proid):
    cmd = "ssh {0}@{1} {2}/proposer {3}".format(user, host, path, config)
    print cmd
    with open("%s/%s-%d.log" % (output, "proposer", proid), "w+") as out:
        ssh = subprocess.Popen(shlex.split(cmd),
                                stdout = out,
                                stderr = out,
                                shell=False)
    return ssh

def start_learner(user, host, path, config, output):
    cmd = "ssh {0}@{1} {2}/learner {3}".format(user, host, path, config)
    print cmd
    with open("%s/%s.log" % (output, "learner"), "a+") as out:
        ssh = subprocess.Popen(shlex.split(cmd),
                                stdout = out,
                                stderr = out,
                                shell=False)
    return ssh


def kill_all_clients(*nodes, **parm):
    for h in nodes:
        cmd = "ssh {0}@{1} pkill proposer".format(parm['user'], h)
        ssh = subprocess.Popen(shlex.split(cmd))
        ssh.wait()

def kill_all_servers(*nodes, **parm):
    for h in nodes:
        cmd = "ssh {0}@{1} pkill learner".format(parm['user'], h)
        ssh = subprocess.Popen(shlex.split(cmd))
        ssh.wait()


def copy_data(output, user, nodes):
    for h in nodes:
        cmd = "scp {0}@{1}:*.txt {2}/".format(user, h, output)
        print cmd
        ssh = subprocess.Popen(shlex.split(cmd))
        ssh.wait()

def delete_data(user, nodes):
    for h in nodes:
        cmd = "ssh {0}@{1} rm -f *.txt".format(user, h)
        print cmd
        ssh = subprocess.Popen(shlex.split(cmd))
        ssh.wait()


def check_result(output):
    cmd = "diff {0}/learner.txt {0}/proposer.txt".format(output)
    print cmd
    p = subprocess.Popen(shlex.split(cmd),
                        stdout = subprocess.PIPE,
                        stderr = subprocess.PIPE,
                        shell=False)
    out, err = p.communicate()
    if out:
        print out
    if err:
        print err


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Run NetPaxos experiment.')
    parser.add_argument('--time', type=int, default=10, help='amout of time in second to run example')
    parser.add_argument('--speed', type=int, default=10000, help='duration between two proposals')
    parser.add_argument('--multi', type=int, default=2, help='number of client')
    parser.add_argument('--verbose', default=False, action='store_true', help='verbose flag')
    parser.add_argument('--path', default='/libperf/build', help='path to programs')
    parser.add_argument('--user', default='vagrant', help='login name of ssh')
    parser.add_argument('--learner', default='sdn-vm', help='learner hostname')
    parser.add_argument('--proposer', default='sdn-vm', help='proposer hostname')
    parser.add_argument('--server', default='localhost', help='server hostname')
    parser.add_argument('--config', required=True, help='config file')
    parser.add_argument('--output', default='output', help='output folder')
    args = parser.parse_args()

    if not os.path.exists(args.output):
        os.makedirs(args.output)
    learners = [ args.learner ]
    parm = {'user': args.user}

    clients = [ args.proposer ]
    parm = {'user': args.user}

    print "kill replicas and client after %d seconds" % args.time
    t1 = Timer(args.time, kill_all_servers, learners, parm)
    t1.start()
    t2 = Timer(args.time, kill_all_clients, clients, parm)
    t2.start()

    pipes = []
    pipes.append(start_learner(args.user, args.learner, args.path, args.config, args.output))
    for i in range(args.multi):
        pipes.append(start_proposer(args.user, args.proposer, args.path, args.config, args.output, i))
   
    for p in pipes:
        p.wait()

    print "copy data"
    nodes = [ args.learner, args.proposer ]
    copy_data(args.output, args.user, nodes)
    delete_data(args.user, nodes)
    # check_result(args.output)
    # plotpaxos.plot_line("%s/learner.log" % args.output, "%s/figure.pdf" % args.output)
