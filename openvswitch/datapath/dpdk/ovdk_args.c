/*
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <string.h>

#include <rte_config.h>
#include <rte_common.h>
#include <rte_log.h>

#include "ovdk_args.h"
#include "ovdk_config.h"

#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1

#define PARAM_STATS_INTERVAL "stats_int"
#define PARAM_STATS_CORE "stats_core"

#define LOG_LEVEL_BASE         10
#define PORTMASK_BASE          16

static const char *progname;
static uint64_t port_mask = 0;
static int stats_interval = 0;
static int stats_core = -1;
/* Default log level - used if '-v' parameter not supplied */
static unsigned log_level = RTE_LOG_ERR;

static uint32_t max_frame_size = OVDK_DEFAULT_MAX_FRAME_SIZE;

static int parse_str_to_uint32t(const char *str, uint32_t *num);
static int parse_portmask(const char *portmask);
static int parse_log_level(const char *level_arg);

/*
 * Display usage instructions.
 */
void
ovdk_args_usage(const char *name)
{
	printf(
	    "%s: Intel DPDK vSwitch datapath application\n"
	    "usage: %s [EAL] -- [ARG...]\n"
	    "\n"
	    "Required Arguments:\n"
	    "  -p PORTMASK                 hex bitmask of phy ports to use\n"
	    "\n"
	    "Optional Arguments:\n"
	    "  -v LOG_LEVEL                verbosity of ovs-dpdk logging "
	                                   "(default: 4)\n"
	    "                              1=EMERGENCY,	2=ALERT,"
	                                   "	3=CRITICAL,\n"
	    "                              4=ERROR		5=WARNING,	"
	                                   "6=NOTICE,\n"
	    "                              7=INFORMATION,	8=DEBUG)\n"
	    "                              ** Higher log levels print all "
	                                   "lower level logs **\n"
	    "  --stats_int INT             print stats every INT (default: 0)\n"
	    "  --stats_core CORE           id of core used to print stats\n"
	    "  -J FRAME_SIZE: maximum frame size (Default %d)\n",
	    name, name, OVDK_DEFAULT_MAX_FRAME_SIZE);
}

/*
 * Parse application arguments.
 *
 * The application specific arguments succeed the DPDK-specific arguments,
 * which are stripped by the DPDK EAL init. Process these application
 * arguments, and display the usage info on error.
 */
int
ovdk_args_parse_app_args(int argc, char *argv[])
{
	int option_index, opt;
	char **argvopt = argv;
	static struct option lgopts[] = {
		{PARAM_STATS_INTERVAL, required_argument, NULL, 0},
		{PARAM_STATS_CORE, required_argument, NULL, 0},
		{NULL, 0, NULL, 0}
	};

	progname = argv[0];

	while ((opt = getopt_long(argc, argvopt, "p:v:J:", lgopts,
	                          &option_index)) != EOF) {
		switch (opt) {
		case 'p':
			if (parse_portmask(optarg) != 0) {
				ovdk_args_usage(progname);
				rte_exit(EXIT_FAILURE, "Invalid option"
				         " specified '%c'\n", opt);
			}
			break;
		case 'v':
			if (parse_log_level(optarg) != 0) {
				ovdk_args_usage(progname);
				rte_exit(EXIT_FAILURE, "Invalid log level"
				         " specified '%s'\n", optarg);
			}
			break;
		case 'J':  /* Frame size */
			if (parse_str_to_uint32t(optarg, &max_frame_size)) {
				ovdk_args_usage(progname);
				return -1;
			}
			break;
		case 0:
			if (strncmp(lgopts[option_index].name,
			            PARAM_STATS_INTERVAL, 9) == 0)
				stats_interval = atoi(optarg);
			else if (strncmp(lgopts[option_index].name,
			                 PARAM_STATS_CORE, 10) == 0)
				stats_core = atoi(optarg);
			break;
		case '?':
		default:
			ovdk_args_usage(progname);
			if (optopt)
				rte_exit(EXIT_FAILURE, "Invalid option '-%c'"
				         "\n", optopt);
			else
				rte_exit(EXIT_FAILURE, "Invalid option '--%s'"
				         "\n", argv[optind - 1]);
		}
	}

	return 0;
}

/*
 * Parse the supplied portmask argument.
 *
 * This does not actually validate the port bitmask - it merely parses and
 * stores it. As a result, validation must be carried out on this value.
 */
static int
parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long num = 0;

	num = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	port_mask = num;

	return 0;
}

/**
 * Take a string and try to convert it to uint32_t.
 * Return 0 if success -1 otherwise.
 */
static int
parse_str_to_uint32t(const char *str, uint32_t *num)
{
	char *end = NULL;
	unsigned long temp;

	if (str == NULL || *str == '\0')
		return -1;

	temp = strtoul(str, &end, 10);
	if (end == NULL || *end != '\0' || temp == 0)
		return -1;

	*num = (uint32_t) temp;
	return 0;
}

/*
 * Parse and validate the supplied log level argument.
 */
static int
parse_log_level(const char *level_arg)
{
	char *end = NULL;
	unsigned long level = log_level;

	if (level_arg == NULL || *level_arg == '\0')
		return -1;

	/* Convert parameter to a number and verify */
	level = strtoul(level_arg, &end, LOG_LEVEL_BASE);

	if (end == NULL || *end != '\0' || level == 0 || level > RTE_LOG_DEBUG)
		return -1;

	log_level = level;

	return 0;
}

uint64_t
ovdk_args_get_portmask(void){
	return port_mask;
}

unsigned
ovdk_args_get_log_level(void) {
	return log_level;
}

int
ovdk_args_get_stats_interval(void) {
	return stats_interval;
}

int
ovdk_args_get_stats_core(void) {
	return stats_core;
}

uint32_t
ovdk_args_get_max_frame_size(void) {
	return max_frame_size;
}
