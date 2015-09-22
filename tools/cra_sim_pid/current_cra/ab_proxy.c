/*
  The proxy between the cos pid controller and the flight gear
  simulator
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <assert.h>
#include <sys/time.h>
#include <sys/timerfd.h>

#include "ap.h"
#include "ab_proxy.h"
#include "host_port.h"

// cra sim-proxy
#define DEBUG
#define TARGET_HEADING 90.0

int sd;
int sds;

static char *sim_get_fg_data(ap_data *data);
static void sim_set_fg_data();

char sim_request_buf[1024];

// periodic
struct periodic_info
{
	int timer_fd;
	unsigned long long wakeups_missed;
};

unsigned int cra_pid_period = 1000000; // unit is us

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

void error(const char *msg)
{
	perror(msg);
	exit(0);
}

/*********************************/
/*      proxy-cos_pid functions
/*********************************/
static void write_request(struct connection * c);
static void close_connection(struct connection * c);

static void err(char *s)
{
	fprintf(stderr, "%s\n", s);
	if (done)
		printf("Total of %d requests completed\n" , done);
	exit(1);
}

/* simple little function to write an APR error string and exit */

static void apr_err(char *s, apr_status_t rv)
{
	char buf[120];

	fprintf(stderr,
		"%s: %s (%d)\n",
		s, apr_strerror(rv, buf, sizeof buf), rv);
	if (done)
		printf("Total of %d requests completed\n" , done);
	exit(rv);
}

static void set_polled_events(struct connection *c, apr_int16_t new_reqevents)
{
	apr_status_t rv;

	if (c->pollfd.reqevents != new_reqevents) {
		if (c->pollfd.reqevents != 0) {
			rv = apr_pollset_remove(readbits, &c->pollfd);
			if (rv != APR_SUCCESS) {
				apr_err("apr_pollset_remove()", rv);
			}
		}

		if (new_reqevents != 0) {
			c->pollfd.reqevents = new_reqevents;
			rv = apr_pollset_add(readbits, &c->pollfd);
			if (rv != APR_SUCCESS) {
				apr_err("apr_pollset_add()", rv);
			}
		}
	}
}

static void set_conn_state(struct connection *c, connect_state_e new_state)
{
	apr_int16_t events_by_state[] = {
		0,           /* for STATE_UNCONNECTED */
		APR_POLLOUT, /* for STATE_CONNECTING */
		APR_POLLIN,  /* for STATE_CONNECTED; we don't poll in this state,
			      * so prepare for polling in the following state --
			      * STATE_READ
			      */
		APR_POLLIN   /* for STATE_READ */
	};

	c->state = new_state;

	set_polled_events(c, events_by_state[new_state]);
}

static void write_request(struct connection * c)
{
	do {
		apr_time_t tnow;
		apr_size_t l = c->rwrite;
		apr_status_t e = APR_SUCCESS; /* prevent gcc warning */

		tnow = lasttime = apr_time_now();

		/*
		 * First time round ?
		 */
		if (c->rwrite == 0) {
			apr_socket_timeout_set(c->aprsock, 0);
			c->connect = tnow;
			c->rwrote = 0;
			c->rwrite = reqlen;
			if (posting)
				c->rwrite += postlen;
		}
		else if (tnow > c->connect + aprtimeout) {
			printf("Send request timed out!\n");
			close_connection(c);
			return;
		}

		e = apr_socket_send(c->aprsock, request + c->rwrote, &l);

		if (e != APR_SUCCESS && !APR_STATUS_IS_EAGAIN(e)) {
			epipe++;
			printf("Send request failed!\n");
			close_connection(c);
			return;
		}
		totalposted += l;
		c->rwrote += l;
		c->rwrite -= l;
	} while (c->rwrite);

	c->endwrite = lasttime = apr_time_now();
	set_conn_state(c, STATE_READ);
}


static void start_connect(struct connection * c)
{
	apr_status_t rv;

	if (!(started < requests))
		return;

	c->read = 0;
	c->bread = 0;
	c->keepalive = 0;
	c->cbx = 0;
	c->gotheader = 0;
	c->rwrite = 0;
	if (c->ctx)
		apr_pool_clear(c->ctx);
	else
		apr_pool_create(&c->ctx, cntxt);

	if ((rv = apr_socket_create(&c->aprsock, destsa->family,
				    SOCK_STREAM, 0, c->ctx)) != APR_SUCCESS) {
		apr_err("socket", rv);
	}

	c->pollfd.desc_type = APR_POLL_SOCKET;
	c->pollfd.desc.s = c->aprsock;
	c->pollfd.reqevents = 0;
	c->pollfd.client_data = c;

	if ((rv = apr_socket_opt_set(c->aprsock, APR_SO_NONBLOCK, 1))
	    != APR_SUCCESS) {
		apr_err("socket nonblock", rv);
	}

	if (windowsize != 0) {
		rv = apr_socket_opt_set(c->aprsock, APR_SO_SNDBUF, 
					windowsize);
		if (rv != APR_SUCCESS && rv != APR_ENOTIMPL) {
			apr_err("socket send buffer", rv);
		}
		rv = apr_socket_opt_set(c->aprsock, APR_SO_RCVBUF, 
					windowsize);
		if (rv != APR_SUCCESS && rv != APR_ENOTIMPL) {
			apr_err("socket receive buffer", rv);
		}
	}

	c->start = lasttime = apr_time_now();

	if ((rv = apr_socket_connect(c->aprsock, destsa)) != APR_SUCCESS) {
		if (APR_STATUS_IS_EINPROGRESS(rv)) {
			set_conn_state(c, STATE_CONNECTING);
			c->rwrite = 0;
			return;
		}
		else {
			set_conn_state(c, STATE_UNCONNECTED);
			apr_socket_close(c->aprsock);
			err_conn++;
			if (bad++ > 10) {
				fprintf(stderr,
					"\nTest aborted after 10 failures\n\n");
				apr_err("apr_socket_connect()", rv);
			}
            
			start_connect(c);
			return;
		}
	}

	/* connected first time */
	set_conn_state(c, STATE_CONNECTED);
	started++;

	write_request(c);

}

static void close_connection(struct connection * c)
{
	if (c->read == 0 && c->keepalive) {
		/*
		 * server has legitimately shut down an idle keep alive request
		 */
		if (good)
			good--;     /* connection never happened */
	}
	else {
		if (good == 1) {
			/* first time here */
			doclen = c->bread;
		}
		else if (c->bread != doclen) {
			bad++;
			err_length++;
		}
		/* save out time */
		if (done < requests) {
			struct data *s = &stats[done++];
			c->done      = lasttime = apr_time_now();
			s->starttime = c->start;
			s->ctime     = ap_max(0, c->connect - c->start);
			s->time      = ap_max(0, c->done - c->start);
			s->waittime  = ap_max(0, c->beginread - c->endwrite);
			if (heartbeatres && !(done % heartbeatres)) {
				fprintf(stderr, "Completed %d requests\n", done);
				fflush(stderr);
			}
		}
	}

	set_conn_state(c, STATE_UNCONNECTED);
	apr_socket_close(c->aprsock);

	/* connect again */
	start_connect(c);
	return;
}

/* read data from connection */

static void read_connection(struct connection * c)
{
	apr_size_t r;
	apr_status_t status;
	char *part;
	char respcode[4];       /* 3 digits and null */

	r = sizeof(buffer);
	status = apr_socket_recv(c->aprsock, buffer, &r);
	if (APR_STATUS_IS_EAGAIN(status)) {
		return;
	}
	else if (r == 0 && APR_STATUS_IS_EOF(status)) {
		good++;
		close_connection(c);
		return;
	}
	/* catch legitimate fatal apr_socket_recv errors */
	else if (status != APR_SUCCESS) {
		err_recv++;
		if (recverrok) {
			bad++;
			close_connection(c);
			if (verbosity >= 1) {
				char buf[120];
				fprintf(stderr,"%s: %s (%d)\n", "apr_socket_recv", apr_strerror(status, buf, sizeof buf), status);
			}
			return;
		} else {
			//apr_err("apr_socket_recv", status);
		}
        }
	totalread += r;
	if (c->read == 0) {
		c->beginread = apr_time_now();
	}
	c->read += r;


	if (!c->gotheader) {
		char *s;
		int l = 4;
		apr_size_t space = CBUFFSIZE - c->cbx - 1; /* -1 allows for \0 term */
		int tocopy = (space < r) ? space : r;
		memcpy(c->cbuff + c->cbx, buffer, space);

		c->cbx += tocopy;
		space -= tocopy;
		c->cbuff[c->cbx] = 0;   /* terminate for benefit of strstr */
		if (verbosity >= 2) {
			printf("LOG: header received:\n%s\n", c->cbuff);
		}
		s = strstr(c->cbuff, "\r\n\r\n");
		/*
		 * this next line is so that we talk to NCSA 1.5 which blatantly
		 * breaks the http specifaction
		 */
		if (!s) {
			s = strstr(c->cbuff, "\n\n");
			l = 2;
		}

		if (!s) {
			/* read rest next time */
			if (space) {
				return;
			}
			else {
				/* header is in invalid or too big - close connection */
				set_conn_state(c, STATE_UNCONNECTED);
				apr_socket_close(c->aprsock);
				err_response++;
				if (bad++ > 10) {
					err("\nTest aborted after 10 failures\n\n");
				}
				start_connect(c);
			}
		}
		else {
			/* have full header */
			if (!good) {
				/*
				 * this is first time, extract some interesting info
				 */
				char *p, *q;
				p = strstr(c->cbuff, "Server:");
				q = servername;
				if (p) {
					p += 8;
					while (*p > 32)
						*q++ = *p++;
				}
				*q = 0;
			}
			/*
			 * XXX: this parsing isn't even remotely HTTP compliant... but in
			 * the interest of speed it doesn't totally have to be, it just
			 * needs to be extended to handle whatever servers folks want to
			 * test against. -djg
			 */

			/* check response code */
			part = strstr(c->cbuff, "HTTP");    /* really HTTP/1.x_ */
			if (part && strlen(part) > strlen("HTTP/1.x_")) {
				strncpy(respcode, (part + strlen("HTTP/1.x_")), 3);
				respcode[3] = '\0';
			}
			else {
				strcpy(respcode, "500");
			}

			if (respcode[0] != '2') {
				err_response++;
				if (verbosity >= 2)
					printf("WARNING: Response code not 2xx (%s)\n", respcode);
			}
			else if (verbosity >= 3) {
				printf("LOG: Response code = %s\n", respcode);
			}
			c->gotheader = 1;
			*s = 0;     /* terminate at end of header */
			if (keepalive &&
			    (strstr(c->cbuff, "Keep-Alive")
			     || strstr(c->cbuff, "keep-alive"))) {  /* for benefit of MSIIS */
				char *cl;
				cl = strstr(c->cbuff, "Content-Length:");
				/* handle NCSA, which sends Content-length: */
				if (!cl)
					cl = strstr(c->cbuff, "Content-length:");
				if (cl) {
					c->keepalive = 1;
					/* response to HEAD doesn't have entity body */
					c->length = posting >= 0 ? atoi(cl + 16) : 0;
				}
				/* The response may not have a Content-Length header */
				if (!cl) {
					c->keepalive = 1;
					c->length = 0; 
				}
			}
			c->bread += c->cbx - (s + l - c->cbuff) + r - tocopy;
			totalbread += c->bread;
		}
	}
	else {
		/* outside header, everything we have read is entity body */
		c->bread += r;
		totalbread += r;
	}

	if (c->keepalive && (c->bread >= c->length)) {
		/* finished a keep-alive connection */
		good++;
		/* save out time */
		if (good == 1) {
			/* first time here */
			doclen = c->bread;
		}
		else if (c->bread != doclen) {
			bad++;
			err_length++;
		}
		if (done < requests) {
			struct data *s = &stats[done++];
			doneka++;
			c->done      = apr_time_now();
			s->starttime = c->start;
			s->ctime     = ap_max(0, c->connect - c->start);
			s->time      = ap_max(0, c->done - c->start);
			s->waittime  = ap_max(0, c->beginread - c->endwrite);
			if (heartbeatres && !(done % heartbeatres)) {
				fprintf(stderr, "Completed %d requests\n", done);
				fflush(stderr);
			}
		}
		c->keepalive = 0;
		c->length = 0;
		c->gotheader = 0;
		c->cbx = 0;
		c->read = c->bread = 0;
		/* zero connect time with keep-alive */
		c->start = c->connect = lasttime = apr_time_now();
		write_request(c);
	}

	good++;
}

static void init_proxy2pid(void)
{
	apr_time_t stoptime;
	apr_int16_t rv;
	int i;
	apr_status_t status;
	int snprintf_res = 0;

	// sim
	ap_data in_data;
	ap_data out_data;
	int ap_active = 0;


	static int out_to_pid = 0;

	connectport = 200;
	connecthost = "10.0.2.8";
	requests = 1;
	concurrency = 1; // Jiguo: test for CRA read/write (fix to 1)

	con = calloc(concurrency, sizeof(struct connection));

	stats = calloc(requests, sizeof(struct data));

	if ((status = apr_pollset_create(&readbits, concurrency, cntxt,
					 APR_POLLSET_NOCOPY)) != APR_SUCCESS) {
		apr_err("apr_pollset_create failed", status);
	}

	/* This only needs to be done once */
	if ((rv = apr_sockaddr_info_get(&destsa, connecthost, APR_UNSPEC, connectport, 0, cntxt))
	    != APR_SUCCESS) {
		char buf[120];
		apr_snprintf(buf, sizeof(buf),
			     "apr_sockaddr_info_get() for %s", connecthost);
		apr_err(buf, rv);
	}

	/* ok - lets start */
	start = lasttime = apr_time_now();
	stoptime = tlimit ? (start + apr_time_from_sec(tlimit)) : AB_MAX;

	/* initialise lots of requests */
	for (i = 0; i < concurrency; i++) {
		con[i].socknum = i;
		start_connect(&con[i]);
	}


	// Kevin Andy Timer
	//struct periodic_info info;
	//init_periodic(cra_pid_period, &info);  // define in us

	do {
		apr_int32_t n;
		const apr_pollfd_t *pollresults;

		n = concurrency;
//		printf("before apr_pollset_poll\n");
		do {
			status = apr_pollset_poll(readbits, aprtimeout, &n, &pollresults);
		} while (APR_STATUS_IS_EINTR(status));
//		printf("after apr_pollset_poll\n");
		if (status != APR_SUCCESS)
			apr_err("apr_poll", status);

		if (!n) {
			err("\nServer timed out\n\n");
		}

		for (i = 0; i < n; i++) {
			const apr_pollfd_t *next_fd = &(pollresults[i]);
			struct connection *c;

			c = next_fd->client_data;

			/*
			 * If the connection isn't connected how can we check it?
			 */
			if (c->state == STATE_UNCONNECTED)
				continue;

			rv = next_fd->rtnevents;

			if ((rv & APR_POLLIN) || (rv & APR_POLLPRI) || (rv & APR_POLLHUP)) {
				read_connection(c);
				set_conn_state(c, STATE_CONNECTING); // Jiguo: CRA only, write again

				printf("proxy: sim_set_fg_data buffer %s\n", buffer);  // Jiguo: CRA test read from COS

				sim_set_fg_data();   // Jiguo: send data to sim
			}
			if ((rv & APR_POLLERR) || (rv & APR_POLLNVAL)) {
				bad++;
				err_except++;
				/* avoid apr_poll/EINPROGRESS loop on HP-UX, let recv discover ECONNREFUSED */
				if (c->state == STATE_CONNECTING) { 
					read_connection(c);
//		    printf("after read_connect 2\n");
				}
				else { 
					start_connect(c);
//		    printf("after start_connect 1\n");
				}
				continue;
			}
			if (rv & APR_POLLOUT) {
				if (c->state == STATE_CONNECTING) {
					rv = apr_socket_connect(c->aprsock, destsa);
//		    printf("after apr_socket_connect\n");
					if (rv != APR_SUCCESS) {
						apr_socket_close(c->aprsock);
//			printf("after apr_socket_close\n");
						err_conn++;
						if (bad++ > 10) {
							fprintf(stderr,
								"\nTest aborted after 10 failures\n\n");
							apr_err("apr_socket_connect()", rv);
						}
						set_conn_state(c, STATE_UNCONNECTED);
						start_connect(c);
//			printf("after start_connect 2\n");
						continue;
					}
					else {
						set_conn_state(c, STATE_CONNECTED);
						started++;
					
						// Jiguo: CRA
						// read data from sim and construct the request
						printf("proxy: sim_get_gf_data....\n");
						char *tmp = sim_get_fg_data(&in_data);
		
						snprintf_res = apr_snprintf(request, sizeof(_request), "%s", tmp); // test
						reqlen = strlen(request);
						//printf("request is %s\n", request);

						write_request(c);

//                        sprintf(request, "%d", out_to_pid++);
//                 	reqlen = strlen(request);
//			printf("after write request\n");
					}
				}
				else {
					write_request(c);
//		    printf("after write request 2342343!!!\n");
				}
			}
		}
//		wait_period (&info);
	} while(1);
	
	if (heartbeatres)
		fprintf(stderr, "Finished %d requests\n", done);
	else
		printf("..done\n");
}

/*********************************/
/*      proxy-sim functions
/*********************************/

static char *
sim_get_fg_data(struct ap_data *data)
{
	char *ret = NULL;
	
//	char buf[1024];
	char *tok;
	char this_byte = '\0';
	int bytes_read = 0;
	int total_bytes_read = 0;
	double val;

	memset(sim_request_buf, 0, 1024);

	do {
		bytes_read = read(sds, &this_byte, 1);
		if(bytes_read < 0) {
			return NULL;
		}
		if(bytes_read > 0) {
			sim_request_buf[total_bytes_read] = this_byte;
		}
		total_bytes_read += bytes_read;
	} while (this_byte != '\n');
	sim_request_buf[total_bytes_read-1] = '\0';

#ifdef DEBUG
	printf("%s\n", sim_request_buf);
#endif

	// test
	ret = sim_request_buf;
	return ret;


	//TODO: move this to Composite and only pass the string to Cos
	// Now parse the line

	// Flight Controls
	tok = strtok(sim_request_buf, ",");
	val = atof(tok);
	data->aileron = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->aileron_trim = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->elevator = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->elevator_trim = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->rudder = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->rudder_trim = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->flaps = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->slats = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->speedbrake = (float)(val);

	// Engines
	tok = strtok(NULL, ",");
	val = atof(tok);
	data->throttle0 = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->throttle1 = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->starter0 = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->starter1 = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->fuel_pump0 = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->fuel_pump1 = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->cutoff0 = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->cutoff1 = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->mixture0 = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->mixture1 = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->propeller_pitch0 = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->propeller_pitch1 = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->magnetos0 = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->magnetos1 = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->ignition0 = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->ignition1 = (float)(val);

	// Gear
	tok = strtok(NULL, ",");
	val = atof(tok);
	data->brake_left = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->brake_right = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->brake_parking = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->steering = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->gear_down = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->gear_position0 = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->gear_position1 = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->gear_position2 = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->gear_position3 = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->gear_position4 = (float)(val);

	// Hydraulics
	tok = strtok(NULL, ",");
	val = atof(tok);
	data->engine_pump0 = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->engine_pump1 = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->electric_pump0 = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->electric_pump1 = (float)(val);

	// Electric
	tok = strtok(NULL, ",");
	val = atof(tok);
	data->battery_switch = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->external_power = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->APU_generator = (float)(val);

	// Autoflight
	tok = strtok(NULL, ",");
	val = atof(tok);
	data->engage = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->heading_select = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->altitude_select = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->bank_angle_select = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->vertical_speed_select = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->speed_select = (float)(val);

	// Position
	tok = strtok(NULL, ",");
	val = atof(tok);
	data->latitude_deg = val;

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->longitude_deg = val;

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->altitude_ft = val;

	// Orientation
	tok = strtok(NULL, ",");
	val = atof(tok);
	data->roll_deg = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->pitch_deg = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->heading_deg = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->side_slip_deg = (float)(val);

	// Velocities
	tok = strtok(NULL, ",");
	val = atof(tok);
	data->airspeed_kt = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->glideslope = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->mach = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->speed_down_fps = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->speed_east_fps = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->speed_north_fps = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->uBody_fps = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->vBody_fps = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->wBody_fps = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->vertical_speed_fps = (float)(val);

	// Accelerations
	tok = strtok(NULL, ",");
	val = atof(tok);
	data->nlf = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->ned_down_accel_fps_sec = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->ned_east_accel_fps_sec = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->ned_north_accel_fps_sec = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->pilot_x_accel_fps_sec = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->pilot_y_accel_fps_sec = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->pilot_z_accel_fps_sec = (float)(val);

	// Surface Positions
	tok = strtok(NULL, ",");
	val = atof(tok);
	data->elevator_pos_norm = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->flap_pos_norm = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->left_aileron_pos_norm = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->right_aileron_pos_norm = (float)(val);

	tok = strtok(NULL, ",");
	val = atof(tok);
	data->rudder_pos_norm = (float)(val);
 
}

static void
sim_set_fg_data()
{
	char buf[32];
	int len, n;
	ap_data *data;

	// buffer is a global buffer that holds the result read from PID
	strcat(buffer, "\n"); // socket recv does not add "\n", so we do here
	n = write(sd, buffer, strlen(buffer));   // sd
	/* n = write(sd, "hello to write", 20); */
	if (n < 0)
		error("ERROR writing to socket");
	printf("%s\n",buffer);
	return;
}

static int
init_connection_proxy2sim()
{
	int sockfd, portno, n;
	struct sockaddr_in serv_addr;
	struct hostent *server;

	char buffer[256];

	portno = FGSIM_PORT;

	sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd < 0) 
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
	printf("proxy connecting sim\n");
	if (connect(sd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
		error("ERROR connecting");
	printf("proxy connected sim\n");
	return 0;
}

static int
init_connection_sim2proxy()
{
	int sockfd, newsockfd, portno;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
		error("ERROR opening socket");

	bzero((char *) &serv_addr, sizeof(serv_addr));
	portno = PROXY_PORT;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);
	if (bind(sockfd, (struct sockaddr *) &serv_addr,
		 sizeof(serv_addr)) < 0) 
		error("ERROR on binding");
	listen(sockfd,5);
	clilen = sizeof(cli_addr);
	printf("proxy server accepting...\n");
	sds = accept(sockfd, 
			   (struct sockaddr *) &cli_addr, 
			   &clilen);
	if (sds < 0) 
		error("ERROR on accept");
	printf("proxy server accepted \n");

	close(sockfd);
	return 0; 
}


//#define DEBUG_NETWORK_LOOP

static void
init_proxy2sim()
{
	char buffer[256];
	int n;
	init_connection_sim2proxy();
	init_connection_proxy2sim();

#ifdef DEBUG_NETWORK_LOOP
	while(1) {
		bzero(buffer,256);
		n = read(sds, buffer,255);   // sds
		if (n < 0) error("ERROR reading from socket");
		printf("Proxy: Here is the message: %s",buffer);
		printf("Proxy: Please enter the message: ");
		bzero(buffer,256);
		/* fgets(buffer,255,stdin); */
		char *test = buffer;
		test = "hello world\n";
		memcpy(buffer, test, strlen(test));
		n = write(sd, buffer, strlen(buffer));   // sd
		/* n = write(sd, "hello to write", 20); */
		if (n < 0)
			error("ERROR writing to socket");
		printf("%s\n",buffer);
	}
#endif
}


int main(int argc, const char * const argv[])
{
	int r, l;
	char tmp[1024];
	apr_status_t status;
	apr_getopt_t *opt;
	const char *optarg;
	char c;

	apr_app_initialize(&argc, &argv, NULL);
	atexit(apr_terminate);
	apr_pool_create(&cntxt, NULL);

	apr_getopt_init(&opt, cntxt, argc, argv);

	// initialize flight simulator 
	init_proxy2sim();

#ifdef DEBUG_NETWORK_LOOP
	return;
#endif
	// initialize this proxy
	init_proxy2pid();

	// close connections
	apr_pool_destroy(cntxt);
	close(sd);

	return 0;
}


