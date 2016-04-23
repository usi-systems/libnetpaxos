#!/usr/bin/env python
import argparse
import subprocess
import shlex
import os
from threading import Timer
import time

def run(user, host, command):
    cmd = "ssh {0}@{1} {2}".format(user, host, command)
    with open("%s.txt" % host, "a+") as out:
        ssh = subprocess.Popen(shlex.split(cmd),
                stdout=out,
                stderr=out,
                shell=False)
    return ssh

def run_sudo(user, host, command):
    cmd = "ssh -t {0}@{1} 'sudo {2}'".format(user, host, command)
    print cmd
    ssh = subprocess.Popen(shlex.split(cmd), shell=False)
    return ssh

def run_ps(user, host):
    command = 'ps -C udp_client,acceptor,coordinator,leveldb_paxos \
                                  -o pcpu,pmem,comm --sort pcpu | tail -n1'
    cmd = "ssh {0}@{1} {2}".format(user, host, command)
    with open("%s-cpu.txt" % host, "a+") as out:
        ssh = subprocess.Popen(shlex.split(cmd),
                stdout=out,
                stderr=out,
                shell=False)
    return ssh

def run_bmon(user, host, t):
    command = 'bmon -p enp4s0 -o ascii:quitafter=%d' % t
    cmd = "ssh {0}@{1} {2}".format(user, host, command)
    with open("%s-net.txt" % host, "a+") as out:
        ssh = subprocess.Popen(shlex.split(cmd),
                stdout=out,
                stderr=out,
                shell=False)
    return ssh

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Run NetPaxos experiment.')
    parser.add_argument('--time', type=int, default=20, help='number of seconds to run')
    parser.add_argument('--verbose', default=False, action='store_true', help='verbose flag')
    parser.add_argument('--output', default='output', help='output folder')
    parser.add_argument('--user', default='danghu', help='ssh user')
    args = parser.parse_args()

    #if not os.path.exists(args.output):
    #    os.makedirs(args.output)

    #print "kill replicas and client after %d seconds" % args.time
    #t1 = Timer(args.time, kill_all_servers, learners, parm)
    #t1.start()

    nodes = [ 'node21', 'node22', 'node23', 'node24', 'node25', 'node26']
    for n in nodes:
        run_bmon(args.user, n, args.time)

    for i in range(args.time):
        for n in nodes:
            run_ps(args.user, n)

        time.sleep(1)
    #pipes = []
    #for p in pipes:
    #    p.wait()
