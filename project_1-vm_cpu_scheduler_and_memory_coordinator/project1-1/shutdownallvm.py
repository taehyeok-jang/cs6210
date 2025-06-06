#!/usr/bin/env python

from __future__ import print_function
from vm import VMManager
import time

VM_PREFIX = "aos"

if __name__ == '__main__':
    manager = VMManager() 
    vms=manager.getRunningVMNames(filterPrefix=VM_PREFIX)
    for vmname in vms:
        print(f"shutdown {vmname}...")
        manager.shutdownVM(vmname)
    time.sleep(1)
