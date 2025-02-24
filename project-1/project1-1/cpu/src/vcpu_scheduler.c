#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#define MIN(a, b) ((a) < (b) ? a : b)
#define MAX(a, b) ((a) > (b) ? a : b)

int is_exit = 0; // DO NOT MODIFY THIS VARIABLE

void CPUScheduler(virConnectPtr conn, int interval);

/*
DO NOT CHANGE THE FOLLOWING FUNCTION
*/
void signal_callback_handler()
{
	printf("Caught Signal");
	is_exit = 1;
}

/*
DO NOT CHANGE THE FOLLOWING FUNCTION
*/
int main(int argc, char *argv[])
{
	virConnectPtr conn;

	if (argc != 2)
	{
		printf("Incorrect number of arguments\n");
		return 0;
	}

	// Gets the interval passes as a command line argument and sets it as the STATS_PERIOD for collection of balloon memory statistics of the domains
	int interval = atoi(argv[1]);

	conn = virConnectOpen("qemu:///system"); // https://libvirt.org/html/libvirt-libvirt-host.html#virConnectOpen
	if (conn == NULL)
	{
		fprintf(stderr, "Failed to open connection\n");
		return 1;
	}

	// Get the total number of pCpus in the host
	signal(SIGINT, signal_callback_handler);

	while (!is_exit)
	// Run the CpuScheduler function that checks the CPU Usage and sets the pin at an interval of "interval" seconds
	{
		CPUScheduler(conn, interval);
		sleep(interval);
	}

	// Closing the connection
	virConnectClose(conn);
	return 0;
}

/* COMPLETE THE IMPLEMENTATION */
/**
 * 0. connect to the hypervisor
 * 1. list active virual machines
 * 2. collect vCPU statistics
 * 3. handle vCPU time data 
 * 4. determine vCPU-pCPU mapping 
 * 5. develop own scheduling algorithm
 * 6. update vCPU-pCPU mapping*
 * 7. create a periodic scheduler
 * 8. test
 * 
 * 
 * REF.
 * https://libvirt.org/api.html
 * https://libvirt.org/html/libvirt-libvirt-domain.html
 * https://libvirt.org/html/libvirt-libvirt-host.html
 */
void CPUScheduler(virConnectPtr conn, int interval)
{
	// printf("run CPUScheduler (do nothing)...\n");

	int numOfActiveDomains = virConnectNumOfDomains(conn); // Provides the number of active domains.
	if (numOfActiveDomains < 1) {
        fprintf(stderr, "No active domains found or error.\n");
        return;
    }

	int *activeDomainIds = (int*)calloc(numOfActiveDomains, sizeof(int));
    virConnectListDomains(conn, activeDomainIds, numOfActiveDomains); // Collect the list of active domains, and store their IDs in array ids

	// TODO: do we really need this? 
	// TODO 
	int totalpCPUs = virNodeGetCPUMap(conn, NULL, NULL, 0); // Get CPU map of host node CPUs.
    if (totalpCPUs < 1) {
        fprintf(stderr, "Cannot get host pCPU count.\n");
        free(activeDomainIds);
        return;
    }
	double *pcpuLoad = (double *)calloc(totalPCPUs, sizeof(double));
    for (int i = 0; i < totalPCPUs; i++) {
        pcpuLoad[i] = 0.0;
    }

	for (int d = 0; d < numOfActiveDomains; d++) {
		int domainId = activeDomainIds[d];
		virDomainPtr domain = virDomainLookupByID(conn, domainId); // Try to find a domain based on the hypervisor ID numbe
		if (domain == NULL) {
			fprintf(stderr, "Cannot find domain with ID %d.\n", domainId);
			continue;
		}

		unsigned int maxVcpus = virDomainGetMaxVcpus(domain); // Provides the maximum number of virtual CPUs supported for the guest VM

		



	}
    



	
}