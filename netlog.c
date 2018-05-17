#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include <sys/timeb.h>

struct log {
	char rx_bytes[30];
	char tx_bytes[30];
};

int my_atoi(char *str) {
	int num = 0;
	int i = 0;
	while(str[i++] != '\0') {
		if(isdigit(str[i-1]) == 0) {
			return 0; // not digit
		}
		num = num * 10 + str[i-1] - '0';
	}
	return num;
}

double my_atof(char *str) {
	double num = 0;
	int point = -1;
	int i = 0;
	while(str[i] != '\0' && str[i] != '.') {
		if(isdigit(str[i]) == 0) {
			return 0; // not digit
		}
		num = num * 10 + str[i] - '0';
		i++;
	}
	if(str[i++] == '.') {
		while(str[i] != '\0') {
			if(isdigit(str[i]) == 0) {
				return 0; // not digit
			}
			num = num + pow(10, point) * (str[i] - '0');
			point--;
			i++;
		}
	}
	return num;
}

unsigned long long my_atoull(char *str) {
	unsigned long long num = 0;
	int i = 0;
	while(str[i++] != '\0') {
		if(isdigit(str[i-1]) == 0) {
			return 0; // not digit
		}
		num = num * 10 + str[i-1] - '0';
	}
	return num;
}

int check_proc_interface(char **interface) {
	// check interface is not NULL
	if(*interface == NULL) {
		puts("Interface is null");
		return -1;
	}
	
	FILE *fp;
	char command[100];
	char buffer[100];
	int len;
	int found = 0;
	
	// check whether interface exists
	len = snprintf(command, sizeof(command), "cat /proc/net/dev | sed '1,2d' | awk '{print $1}'");
	if(len >= sizeof(command)) { // overflow
		return -1;
	}
	fp = popen(command, "r");
	if(fp == NULL) { // fork or pipe calls fail, or if it cannot allocate memory
		return -1;
	}
	while(fgets(buffer, sizeof(buffer), fp)) {
		buffer[strlen(buffer) - 2] = '\0'; // replace ':' with '\0'
		if(strcmp(*interface, buffer) == 0) { // compare interface name
			found = 1;
			break;
		}
	}
	if(found == 0) { // interface is not found
		puts("Interface is not found");
		return -1; 
	}
	pclose(fp);
	return 0;
}

int read_proc(char **interface, struct log *mylog) {
	if(check_proc_interface(interface) != 0) { // check whether interface exists
		return -1;
	}
	
	FILE *fp;
	char command[100];
	char buffer[100];
	int len;
	
	// read rx and tx
	len = snprintf(command, sizeof(command), "cat /proc/net/dev | grep '%s' | awk '{print $2 \"\\0\" $10}'", *interface);
	if(len >= sizeof(command)) { // overflow
		return -1;
	}
	
	fp = popen(command, "r");
	if(fp == NULL) { // fork or pipe calls fail, or if it cannot allocate memory
		return -1;
	}
	
	// save value into struct
	fgets(buffer, sizeof(buffer),fp);
	strcpy(mylog->rx_bytes, buffer);
	fgets(buffer, sizeof(buffer),fp);
	strcpy(mylog->tx_bytes, buffer);
	pclose(fp);
	return 0;
}

int check_ifconfig_interface(char **interface) {
	// check interface is not NULL
	if(*interface == NULL) {
		puts("Interface is null");
		return -1;
	}
	
	FILE *fp;
	char command[100];
	char buffer[100];
	int len;
	
	// check whether interface exists
	len = snprintf(command, sizeof(command), "ifconfig %s", *interface);
	if(len >= sizeof(command)) { // overflow
		return -1;
	}
	fp = popen(command, "r");
	if(fp == NULL) { // fork or pipe calls fail, or if it cannot allocate memory
		return -1;
	}
	if(fgets(buffer, sizeof(buffer), fp) == NULL) {
		puts("Interface is not found");
		return -1; 
	}
	pclose(fp);
	return 0;
}

int read_ifconfig(char **interface, struct log *mylog) {
	if(check_ifconfig_interface(interface) != 0) { // check whether interface exists
		return -1;
	}
	
	FILE *fp;
	char command[100];
	char buffer[100];
	int len;
	
	// read rx and tx
	len = snprintf(command, sizeof(command), "ifconfig %s | grep 'RX bytes' | awk '{print $2 \"\\0\" $6}'", *interface);
	if(len >= sizeof(command)) { // overflow
		return -1;
	}
	
	fp = popen(command, "r");
	if(fp == NULL) { // fork or pipe calls fail, or if it cannot allocate memory
		return -1;
	}
	
	// save value into struct
	fgets(buffer, sizeof(buffer),fp);
	strcpy(mylog->rx_bytes, &buffer[6]);
	fgets(buffer, sizeof(buffer),fp);
	strcpy(mylog->tx_bytes, &buffer[6]);
	pclose(fp);
	return 0;
}

int print_log(int m, char **interface, double interval, int count){
	struct log start_log, now_log, last_log;
	struct timeb now, start, last;
	double diff; // compute time diff
	ftime(&start); // start time

	// print first data
	if(m == 1) {
		if(read_proc(interface, &start_log) != 0) { // read rx and tx from proc
			return -1;
		}
	}
	else if(m == 2) {
		if(read_ifconfig(interface, &start_log) != 0) { // read rx and tx from ifconfig
			return -1;
		}
	}
	else {
		return -1;
	}
	
	printf("%.24s\t",ctime(&start.time));
	printf("RX bytes: %s\t", start_log.rx_bytes);
	printf("TX bytes: %s\n", start_log.tx_bytes);
	
	
	// print other data
	last = start;
	last_log = start_log;
	for(int i = 0; i < count -1; i++) {
		usleep(interval * 1000000); // sleep
		if(m == 1) { // read rx and tx from proc
			if(read_proc(interface, &now_log) != 0) {
				return -1;
			}
		}
		else if(m == 2) { // read rx and tx from ifconfig
			if(read_ifconfig(interface, &now_log) != 0) {
				return -1;
			}
		}
		ftime(&now);
		printf("%.24s\t",ctime(&now.time));
		printf("RX bytes: %s\t", now_log.rx_bytes);
		printf("TX bytes: %s\t", now_log.tx_bytes);
		diff = now.time - last.time + (now.millitm - last.millitm) / 1000.0;
		printf("Average: %.2lf bytes/sec\n", (my_atoull(now_log.rx_bytes) - my_atoull(last_log.rx_bytes)) / diff);
		last = now;
		last_log = now_log;
	}
	
	// print total average
	if(count > 1) {
		diff = now.time - start.time + (now.millitm - start.millitm) / 1000.0;
		printf("Total average RX: %.2lf bytes/sec\t", (my_atoull(now_log.rx_bytes) - my_atoull(start_log.rx_bytes)) / diff);
		printf("Total average TX: %.2lf bytes/sec\n", (my_atoull(now_log.rx_bytes) - my_atoull(start_log.rx_bytes)) / diff);
	}
	
	return 0;
}

int main(int argc, char *argv[]) {
	// initialize
	double interval = 1;
	int count = 1;
	char *interface = NULL;
	int m = 1;
	
	if(argc % 2 == 0) {
		printf("missing argument\n");
		return -1;
	}
	
	for(int i = 1; i < argc; i += 2) {
		if(strcmp("-m", argv[i]) == 0) { // read from proc or ifconfig
			if(strcmp("proc", argv[i+1]) == 0) {
				m = 1;
			}
			else if(strcmp("ifconfig", argv[i+1]) == 0) {
				m = 2;
			}
			else {
				printf("unrecongnized argument\ntry: -m [proc | ifconfig]\n");
				return -1;
			}
		}
		else if(strcmp("-I", argv[i]) == 0) { // Interface
			interface = argv[i+1];
		}
		else if(strcmp("-i", argv[i]) == 0) { // interval
			interval = my_atof(argv[i+1]);
			if(interval < 0.1) {
				puts("argument -i error");
				return -1;
			}
		}
		else if(strcmp("-c", argv[i]) == 0) { // count
			count = my_atoi(argv[i+1]);
			if(count <= 0) {
				puts("argument -c error");
				return -1;
			}
		}
		else {
			printf("unrecongnized argument '%s'\n", argv[i]);
			return -1;
		}
	}

	
	print_log(m, &interface, interval, count);
	return 0;
}
