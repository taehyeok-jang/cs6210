#!/usr/bin/env python

from __future__ import print_function
import os, sys
sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
from vm import VMManager
from testLibrary import TestLib

VM_PREFIX="aos"

if __name__ == '__main__':
    manager = VMManager()
    vms = manager.getRunningVMNames(VM_PREFIX)
    rounds = manager.getPhysicalCpus()
    with open("runtest1.txt", "a") as log_file:
        log_file.write(f"vms: {vms}\n")
        log_file.write(f"pcpus: {rounds}\n")

    i=0
    for vmname in vms:
        manager.pinVCpuToPCpu(vmname,0,i)
        i = (i + 1) % rounds
    ips = TestLib.getIps(vms)
    ipsAndVals = { ip : 100000 for ip in ips }
    TestLib.startTestCase("~/cpu/test/testcases/1/iambusy {}",ipsAndVals)
