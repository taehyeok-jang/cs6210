#!/usr/bin/env python

from __future__ import print_function
from vm import VMManager
import time

VM_PREFIX = "aos"

if __name__ == '__main__':
    manager = VMManager()
    vms = manager.getFilteredVms(VM_PREFIX)
    for vmname in vms:
        print(f"strating {vmname}...")
        manager.startVM(vmname)
    time.sleep(1)
