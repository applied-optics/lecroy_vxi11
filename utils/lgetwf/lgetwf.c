/* lgetwf.c
 * Copyright (C) 2010 Steve D. Sharples
 *
 * Command line utility to acquire traces from LeCroy oscilloscopes.
 * After compiling, run it without any arguments for help info.
 * For historical reasons, we have our own data format for scope trace data.
 * Each trace consists of a trace.wf file that contains the binary data, and a
 * trace.wfi text file that contains the waveform info. We then use a very
 * cheesy Matlab script, loadwf.m to load the data into Matlab. The wfi file
 * does not contain all the information that LeCroy's own "preamble"
 * information contains; on the other hand, you can have multiple traces in
 * the same wf file.
 *
 * The source is extensively commented and from this, and a look at the 
 * lecroy_vxi11.c library, you will begin to understand the approach to 
 * acquiring data that I've taken.
 *
 * You will also need the
 * vxi11_X.XX.tar.gz source, currently available from:
 * http://optics.eee.nottingham.ac.uk/vxi11/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The author's email address is steve.sharples@nottingham.ac.uk
 */

#include <stdio.h>
#include <string.h>

#include "lecroy_vxi11.h"

#ifndef	BOOL
#define	BOOL	int
#endif
#ifndef TRUE
#define	TRUE	1
#endif
#ifndef FALSE
#define	FALSE	0
#endif

BOOL sc(const char *, const char *);

int main(int argc, char *argv[])
{

	static char *progname;
	static char *serverIP;
	char chnl;		/* we use '1' to '4' for channels, and 'A' to 'D' for FUNC[1...4] */
	FILE *f_wf;
	char wfname[256];
	char wfiname[256];
	long buf_size;
	char *buf;
	unsigned long timeout = 10000;	/* in ms (= 10 seconds) */

	long bytes_returned;
	BOOL clear_sweeps = TRUE;
	BOOL got_ip = FALSE;
	BOOL got_scope_channel = FALSE;
	BOOL got_file = FALSE;
	BOOL got_no_averages = FALSE;
	BOOL got_segmented_averages = FALSE;
	int no_averages;
	BOOL got_no_segments = FALSE;
	int no_segments = 1;
	int bytes_per_point = 2;
	int index = 1;
	double s_rate = 0;
	long npoints = 0;
	double actual_s_rate;
	long actual_npoints;
	VXI11_CLINK *clink;	/* client link (actually a structure contining CLIENT and VXI11_LINK pointers) */
	int l;
	char cmd[256];
	long long_ret;
	double double_ret;

	progname = argv[0];

	while (index < argc) {
		if (sc(argv[index], "-filename") || sc(argv[index], "-f")
		    || sc(argv[index], "-file")) {
			snprintf(wfname, 256, "%s.wf", argv[++index]);
			snprintf(wfiname, 256, "%s.wfi", argv[index]);
			got_file = TRUE;
		}

		if (sc(argv[index], "-ip") || sc(argv[index], "-ip_address")
		    || sc(argv[index], "-IP")) {
			serverIP = argv[++index];
			got_ip = TRUE;
		}

		if (sc(argv[index], "-channel") || sc(argv[index], "-c")
		    || sc(argv[index], "-scope_channel")) {
			sscanf(argv[++index], "%c", &chnl);
			got_scope_channel = TRUE;
		}

		if (sc(argv[index], "-sample_rate") || sc(argv[index], "-s")
		    || sc(argv[index], "-rate")) {
			sscanf(argv[++index], "%lg", &s_rate);	/* %g in sscanf is a float, so use %lg for double. %g in printf is a double. Great. */
		}

		if (sc(argv[index], "-no_points") || sc(argv[index], "-n")
		    || sc(argv[index], "-points")) {
			sscanf(argv[++index], "%ld", &npoints);
		}

		if (sc(argv[index], "-bytes_per_point") || sc(argv[index], "-b")
		    || sc(argv[index], "-bytes")) {
			sscanf(argv[++index], "%d", &bytes_per_point);
		}

		if (sc(argv[index], "-averages") || sc(argv[index], "-a")
		    || sc(argv[index], "-aver")) {
			sscanf(argv[++index], "%d", &no_averages);
			got_no_averages = TRUE;
			clear_sweeps = TRUE;
		}

		if (sc(argv[index], "-seg_averages") || sc(argv[index], "-sa")
		    || sc(argv[index], "-seg_aver")) {
			sscanf(argv[++index], "%d", &no_averages);
			got_segmented_averages = TRUE;
			clear_sweeps = TRUE;
		}

		if (sc(argv[index], "-segmented") || sc(argv[index], "-seg")
		    || sc(argv[index], "-seq")) {
			sscanf(argv[++index], "%d", &no_segments);
			got_no_segments = TRUE;
		}

		if (sc(argv[index], "-clear_sweeps") || sc(argv[index], "-clsw")
		    || sc(argv[index], "-clear")) {
			clear_sweeps = TRUE;
		}

		if (sc(argv[index], "-timeout") || sc(argv[index], "-t")) {
			sscanf(argv[++index], "%lu", &timeout);
		}

		index++;
	}

	if (got_file == FALSE || got_scope_channel == FALSE || got_ip == FALSE) {
		printf
		    ("%s: grabs a waveform from an Agilent scope via ethernet, by Steve (June 06)\n",
		     progname);
		printf("Run using %s [arguments]\n\n", progname);
		printf("REQUIRED ARGUMENTS:\n");
		printf
		    ("-ip    -ip_address     -IP      : IP address of scope (eg 128.243.74.78)\n");
		printf
		    ("-f     -filename       -file    : filename (without extension)\n");
		printf
		    ("-c     -scope_channel  -channel : scope channel (1,2,3,4)\n");
		printf
		    ("                                                (A=F1, B=F2, C=F3, D=F4)\n");
		printf
		    ("                                :               (S=M1, T=M2, U=M3, V=M4)\n");
		printf("OPTIONAL ARGUMENTS:\n");
		printf
		    ("-t     -timeout                 : timout (in milliseconds)\n");
		printf
		    ("-s     -sample_rate    -rate    : set sample rate (eg 1e9 = 1GS/s)\n");
		printf
		    ("-n     -no_points      -points  : set minimum no of points\n");
		printf
		    ("-a     -averages       -aver    : set no of averages (<=0 means none)\n");
		printf
		    ("-sa    -seg_averages   -seg_aver: set no of averages (segmented mode)\n");
		printf
		    ("-seg   -segmented      -seq     : set no of segments\n\n");
		printf("OUTPUTS:\n");
		printf("filename.wf  : binary data of waveform\n");
		printf("filename.wfi : waveform information (text)\n\n");
		printf
		    ("In Matlab, use loadwf or similar to load and process the waveform\n\n");
		printf("EXAMPLE:\n");
		printf("%s -ip 128.243.74.78 -f test -c 2 -s 1e9\n", progname);
		exit(1);
	}

	f_wf = fopen(wfname, "w");
	if (f_wf > 0) {
		/* This utility illustrates the general idea behind how data is acquired.
		 * First we open the device, referenced by an IP address, and obtain
		 * a client id, and a link id, all contained in a "VXI11_CLINK" structure.  Each
		 * client can have more than one link. For simplicity we bundle them together. */

		if (lecroy_open(&clink, serverIP) != 0) {	// could also use "vxi11_open_device()"
			printf("Quitting...\n");
			exit(2);
		}

		/* Next we do some trivial initialisation. This sets LSB first, binary 
		 * word transfer, etc. A good opportunity to check we can talk to the scope. */
		if (lecroy_init(clink) != 0) {
			printf("Quitting...\n");
			exit(2);
		}

		/* This function can cope with s_rate and/or npoints <=0, in which case it just
		 * returns the actual sample rate */
		actual_s_rate =
		    lecroy_set_sample_rate(clink, s_rate, npoints, timeout);

		/* Check if we've specifically requested 8-bit transfers, if so, set it up */
		if (bytes_per_point == 1)
			vxi11_send_sprintf(clink, "COMM_FORMAT DEF9,BYTE,BIN");

		if (got_no_averages == TRUE) {
			chnl = lecroy_set_averages(clink, chnl, no_averages);
		}

		if (got_segmented_averages == TRUE) {
			chnl =
			    lecroy_set_segmented_averages(clink, chnl,
							  no_averages);
		}

		if (got_no_segments == TRUE)
			lecroy_set_segmented(clink, no_segments);

		/* Make sure the channel is turned on */
		lecroy_display_channel(clink, chnl, 1);

//              sprintf(cmd,"F1:INSP? 'WAVE_ARRAY_1'");
//              long_ret = lecroy_obtain_insp_long(clink, cmd, timeout);
//              printf("Returned value: %ld\n",long_ret);

//              sprintf(cmd,"F1:INSP? 'VERTICAL_GAIN'");
//              double_ret = lecroy_obtain_insp_double(clink, cmd, timeout);
//              printf("Returned value: %g\n",double_ret);

		buf_size =
		    lecroy_write_wfi_file(clink, wfiname, chnl, progname, 1,
					  bytes_per_point, timeout);
		actual_npoints = buf_size / (bytes_per_point * no_segments);
		printf
		    ("Bytes per trace (channel %c): %ld; pts/trace: %ld; sample rate: %gSa/S\n",
		     chnl, buf_size, actual_npoints, actual_s_rate);
		buf = new char[buf_size];
		bytes_returned =
		    lecroy_get_data(clink, chnl, clear_sweeps, buf, buf_size,
				    got_no_segments, timeout);
		//lecroy_set_for_norm(clink);
		/* Check if we've specifically requested 8-bit transfers, if so, set back to 16 */
		if (bytes_per_point == 1)
			vxi11_send_sprintf(clink, "COMM_FORMAT DEF9,WORD,BIN");
		fwrite(buf, sizeof(char), buf_size, f_wf);
//              fwrite(buf, sizeof(char), bytes_returned, f_wf);
		fclose(f_wf);
		delete[]buf;

		/* Finally we sever the link to the client. */
		lecroy_close(clink, serverIP);	// could also use "vxi11_close_device()"
	} else {
		printf("error: could not open file for writing, quitting...\n");
		exit(3);
	}
}

/* Extra notes:
 * Another way of going through the acquisition loop, which is more relevant to
 * acquiring multiple waveforms, is as follows:

	lecroy_open(&clink, serverIP);
	lecroy_init(clink);
	lecroy_set_for_capture(clink, s_rate, npoints, timeout);
	buf_size = lecroy_calculate_no_of_bytes(clink, chnl, timeout); // performs :DIG
	buf = new char[buf_size];
	count=0;
	do { // note for first acquisition no :DIG is done, then it is for each one after
		bytes_returned = lecroy_get_data(clink, chnl, count++, buf, buf_size, timeout);
		<append trace to wf file>;
		} while (<some condition>);

	// Note we are sending the function the buf_size instead of the chnl; no digitisation
	// needs to be done (however you must take care that the scope has not been adjusted
	// since the last acquisition)
	ret = lecroy_write_wfi_file(clink, wfiname, buf_size, progname, count, timeout);
	lecroy_set_for_auto(clink);
	delete[] buf;
	lecroy_close(serverIP,clink);
 */

/* string compare (sc) function for parsing... ignore */
BOOL sc(const char *con, const char *var)
{
	if (strcmp(con, var) == 0) {
		return TRUE;
	}
	return FALSE;
}
