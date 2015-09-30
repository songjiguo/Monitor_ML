#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <sys/time.h>
#include <sys/timerfd.h>
#include <assert.h>

#include "client_server_test.h"

char buffer[256];

void error(const char *msg)
{
	perror(msg);
	exit(0);
}

// periodic
struct periodic_info
{
	int timer_fd;
	unsigned long long wakeups_missed;
};

unsigned int sim_client_period = 10000; // unit is us

static int init_periodic (unsigned int period, struct periodic_info *info)
{
	int ret;
	unsigned int ns;
	unsigned int sec;
	int fd;
	struct itimerspec itval;

	/* Create the timer */
	fd = timerfd_create (CLOCK_MONOTONIC, 0);
	info->wakeups_missed = 0;
	info->timer_fd = fd;
	if (fd == -1) assert(0);

	/* Make the timer periodic */
	sec = period/1000000;
	ns = (period - (sec * 1000000)) * 1000;
	itval.it_interval.tv_sec = sec;
	itval.it_interval.tv_nsec = ns;
	itval.it_value.tv_sec = sec;
	itval.it_value.tv_nsec = ns;
	ret = timerfd_settime (fd, 0, &itval, NULL);
	return ret;
}

static void wait_period (struct periodic_info *info)
{
	unsigned long long missed;
	int ret;

	/* Wait for the next timer event. If we have missed any the
	   number is written to "missed" */
	ret = read(info->timer_fd, &missed, sizeof(missed));
	if (ret == -1)
	{
		printf("failed to read timer");
		assert(0);
	}

	info->wakeups_missed += missed;
}


int main(int argc, char *argv[])
{
	int sockfd, portno, n;
	struct sockaddr_in serv_addr;
	struct hostent *server;

	portno = PROXY_PORT;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
		error("ERROR opening socket");

	/* server = gethostbyname(argv[1]); */
	server = gethostbyname(HOST);
	if (server == NULL) {
		fprintf(stderr,"ERROR, no such host\n");
		exit(0);
	}
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, 
	      (char *)&serv_addr.sin_addr.s_addr,
	      server->h_length);
	serv_addr.sin_port = htons(portno);
	if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
		error("ERROR connecting");

	struct periodic_info info;
	init_periodic(sim_client_period, &info);  // define in us

	char *fake_msg = "fake msg from flight simulator";
	while(1) {
//		printf("sim client: Please enter the message: ");
//		bzero(buffer,256);
//		fgets(buffer,255,stdin);
		memcpy(buffer, fake_msg, strlen(fake_msg));
		strcat(buffer, "\n");
		n = write(sockfd,buffer, strlen(buffer));
		if (n < 0) 
			error("ERROR writing to socket");
		
		memset(buffer, 0, strlen(buffer));
		wait_period (&info);
	}
	close(sockfd);
	return 0;
}
