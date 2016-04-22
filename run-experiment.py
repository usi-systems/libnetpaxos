#!/usr/bin/env python
import argparse
import subprocess
import shlex
import os
from threading import Timer
import time

def acceptor(host, path, config, node_id):
    cmd = "ssh {0} {1}/acceptor {2} {3}".format(host, path, config, node_id)
    print cmd
    ssh = subprocess.Popen(shlex.split(cmd),
                                stdout = subprocess.PIPE,
                                stderr = subprocess.PIPE,
                                shell=False)
    return ssh

def coordinator(host, path, config):
    cmd = "ssh {0} {1}/coordinator {2}".format(host, path, config)
    print cmd
    ssh = subprocess.Popen(shlex.split(cmd),
                                stdout = subprocess.PIPE,
                                stderr = subprocess.PIPE,
                                shell=False)
    return ssh

def leveldb(host, path, config, node_id):
    cmd = "ssh {0} {1}/leveldb_paxos {2} {3}".format(host, path, config, node_id)
    print cmd
    with open("output-leveldb.txt", "w+") as out:
        ssh = subprocess.Popen(shlex.split(cmd),
                                stdout = out,
                                stderr = out,
                                shell=False)
    return ssh

def client(host, path, config, output):
    cmd = "ssh {0} {1}/udp_client {2} {3}".format(host, path, config, output)
    print cmd
    ssh = subprocess.Popen(shlex.split(cmd),
                                stdout = subprocess.PIPE,
                                stderr = subprocess.PIPE,
                                shell=False)
    return ssh

def kill_all(*nodes):
    for idx in range(3):
        print nodes[idx]
        cmd = "ssh %s pkill acceptor" % nodes[idx]
        print cmd
        ssh = subprocess.Popen(shlex.split(cmd))
        ssh.wait()

    cmd = "ssh %s pkill coordinator" % nodes[3]
    print cmd
    ssh = subprocess.Popen(shlex.split(cmd))
    ssh.wait()

    cmd = "ssh %s pkill leveldb_paxos" % nodes[4]
    print cmd
    ssh = subprocess.Popen(shlex.split(cmd))
    ssh.wait()

    cmd = "ssh %s pkill udp_client" % nodes[5]
    print cmd
    ssh = subprocess.Popen(shlex.split(cmd))
    ssh.wait()


def stop_all(nodes):
    for idx in range(3):
        print nodes[idx]
        cmd = "ssh %s pkill acceptor" % nodes[idx]
        print cmd
        ssh = subprocess.Popen(shlex.split(cmd))
        ssh.wait()

    cmd = "ssh %s pkill coordinator" % nodes[3]
    print cmd
    ssh = subprocess.Popen(shlex.split(cmd))
    ssh.wait()

    cmd = "ssh %s pkill leveldb_paxos" % nodes[4]
    print cmd
    ssh = subprocess.Popen(shlex.split(cmd))
    ssh.wait()

    cmd = "ssh %s pkill client" % nodes[5]
    print cmd
    ssh = subprocess.Popen(shlex.split(cmd))
    ssh.wait()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Run NetPaxos experiment.')
    parser.add_argument('config', help='config file')
    parser.add_argument('--time', type=int, default=20, help='amout of time in second to run example')
    parser.add_argument('--verbose', default=False, action='store_true', help='verbose flag')
    parser.add_argument('--kill', default=False, action='store_true', help='kill flag')
    parser.add_argument('--path', default='./build',  help='path to programs')
    parser.add_argument('--client', type=int, default=10, help='amout of clients')
    args = parser.parse_args()

    args.config = os.path.realpath(args.config)

    args.path = os.path.abspath(args.path)
    pipes = []

    nodes = [ "node21", "node22", "node23", "node24", "node25", "node26" ]

    if args.kill:
        stop_all(nodes)

    # start acceptor
    for idx in range(0, 3):
        pipes.append( acceptor(nodes[idx], args.path, args.config, idx) )

    # start coordinator
    pipes.append( coordinator(nodes[3], args.path, args.config) )

    # start leveldb
    pipes.append( leveldb(nodes[4], args.path, args.config, idx) )
    time.sleep(1)
    # start client
    for idx in range(args.client):
        pipes.append( client( nodes[5], args.path, args.config, '%s/client-%d.csv' % (args.path, idx) ) )
    
    # kill replicas and client after (args.time) seconds
    t= Timer(args.time, kill_all, nodes)
    t.start()
   
    for p in pipes:
        p.wait()

