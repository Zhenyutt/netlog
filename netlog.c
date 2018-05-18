#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/timeb.h>

#ifndef READ_FROM_PROC
#define READ_FROM_PROC 1
#endif

#ifndef READ_FROM_IFCONFIG
#define READ_FROM_IFCONFIG 2
#endif

#ifndef READ_STR_SIZE
#define READ_STR_SIZE 512
#endif

#ifndef NAME_SIZE
#define NAME_SIZE 128
#endif

struct log {
	char name[NAME_SIZE];
	unsigned long long rx_bytes;
    unsigned long long rx_packets;
	unsigned long rx_errors;
    unsigned long rx_dropped;
    unsigned long rx_fifo_errors;
    unsigned long rx_frame_errors;
	unsigned long rx_compressed;
	unsigned long rx_multicast;
    unsigned long long tx_bytes;
    unsigned long long tx_packets;
	unsigned long tx_errors;
    unsigned long tx_dropped;
    unsigned long tx_fifo_errors;
    unsigned long collisions;
	unsigned long tx_carrier_errors;
	unsigned long tx_compressed;
};


int read_proc(char *interface, struct log *netlog) {
	FILE *fp;
	char buffer[READ_STR_SIZE];
	fp = fopen("/proc/net/dev", "r");
	if(fp == NULL) {
		fprintf(stderr, "Error opening file /proc/net/dev\n");
		return -1;
	}
	
	// useless line
	fgets(buffer, sizeof(buffer), fp);
	fgets(buffer, sizeof(buffer), fp);
	
	int read_num;
	int found = 0;
	while(fgets(buffer, sizeof(buffer), fp)) {
		read_num = sscanf(buffer, "%s %llu %llu %lu %lu %lu %lu %lu %lu %llu %llu %lu %lu %lu %lu %lu %lu",
			netlog->name,
			&netlog->rx_bytes,
			&netlog->rx_packets,
			&netlog->rx_errors,
			&netlog->rx_dropped,
			&netlog->rx_fifo_errors,
			&netlog->rx_frame_errors,
			&netlog->rx_compressed,
			&netlog->rx_multicast,
			&netlog->tx_bytes,
			&netlog->tx_packets,
			&netlog->tx_errors,
			&netlog->tx_dropped,
			&netlog->tx_fifo_errors,
			&netlog->collisions,
			&netlog->tx_carrier_errors,
			&netlog->tx_compressed
			);
		if(read_num != 17) {
			fprintf(stderr, "Number of input item error in proc\n");
			return -1;
		}
		
		// replace ':' with '\0'
		netlog->name[strlen(netlog->name) - 1] = '\0';

		if(strcmp(interface, netlog->name) == 0) { // Interface is found
			found = 1;
			break;
		}
	}
	fclose(fp);
	
	if(found == 0) {
		fprintf(stderr, "Interface is not found\n");
		return -1;
	}
	return 0; 
}

int read_ifconfig(char *interface, struct log *netlog) {
	FILE *fp;
	char buffer[READ_STR_SIZE];
	
	fp = popen("ifconfig", "r");
	if(fp == NULL) { // fork or pipe calls fail, or if it cannot allocate memory
		return -1;
	}
	
	int found = 0;
	char *strptr = NULL;
	while(fgets(buffer, sizeof(buffer), fp)) {
		sscanf(buffer, "%s", netlog->name);
		if(strcmp(interface, netlog->name) == 0) { // interface is found
			while(fgets(buffer, sizeof(buffer), fp)) {
				strptr = strstr(buffer, "RX bytes:");
				if(strptr != NULL) {
					sscanf(strptr, "%*[^0-9]%llu", &netlog->rx_bytes);
				}
				strptr = strstr(buffer, "TX bytes:");
				if(strptr != NULL) {
					sscanf(strptr, "%*[^0-9]%llu", &netlog->tx_bytes);
				}
			}
			found = 1;
			break;
		}
		
		// find next interface
		while(fgets(buffer, sizeof(buffer), fp)) { 
			if(buffer[0] == '\n') {
				break;
			}
		}
	}
	pclose(fp);
	
	if(found == 0) {
		fprintf(stderr, "Interface is not found\n");
		return -1;
	}
	return 0;
}

int print_log(int method, char *interface, double interval, long int count){
	if(interface == NULL) {
		fprintf(stderr, "Interface is null\n");
		return -1;
	}

	
	struct log start_log, now_log, last_log;
	struct timeb now, start, last;
	double diff;
	
	
	// print first data
	ftime(&start);
	if(method == READ_FROM_PROC) {
		if(read_proc(interface, &start_log) != 0) {
			return -1;	
		}
	}
	else if(method == READ_FROM_IFCONFIG) {
		if(read_ifconfig(interface, &start_log) != 0) {
			return -1;
		}
	}
	printf("%.24s\t",ctime(&start.time));
	printf("RX bytes: %llu\t", start_log.rx_bytes);
	printf("TX bytes: %llu\n", start_log.tx_bytes);
	
	
	// print other data
	last = start;
	last_log = start_log;
	for(int i = 0; i < count -1; i++) {
		usleep(interval * 1000000); // sleep
		
		ftime(&now);
		if(method == READ_FROM_PROC) {
			if(read_proc(interface, &now_log) != 0) {
				return -1;
			}
		}
		else if(method == READ_FROM_IFCONFIG) {
			if(read_ifconfig(interface, &now_log) != 0) {
				return -1;
			}
		}
		printf("%.24s\t",ctime(&now.time));
		printf("RX bytes: %llu\t", now_log.rx_bytes);
		printf("TX bytes: %llu\t", now_log.tx_bytes);
		diff = now.time - last.time + (now.millitm - last.millitm) / 1000.0;
		printf("Average RX: %.2lf bytes/sec\t", (now_log.rx_bytes - last_log.rx_bytes) / diff);
		printf("Average TX: %.2lf bytes/sec\n", (now_log.tx_bytes - last_log.tx_bytes) / diff);
		last = now;
		last_log = now_log;
	}
	
	// print total average
	if(count > 1) {
		diff = now.time - start.time + (now.millitm - start.millitm) / 1000.0;
		printf("Total average RX: %.2lf bytes/sec\t", (now_log.rx_bytes - start_log.rx_bytes) / diff);
		printf("Total average TX: %.2lf bytes/sec\n", (now_log.tx_bytes - start_log.tx_bytes) / diff);
	}
	
	return 0;
}

int main(int argc, char *argv[]) {
	// initialize
	double interval = 1;
	long int count = 1;
	char *interface = NULL;
	int method = READ_FROM_PROC;
	
	// read option
	int opt;
	char *endptr;
	
	while((opt = getopt(argc, argv, "m:I:i:c:")) != -1) {
		switch(opt) {
			case 'm':
				if(strcmp("proc", optarg) == 0) {
					method = READ_FROM_PROC;
				}
				else if(strcmp("ifconfig", optarg) == 0) {
					method = READ_FROM_IFCONFIG;
				}
				else {
					fprintf(stderr, "Try: -m [proc] | [ifconfig]\n");
				}
				break;
				
			case 'I':
				interface = optarg;
				break;
			
			case 'i':
				errno = 0;
				interval = strtod(optarg, &endptr);
				if(*endptr != '\0' || errno != 0) {
					fprintf(stderr, "-i argument error\n");
				}
				else if(interval < 0.1) {
					fprintf(stderr, "interval >= 0.1\n");
				}
				break;
			
			case 'c':
				errno = 0;
				count = strtol(optarg, &endptr, 10);
				if(*endptr != '\0' || errno != 0) {
					fprintf(stderr, "-c argument error\n");
				}
				else if(count < 1) {
					fprintf(stderr, "count >= 1\n");
				}
				break;
				
			default: /* ? */
				return -1;
				break;
		}
	}
	
	print_log(method, interface, interval, count);
	return 0;
}
