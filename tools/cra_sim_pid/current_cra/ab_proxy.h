#define AP_AB_BASEREVISION "2.3"

#include <apr.h>
#include <apr_signal.h>
#include <apr_strings.h>
#include <apr_network_io.h>
#include <apr_file_io.h>
#include <apr_time.h>
#include <apr_getopt.h>
#include <apr_general.h>
#include <apr_lib.h>
#include <apr_portable.h>
#include <apr_poll.h>
#include "ap_release.h"

#define APR_WANT_STRFUNC
#include <apr_want.h>

#include <apr_base64.h>
#if APR_HAVE_STDIO_H
#include <stdio.h>
#endif
#if APR_HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if APR_HAVE_UNISTD_H
#include <unistd.h> /* for getpid() */
#endif

#include <math.h>
#if APR_HAVE_CTYPE_H
#include <ctype.h>
#endif
#if APR_HAVE_LIMITS_H
#include <limits.h>
#endif

/* ------------------- DEFINITIONS -------------------------- */

#ifndef LLONG_MAX
#define AB_MAX APR_INT64_C(0x7fffffffffffffff)
#else
#define AB_MAX LLONG_MAX
#endif

/* maximum number of requests on a time limited test */
#define MAX_REQUESTS (INT_MAX > 50000 ? 50000 : INT_MAX)

/* connection state
 * don't add enums or rearrange or otherwise change values without
 * visiting set_conn_state()
 */
typedef enum {
	STATE_UNCONNECTED = 0,
	STATE_CONNECTING,           /* TCP connect initiated, but we don't
				     * know if it worked yet
				     */
	STATE_CONNECTED,            /* we know TCP connect completed */
	STATE_READ
} connect_state_e;

#define CBUFFSIZE (2048)

struct connection {
	apr_pool_t *ctx;
	apr_socket_t *aprsock;
	apr_pollfd_t pollfd;
	int state;
	apr_size_t read;            /* amount of bytes read */
	apr_size_t bread;           /* amount of body read */
	apr_size_t rwrite, rwrote;  /* keep pointers in what we write - across
				     * EAGAINs */
	apr_size_t length;          /* Content-Length value used for keep-alive */
	char cbuff[CBUFFSIZE];      /* a buffer to store server response header */
	int cbx;                    /* offset in cbuffer */
	int keepalive;              /* non-zero if a keep-alive request */
	int gotheader;              /* non-zero if we have the entire header in
				     * cbuff */
	apr_time_t start,           /* Start of connection */
		connect,         /* Connected, start writing */
		endwrite,        /* Request written */
		beginread,       /* First byte of input */
		done;            /* Connection closed */

	int socknum;
};

struct data {
	apr_time_t starttime;         /* start time of connection */
	apr_interval_time_t waittime; /* between request and reading response */
	apr_interval_time_t ctime;    /* time to connect */
	apr_interval_time_t time;     /* time for connection */
};

#define ap_min(a,b) ((a)<(b))?(a):(b)
#define ap_max(a,b) ((a)>(b))?(a):(b)
#define ap_round_ms(a) ((apr_time_t)((a) + 500)/1000)
#define ap_double_ms(a) ((double)(a)/1000.0)
#define MAX_CONCURRENCY 20000

/* --------------------- GLOBALS ---------------------------- */

int verbosity = 0;      /* no verbosity by default */
int recverrok = 0;      /* ok to proceed after socket receive errors */
int posting = 0;        /* GET by default */
int requests = 1;       /* Number of requests to make */
int heartbeatres = 100; /* How often do we say we're alive */
int concurrency = 1;    /* Number of multiple requests to make */
int percentile = 1;     /* Show percentile served */
int confidence = 1;     /* Show confidence estimator and warnings */
int tlimit = 0;         /* time limit in secs */
int keepalive = 0;      /* try and do keepalive connections */
int windowsize = 0;     /* we use the OS default window size */
char servername[1024];  /* name that server reports */
char *hostname;         /* host name from URL */
char *host_field;       /* value of "Host:" header field */
char *path;             /* path name */
char postfile[1024];    /* name of file containing post data */
char *postdata;         /* *buffer containing data from postfile */
apr_size_t postlen = 0; /* length of data to be POSTed */
char content_type[1024];/* content type to put in POST header */
char *cookie,           /* optional cookie line */
	*auth,             /* optional (basic/uuencoded) auhentication */
	*hdrs;             /* optional arbitrary headers */
apr_port_t port;        /* port number */
char proxyhost[1024];   /* proxy host name */
int proxyport = 0;      /* proxy port */
char *connecthost;
apr_port_t connectport;
char *gnuplot;          /* GNUplot file */
char *csvperc;          /* CSV Percentile file */
char url[1024];
char * fullurl, * colonhost;
int isproxy = 0;
apr_interval_time_t aprtimeout = apr_time_from_sec(30); /* timeout value */

/* overrides for ab-generated common headers */
int opt_host = 0;       /* was an optional "Host:" header specified? */
int opt_useragent = 0;  /* was an optional "User-Agent:" header specified? */
int opt_accept = 0;     /* was an optional "Accept:" header specified? */
/*
 * XXX - this is now a per read/write transact type of value
 */

int use_html = 0;       /* use html in the report */
const char *tablestring;
const char *trstring;
const char *tdstring;

apr_size_t doclen = 0;     /* the length the document should be */
apr_int64_t totalread = 0;    /* total number of bytes read */
apr_int64_t totalbread = 0;   /* totoal amount of entity body read */
apr_int64_t totalposted = 0;  /* total number of bytes posted, inc. headers */
int started = 0;           /* number of requests started, so no excess */
int done = 0;              /* number of requests we have done */
int doneka = 0;            /* number of keep alive connections done */
int good = 0, bad = 0;     /* number of good and bad requests */
int epipe = 0;             /* number of broken pipe writes */
int err_length = 0;        /* requests failed due to response length */
int err_conn = 0;          /* requests failed due to connection drop */
int err_recv = 0;          /* requests failed due to broken read */
int err_except = 0;        /* requests failed due to exception */
int err_response = 0;      /* requests with invalid or non-200 response */

apr_time_t start, lasttime, stoptime;

/* global request (and its length) */
char _request[2048];
char *request = _request;
apr_size_t reqlen;

/* one global throw-away buffer to read stuff into */
char buffer[8192];

/* interesting percentiles */
int percs[] = {50, 66, 75, 80, 90, 95, 98, 99, 100};

struct connection *con;     /* connection array */
struct data *stats;         /* data for each request */
apr_pool_t *cntxt;

apr_pollset_t *readbits;

apr_sockaddr_t *destsa;
