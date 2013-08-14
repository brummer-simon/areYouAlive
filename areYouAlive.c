/*
 ============================================================================
 Name        : areYouAlive.c
 Author      : Simon Brummer
 Version     : 0.1
 Description : Simple Programm to test if an Network node is alive.
 ============================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/time.h>

#define BUFSIZE sizeof(MSG)
#define IPSIZE  30
#define NOTSILENT( ... ) if(!silent){ printf(__VA_ARGS__); }


/* "Constants" */
char  MSG[] = "Dude are you still alive?\n";
char  RPL[] = "Jeah Dude, still kickin!\n";


/* Default Values */
enum  MODE{SERVER, CLIENT} mode = CLIENT;
char* node = "::";
char* port = "10000";
int   silent  = 0;
int   timeout = 1000;


/* Prototyes */
int client(int sockfd);
int server(int sockfd);
int parseArgs(int argc, char** argv);
int usage(void);
int ipaddrToStr(char* dst, unsigned int dst_size, struct sockaddr_storage *addr );
int die(char* error_msg);


/* Main Function. Trys to resolv supplied Network Node and starts specified Mode.*/
int main(int argc, char** argv){
	int sfd = -1, err = -1;
	parseArgs(argc, argv);

	/* Setup Socket*/
	struct addrinfo hints, *pRes, *ptr;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family   = AF_UNSPEC;  // Version Agnostic
	hints.ai_socktype = SOCK_DGRAM; // Using UDP
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = AI_PASSIVE;

	if( getaddrinfo(node, port, &hints, &pRes) != 0){
		die("Can't Resolv specified Hostname\n");
	}

	for( ptr = pRes; ptr != NULL; ptr = ptr->ai_next){
		if( (sfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol)) == -1 ){
			continue;
		}

		switch(mode){
		case SERVER: if( bind(sfd, ptr->ai_addr, ptr->ai_addrlen) == 0 )   goto cleanup; else break;
		case CLIENT: if( connect(sfd, ptr->ai_addr, ptr->ai_addrlen) == 0) goto cleanup; else break;
		}

		close(sfd);
	}

	/* Clean Resolver Structures */
	cleanup:
	if( ptr == NULL){
		die("Can't Establish Connection\n");
	}
	freeaddrinfo(pRes);

	switch(mode){
	case SERVER: err = server(sfd); break;
	case CLIENT: err = client(sfd); break;
	}

	close(sfd);

	return err;
}


/* Server Mode. Waits for Message from Clients and Answers them.*/
int server(int sockfd){
	char buf[BUFSIZE];
	char ipaddr[IPSIZE];
	struct sockaddr_storage peer_addr;
	socklen_t peer_addr_len = sizeof(struct sockaddr_storage );

	while(1){
		memset(buf,0,BUFSIZE);
		NOTSILENT("Waiting for incomming Message on Port %s.\n", port);
		if( recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &peer_addr, &peer_addr_len ) < 0 ){
			continue;
		}

		if(!silent){
			ipaddrToStr(ipaddr, IPSIZE, &peer_addr);
			printf("Message received from: %s. Sending Replay.\n\n", ipaddr);
		}

		if( strcmp(buf, MSG) == 0){
			memset(buf,0,BUFSIZE);
			strncpy(buf, RPL, sizeof(RPL));
			sendto(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &peer_addr, peer_addr_len );
		}
	}
	return EXIT_SUCCESS;
}


/* Client Mode. Trys to reach Server. Function terminates after Timeout or then an answer arrives*/
int client(int sockfd){
	char buf[BUFSIZE];
	char ipaddr[IPSIZE];
	struct sockaddr_storage serv_addr;
	socklen_t serv_addr_len = sizeof(serv_addr);
	struct timeval str_time, end_time;
	int duration = 0;

	/* Set Timeout for recv */
	struct timeval tv;
	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));

	/* Send Message */
	memset(buf,0,BUFSIZE);
	strncpy(buf, MSG, sizeof(MSG));

	/* Print IP-Address and start Time measuring */
	if(!silent){
		getpeername(sockfd, (struct sockaddr *)&serv_addr, &serv_addr_len);
		ipaddrToStr(ipaddr, IPSIZE, &serv_addr);
		printf("Sending Message to %s:%s. Connection times out after: %d ms\n", ipaddr, port, timeout);
		gettimeofday(&str_time, NULL);
	}

	if( send(sockfd, buf, BUFSIZE, 0) != BUFSIZE ){
		die("Can't write Message in one row.");
	}

	/* Get Replay */
	memset(buf,0,BUFSIZE);
	recv(sockfd, buf, BUFSIZE, 0);

	/* Check Replay */
	if(strncmp(buf,RPL,sizeof(RPL)) == 0){
		if(!silent){
			gettimeofday(&end_time, NULL);
			duration = (end_time.tv_sec - str_time.tv_sec) * 1000 + (end_time.tv_usec - str_time.tv_usec) / 1000;
			printf("Replay from %s:%s after %d ms.\n",ipaddr,port, duration);
		}
		return EXIT_SUCCESS;
	}else{
		NOTSILENT("Timeout!!\n");
		return EXIT_FAILURE;
	}
}


/* Parse supplied Arguments*/
int parseArgs(int argc, char** argv){
	int  i = 0;
	char OPTS[]  = "slhp:t:";

	/* Parse Args */
	while((i=getopt(argc, argv, OPTS)) != -1){
	    switch(i){
	    case 'h': usage();                break;
	    case 's': silent = 1;             break;
	    case 'l': mode = SERVER;          break;
	    case 'p': port = optarg;          break;
	    case 't': timeout = atoi(optarg); break;
	    case '?': if(optopt == 'p')
	    			  printf("-p needs an argument.\n\n");
	              if(optopt == 't')
	            	  printf("-t needs an argument.\n\n");
	              usage();
	              break;
	    }
	}

	/* Check Portnumber */
	int port_int = atoi(port);
	if( port_int  <= 0 || 65535 < port_int  ){
		printf("Supplied Port number must be between 1 and 65534. \n\n");
		usage();
	}

	/* Check Timeout */
	if( 0 >= timeout ){
		printf("Supplied Timeout is invalid. Timeout must be a positive number\n\n");
		usage();
	}

	/* Get Network Node */
	if( optind == argc -1 ){
		node = argv[optind];
	}

	return 0;
}


/* Print Usage Menu */
int usage(void){
	printf("\nUsage: \n");
	printf("  areYouAlive [-s] [-p portno] -l\n");
	printf("  areYouAlive [-s] [-p portno] [-t timeout] hostname \n");
	printf("  areYouAlive -h\n\n");
	printf("Options:\n");
	printf("  -s            Suppress any tty Output\n");
	printf("  -l            Starts areYouAlive in Server Mode. Clients can connect to this Server\n");
	printf("  -p portno     Specify a Portnumber.\n");
	printf("  -t timout     Specify a timeout in ms\n");
	printf("  -h            Print this help.\n\n");
	printf("Return:\n");
	printf("  areYouAlive, returns 1 if connection timed out or reply was wrong. \n  If everything went well, return is 0.\n\n");
	exit(EXIT_SUCCESS);
}


/* Converts a supplied IP-Address in its String representation */
int ipaddrToStr(char* dst, unsigned int dst_size, struct sockaddr_storage *addr ){
	if( addr->ss_family == AF_INET ){
		struct sockaddr_in *s = (struct sockaddr_in *)addr;
	    inet_ntop(AF_INET, &s->sin_addr, dst, dst_size);
	}else if( addr->ss_family == AF_INET6){
	    struct sockaddr_in6 *s = (struct sockaddr_in6 *)addr;
	    inet_ntop(AF_INET6, &s->sin6_addr, dst, dst_size);
	}else{
		die("idaddrToStr: Unsupported Address family");
	}

	if( errno == ENOSPC ){
		die("idaddrToStr: Buffer dst to small to store IP-Address.\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}


/* Kills Programm and Prints Message */
int die(char* error_msg){
	fprintf(stderr, error_msg);
	exit(EXIT_FAILURE);
}
