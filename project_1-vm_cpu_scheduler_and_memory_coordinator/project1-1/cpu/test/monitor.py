#!/usr/bin/env python

from __future__ import print_function
import argparse
import os, sys
sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
from vm import VMManager
from testLibrary import TestLib
import sched, time
from libvirt import libvirtError

VM_PREFIX="aos"
ITERATION_THRESHOLD = 25

def print_line(x, iterations):
    print("")
    for i in range(x):
        print('-', end = "")
    print("")
    print("Iteration number : ", iterations)

def which_cpu(vcpuinfo):
    return vcpuinfo[0][0][3]

def which_usage(newinfo, oldinfo):
    return ( newinfo[0][0][2] - oldinfo[0][0][2]) * 1.0 / (10 ** 9)

def run(sc,numpcpu,vmlist,vmobjlist,vminfolist,machineParseable, iterations, test,):
    if iterations == 5:
        print("starting {0}...".format(test))
        os.system('python3 {0}'.format(test))
        
    iterations += 1
    cpulist = {}
    for i in range(numpcpu):
        cpulist[i] = {}
        cpulist[i]['mapping'] = []
        cpulist[i]['usage'] = 0.0

    for i in range(len(vmobjlist)):
        newinfo = vmobjlist[i].vcpus()
        if vminfolist[i]:
            cpu = which_cpu(newinfo)
            usage = which_usage(newinfo, vminfolist[i])
            cpulist[cpu]['mapping'].append(vmlist[i])
            cpulist[cpu]['usage'] += usage
        vminfolist[i] = newinfo
    
    if machineParseable:
        for i in range(numpcpu):
            print('usage,{},{}'.format(i,cpulist[i]['usage'] * 100))
            for mapping in cpulist[i]['mapping']:
                print('mapping,{},{}'.format(i,mapping))
    else:
        print_line(50, iterations) 
        for i in range(numpcpu):
            print('{} - usage: {} | mapping {}'.format(i,cpulist[i]['usage'] * 100,cpulist[i]['mapping']))
    if iterations == ITERATION_THRESHOLD:
        return
    s.enter(1, 1, run, (s,numpcpu,vmlist,vmobjlist,vminfolist,machineParseable,iterations, test))

#This runs the dynamic monitor which can observe vm addition/deletion.
def run_dynamic(sc, manager, vminfoDict, machineParseable, iterations, test):

    if iterations == 5:
        print("starting {0}...".format(test))
        os.system('python3 {0}'.format(test))
    
    iterations += 1
    vmlist = manager.getRunningVMNames(VM_PREFIX)
    vmobjlist = [manager.getVmObject(name) for name in vmlist]   
    numpcpu = manager.getPhysicalCpus() 
    cpulist = {}

    #handle vm addition/deletion
    if len(vmlist) < len(vminfoDict):
        all_keys = vminfoDict.keys()
        del_keys = list(set(all_keys) - set(vmlist))
        for key in del_keys:
            vminfoDict.pop(key)

    for i in range(numpcpu):
        cpulist[i] = {}
        cpulist[i]['mapping'] = []
        cpulist[i]['usage'] = 0.0

    for i in range(len(vmobjlist)):
        try:
            newinfo = vmobjlist[i].vcpus()
        except libvirtError:
            continue
        if vmlist[i] in vminfoDict:
            cpu = which_cpu(newinfo)
            usage = which_usage(newinfo, vminfoDict[vmlist[i]])
            cpulist[cpu]['mapping'].append(vmlist[i])
            cpulist[cpu]['usage'] += usage
        vminfoDict[vmlist[i]] = newinfo
    
    if machineParseable:
        for i in range(numpcpu):
            print('usage,{},{}'.format(i,cpulist[i]['usage'] * 100))
            for mapping in cpulist[i]['mapping']:
                print('mapping,{},{}'.format(i,mapping))
    else:
        print_line(50, iterations) 
        for i in range(numpcpu):
            print('{} - usage: {} | mapping {}'.format(i,cpulist[i]['usage'] * 100,cpulist[i]['mapping']))

    if iterations == ITERATION_THRESHOLD:
        return
    s.enter(1, 1, run_dynamic, (s,manager, vminfoDict, machineParseable,iterations, test))

if __name__ == '__main__':

    parser = argparse.ArgumentParser()
    parser.add_argument("-m","--machine",action="store_true",help="outputs a machine parseable format")
    parser.add_argument("-i", "--iterations", type=int, default=25, help="number of iterations to run")
    parser.add_argument("-d","--dynamic",action="store_true",help="enables dynamic observation of VM addition/deletion")
    parser.add_argument("-t","--test",type=str,help="test case file")
    
    args = parser.parse_args()
    machineParseable = args.machine
    dynamic = args.dynamic
    test = args.test
    ITERATION_THRESHOLD = args.iterations
    
    s = sched.scheduler(time.time, time.sleep)
    manager=VMManager()
    
    iterations = 0
    if dynamic:
        vminfoDict = {}
        s.enter(1, 1, run_dynamic, (s,manager,vminfoDict,machineParseable,iterations,test,))
    else:
        vmlist = manager.getRunningVMNames(VM_PREFIX)
        vmobjlist = [manager.getVmObject(name) for name in vmlist]   
        vminfolist = [None] * len(vmobjlist)
        numpcpu = manager.getPhysicalCpus() 
        s.enter(1, 1, run, (s,numpcpu,vmlist,vmobjlist,vminfolist,machineParseable,iterations,test,))
        
    s.run()
    os.system('python3 killall.py')



