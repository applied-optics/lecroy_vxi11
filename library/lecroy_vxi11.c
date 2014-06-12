/* lecroy_vxi11.c
 * Copyright (C) 2010 Steve D. Sharples
 *
 * User library of useful functions for talking to LeCroy
 * oscilloscopes using the VXI11 protocol, for Linux. You will also need the
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

/* This really is just a wrapper. Only here because folk might be uncomfortable
 * using commands from the vxi11_vxi11 library directly! */
int lecroy_open(VXI11_CLINK ** clink, const char *ip)
{
	return vxi11_open_device(clink, ip, NULL);
}

/* Again, just a wrapper */
int lecroy_close(VXI11_CLINK * clink, const char *ip)
{
	vxi11_send_str(clink, "MSG");	/* remove message on bottom of screen */
	return vxi11_close_device(clink, ip);
}

/* Set up some fundamental settings for data transfer. It's possible
 * (although not certain) that some or all of these would be reset after
 * a system reset. It's a very tiny overhead right at the beginning of your
 * acquisition that's performed just once. */
int lecroy_init(VXI11_CLINK * clink)
{
	char cmd[256];
	int ret;
	/* Sets DEF9 (defines arbitrary data block header), 16-bit data
	 * (needed when averaging), binary format (more efficient than ascii) */
	ret = vxi11_send_str(clink, "COMM_FORMAT DEF9,WORD,BIN");
	if (ret < 0) {
		printf
		    ("ERROR in lecroy_init, could not send very first command.\n");
		return ret;
	}
	vxi11_send_str(clink, "COMM_HEADER OFF");	/* much easier parsing of responses */
	vxi11_send_str(clink, "COMM_ORDER LO");	/* sets endian-ness to Intel, ie LSB, MSB */
	vxi11_send_str(clink, "MSG \"STEVE'S LINUX VXI-11 LECROY DRIVER\"");	/* message on bottom of screen */
	return 0;
}

/* Annoyingly, INSP? queries don't just return a number, they also return the
 * paramater name you want to inspect, followed by spaces, a ":", then a space.
 * In order to parse these, we look for the ":" */
long lecroy_obtain_insp_long(VXI11_CLINK * clink, const char *cmd,
			     unsigned long timeout)
{
	char buf[256];		/* 256=arbitrary length...  */
	int l = 0;
	memset(buf, 0, 256);
	if (vxi11_send_and_receive(clink, cmd, buf, 256, timeout) != 0) {
		printf("Error: lecroy_obtain_insp_long returning 0\n");
		return 0;
	}
	while ((buf[l] != ':') && (l < 256))
		l++;
	if (l == 256) {
		printf
		    ("Error: problem parsing returned string in lecroy_obtain_insp_long. String:\n");
		printf("%s\nReturning 0\n", buf);
		return 0;
	}
	return strtol(buf + l + 2, (char **)NULL, 10);
}

/* Annoyingly, INSP? queries don't just return a number, they also return the
 * paramater name you want to inspect, followed by spaces, a ":", then a space.
 * In order to parse these, we look for the ":" */
double lecroy_obtain_insp_double(VXI11_CLINK * clink, const char *cmd,
				 unsigned long timeout)
{
	char buf[256];		/* 256=arbitrary length...  */
	int l = 0;
	memset(buf, 0, 256);
	if (vxi11_send_and_receive(clink, cmd, buf, 256, timeout) != 0) {
		printf("Error: lecroy_obtain_insp_double returning 0.0\n");
		return 0.0;
	}
	while ((buf[l] != ':') && (l < 256))
		l++;
	if (l == 256) {
		printf
		    ("Error: problem parsing returned string in lecroy_obtain_insp_double. String:\n");
		printf("%s\nReturning 0.0\n", buf);
		return 0.0;
	}
	return strtod(buf + l + 2, (char **)NULL);
}

/* Why can't everything be this easy? */
/* ...
 * Famous last words... turns out you have to ask twice, as if you've recently
 * changed the sample rate then the changes don't propagate through unless you've
 * asked a couple of times. Way to go, lecroy! */
long lecroy_calculate_no_of_bytes(VXI11_CLINK * clink, char chan,
				  unsigned long timeout)
{
	char cmd[256];
	char source[20];
	lecroy_scope_channel_str(chan, source);
	sprintf(cmd, "%s:INSP? WAVE_ARRAY_1", source);
	lecroy_obtain_insp_long(clink, cmd, timeout);
	return lecroy_obtain_insp_long(clink, cmd, timeout);
}

/* This version of the function, rather than using the "INSP? WAVE_ARRAY_1" query,
 * uses VBS commands. The difference is that the VBS responses get updated the moment
 * a setting (like the timebase) is set. However, there is no simple command that
 * gives you they same number as the actual number of bytes returned, there is usually
 * an extra single byte on the maths channels, and an extra 2 bytes on the acquisition
 * channels. */
long lecroy_calculate_no_of_bytes_from_vbs(VXI11_CLINK * clink, char chan)
{
	int bytes_per_point;
	long no_of_points, no_of_segments, no_of_bytes;

	no_of_points =
	    vxi11_obtain_long_value(clink,
				    "VBS? 'Return=app.Acquisition.Horizontal.NumPoints'");
	bytes_per_point = lecroy_get_bytes_per_point(clink);

	// Check whether we've been passed a maths channel or not. If so, then it
	// doesn't matter whether we're in segmented mode or not, as we will be taking
	// the average of all the segments, in which case the number of segments is
	// irrelevant as it will not affect the number of bytes. Also, maths channels
	// return an array which is 1+app.Acquisition.Horizontal.Points, acquisition
	// channels return 2+app.Acquisition.Horizontal.Points (x no of bytes per point)
	if (lecroy_is_maths_chan(chan) == 1) {
		no_of_bytes = bytes_per_point * (1 + no_of_points);
	} else {
		no_of_segments = lecroy_get_segmented(clink);	// returns 1 if not in segmented mode
		no_of_bytes =
		    bytes_per_point * no_of_segments * (2 + no_of_points);
	}
	return no_of_bytes;
}

/* This function reads a response in the form of a definite-length block, such
 * as when you ask for waveform data. The data is returned in the following
 * format:
 *   DATA_ARRAY_1,#9000001000<1000 bytes of data>
 *   \___________/||\_______/
 *         |      ||    |
 *         |      ||    \---- number of bytes of data
 *         |      |\--------- number of digits that follow (in this case 9, with leading 0's)
 *         |      \---------- always starts with #
 *         \----------------- whatever array you ask for, if you ask for "DAT1" you get "DAT1,#9...."
 */
long lecroy_receive_data_block(VXI11_CLINK * clink, char *buffer,
			       unsigned long len, unsigned long timeout)
{
/* I'm not sure what the maximum length of this header is, I'll assume it's
 * 24 (DATA_ARRAY_1,#9 + 9 digits) */
	unsigned long necessary_buffer_size;
	char *in_buffer;
	int ret;
	int ndigits;
	unsigned long returned_bytes;
	int l = 0;
	char scan_cmd[20];
	necessary_buffer_size = len + 25;
	in_buffer = new char[necessary_buffer_size];
	ret =
	    vxi11_receive_timeout(clink, in_buffer, necessary_buffer_size,
				  timeout);
	if (ret < 0)
		return ret;
	while ((in_buffer[l] != '#') && (l < 25))
		l++;
	if (in_buffer[l] != '#') {
		printf
		    ("lecroy_user: data block error: data block does not begin with '#'\n");
		printf("First 25 characters received were: '");
		for (l = 0; l < 25; l++) {
			printf("%c", in_buffer[l]);
		}
		printf("'\n");
		return -3;
	}

	/* first find out how many digits */
	sscanf(in_buffer + l, "#%1d", &ndigits);
	/* some instruments, if there is a problem acquiring the data, return only "#0" */
	if (ndigits > 0) {
		/* now that we know, we can convert the next <ndigits> bytes into an unsigned long */
		sprintf(scan_cmd, "#%%1d%%%dlu", ndigits);
		sscanf(in_buffer + l, scan_cmd, &ndigits, &returned_bytes);
		memcpy(buffer, in_buffer + (ndigits + l + 2), returned_bytes);
		delete[]in_buffer;
		return (long)returned_bytes;
	} else
		return 0;
}

/* Wrapper. Most times we want to arm and wait... unless we've already set this up and returned
 * control to some other process (eg moving a motorised stage), and all we want to do now is
 * grab the data */
long lecroy_get_data(VXI11_CLINK * clink, char chan, int clear_sweeps,
		     char *buf, unsigned long buf_len, unsigned long timeout)
{
	return lecroy_get_data(clink, chan, clear_sweeps, buf, buf_len, 1,
			       timeout);
}

/* The "get_data" function has to cope with grabbing the data under a variety
 * of acquisition conditions. For data from channels 1-4 the general idea is to
 * ARM (single acquisition), WAIT, then *OPC? (OPeration Complete?). For data
 * from the maths channels (averaging) you tend to want the scope in norm mode,
 * issue a CLSW request then wait for the registers to indicate that averaging
 * has finished. Using a maths channel to take the average of a sequence (using
 * segmented memory) involves a combination of the two: issuing ARM and CLSW,
 * and checking that averaging has completed before grabbing the data.
 * The additional complication is that if you are grabbing data from more than 
 * one channel, if you want the data to be synchronous, you must avoid issuing 
 * either an ARM or a CLSW command. Hence the clear_sweeps and arm_and_wait
 * flag arguments.
 *
 * Summary of required settings (X = "don't care"):
 * Job				| New acq?	| Channel	| clear_sweeps	| arm_and_wait
 * -------------------------------------------------------------------------------------------
 * Realtime acquisition		| Yes		| 1-4		| X		| 1
 *    "          "		| No		| 1-4		| X		| 0
 * Segmented acquisition	| Yes		| 1-4		| X		| 1
 *     "          "		| No		| 1-4		| X		| 0
 * Averages			| Yes		| A-D		| 1		| 0
 *    "				| No		| A-D		| 0		| 0
 * Segmented averages		| Yes		| A-D		| 1		| 1
 *     "        "		| No		| A-D		| 0		| 0
 *
 */
long lecroy_get_data(VXI11_CLINK * clink, char chan, int clear_sweeps,
		     char *buf, unsigned long buf_len, int arm_and_wait,
		     unsigned long timeout)
{
	char cmd[256];
	char source[20];
	long ret;
	int is_maths_chan;

	is_maths_chan = lecroy_is_maths_chan(chan);

	if ((is_maths_chan == 1) && (clear_sweeps == 1))
		lecroy_clear_sweeps(clink);
	if (arm_and_wait == 1)
		vxi11_send_str(clink, "ARM;WAIT");
	if ((arm_and_wait == 1) || (is_maths_chan == 0)) {
		ret = vxi11_obtain_long_value_timeout(clink, "*OPC?", timeout);
		if (ret != 1) {
			printf
			    ("lecroy_get_data: error, *OPC? did not return 1\n");
			return 0;
		}
	}
	if ((is_maths_chan == 1) && (clear_sweeps == 1))
		lecroy_wait_all_averages(clink, timeout);
	lecroy_scope_channel_str(chan, source);
	sprintf(cmd, "%s:WF? DAT1", source);
	vxi11_send_str(clink, cmd);
	return lecroy_receive_data_block(clink, buf, buf_len, timeout);
}

void lecroy_set_for_auto(VXI11_CLINK * clink)
{
	vxi11_send_str(clink, "TRMD AUTO");
}

void lecroy_set_for_norm(VXI11_CLINK * clink)
{
	vxi11_send_str(clink, "TRMD NORM");
}

void lecroy_single(VXI11_CLINK * clink)
{
	vxi11_send_str(clink, "ARM;WAIT");
}

void lecroy_stop(VXI11_CLINK * clink)
{
	vxi11_send_str(clink, "STOP");
}

int lecroy_get_bytes_per_point(VXI11_CLINK * clink)
{
	char buf[256];
	memset(buf, 0, 256);
	vxi11_send_and_receive(clink, "COMM_FORMAT?", buf, 256,
			       VXI11_READ_TIMEOUT);
	if (strstr(buf, "WORD") != NULL)
		return 2;
	else
		return 1;
}

void lecroy_clear_sweeps(VXI11_CLINK * clink)
{
	/* Needs to send an INR? query, in order to reset the registers
	 * (we don't care what the value is) */
	vxi11_obtain_long_value(clink, "INR?");
	vxi11_send_str(clink, "CLSW");
}

int lecroy_wait_all_averages(VXI11_CLINK * clink, unsigned long timeout)
{
	char cmd[256];
	char buf[256];
	int chan_on[4];
	int mask;
	int l;
	int inr = 0;
	int old_inr = 0;
	int test = 0;
	/* Go through all maths channels, see if they're turned on or not */
	for (l = 0; l < 4; l++) {
		sprintf(cmd, "F%d:TRACE?", l + 1);
		memset(buf, 0, 256);
		vxi11_send_and_receive(clink, cmd, buf, 256, timeout);
		if (strstr(buf, "ON") != NULL) {
			chan_on[l] = 1;
		} else {
			chan_on[l] = 0;
		}
	}

	/* Now investigate which maths channels (that are turned on) are averaging */
	l = 0;
	while (l < 4) {
		if (chan_on[l] == 1) {
			sprintf(cmd, "F%d:DEF?", l + 1);
			memset(buf, 0, 256);
			vxi11_send_and_receive(clink, cmd, buf, 256, timeout);
			if (strstr(buf, "AVG") == NULL) {
				chan_on[l] = 0;
			}
		}
		l++;
	}
	/* make the appropriate mask */
	mask =
	    (chan_on[0] * 256) + (chan_on[1] * 512) + (chan_on[2] * 1024) +
	    (chan_on[3] * 2048);

	while (test == 0) {
		inr =
		    (int)vxi11_obtain_long_value_timeout(clink, "INR?",
							 timeout);
		old_inr = old_inr | inr;
		if ((old_inr & mask) == mask)
			test = 1;
		else
			test = 0;
	}
	return 0;
}

/* This wrapper first calculates the number of bytes, then passes this information on to the main function */
long lecroy_write_wfi_file(VXI11_CLINK * clink, char *wfiname, char chan,
			   char *captured_by, int no_of_traces,
			   int bytes_per_point, unsigned long timeout)
{
	return lecroy_write_wfi_file(clink, wfiname, chan, captured_by,
				     no_of_traces, bytes_per_point,
				     lecroy_calculate_no_of_bytes(clink, chan,
								  timeout),
				     timeout);
}

/* This wrapper does not force the value of voffset (and is the usual behaviour). You might want to force voffset,
 * typically making it zero, if prior to saving the waveform to the disk you have subtracted one waveform from
 * another, using the lecroy_subtract_char_arrays() fn. */
long lecroy_write_wfi_file(VXI11_CLINK * clink, char *wfiname, char chan,
			   char *captured_by, int no_of_traces,
			   int bytes_per_point, long no_of_bytes,
			   unsigned long timeout)
{
	return lecroy_write_wfi_file(clink, wfiname, chan, captured_by,
				     no_of_traces, bytes_per_point, no_of_bytes,
				     timeout, 0, 0);
}

long lecroy_write_wfi_file(VXI11_CLINK * clink, char *wfiname, char chan,
			   char *captured_by, int no_of_traces,
			   int bytes_per_point, long no_of_bytes,
			   unsigned long timeout, int force_voffset,
			   double voffset)
{
	FILE *wfi;
	double vgain, hinterval, hoffset;
	int ret;
	char cmd[256];
	char source[20];
	int no_of_segments;

	lecroy_scope_channel_str(chan, source);

	// VBS commands return quicker than INSP? commands, so where there is a
	// direct equivalent they are used instead to speed things up

//      sprintf(cmd, "%s:INSP? HORIZ_INTERVAL", source);
//      hinterval = lecroy_obtain_insp_double(clink, cmd, timeout);
	sprintf(cmd, "VBS? 'Return=app.Acquisition.Horizontal.TimePerPoint'");
	hinterval = vxi11_obtain_double_value_timeout(clink, cmd, timeout);
	sprintf(cmd, "%s:INSP? HORIZ_OFFSET", source);
	hoffset = lecroy_obtain_insp_double(clink, cmd, timeout);
	sprintf(cmd, "%s:INSP? VERTICAL_GAIN", source);
	vgain = lecroy_obtain_insp_double(clink, cmd, timeout);
	if (force_voffset == 0) {
		sprintf(cmd, "%s:INSP? VERTICAL_OFFSET", source);
		voffset = lecroy_obtain_insp_double(clink, cmd, timeout);
	}
	//sprintf(cmd, "VBS? 'Return=app.Acquisition.%s.VerOffset'", source); // commented out as this doesn't work for maths channels
	//voffset = vxi11_obtain_double_value_timeout(clink, cmd, timeout);

	if (lecroy_is_maths_chan(chan) == 0) {
		no_of_segments = lecroy_get_segmented(clink);	// returns 1 if not in segmented mode
	} else {
		no_of_segments = 1;
	}

	wfi = fopen(wfiname, "w");
	if (wfi > 0) {
		fprintf(wfi, "%% %s\n", wfiname);
		fprintf(wfi, "%% Waveform captured using %s\n\n", captured_by);
		if (no_of_segments == 0) {
			fprintf(wfi, "%% Number of bytes:\n%d\n\n",
				no_of_bytes);
		} else {
			fprintf(wfi, "%% Number of bytes:\n%d\n\n",
				(no_of_bytes / no_of_segments));
		}
		fprintf(wfi, "%% Vertical gain:\n%g\n\n", vgain);
		fprintf(wfi, "%% Vertical offset:\n%g\n\n", voffset);
		fprintf(wfi, "%% Horizontal interval:\n%g\n\n", hinterval);
		fprintf(wfi, "%% Horizontal offset:\n%g\n\n", hoffset);
		if (no_of_segments == 0) {
			fprintf(wfi, "%% Number of traces:\n%d\n\n",
				no_of_traces);
		} else {
			fprintf(wfi, "%% Number of traces:\n%d\n\n",
				(no_of_traces * no_of_segments));
		}
		fprintf(wfi, "%% Number of bytes per data-point:\n%d\n\n",
			bytes_per_point);
		fprintf(wfi,
			"%% Keep all datapoints (0 or missing knocks off 1 point, legacy lecroy):\n%d\n\n",
			1);
		fclose(wfi);
	} else {
		printf
		    ("error: lecroy_write_wfi_file: could not open %s for writing\n",
		     wfiname);
		return -1;
	}
	return no_of_bytes;
}

/* Here, "chan" is either one of the acquisistion channels (1-4), or one of
 * the maths channels (A-D). In setting the number of averages, the 
 * nomenclature forces you to state not only the maths channel (quite
 * reasonably) but also the acquisition source. To save the end user some
 * complexity (having to state both the maths and acquisition channels)
 * this function takes either of them, and chooses the appropriate twin,
 * using "lecroy_relate_function_to_source(chan)". So A<->1, and 4<->D etc.
 *
 * As special cases, if no_averages is 0 or 1, then the maths channel is
 * turned off and the acquisition channel (1-4) is returned.
 */

char lecroy_set_averages(VXI11_CLINK * clink, char chan, int no_averages)
{
	char maths_chan;
	char maths_chan_str[20];
	char source[20];
	char cmd[256];

	if (lecroy_is_maths_chan(chan) == 0) {
		maths_chan = lecroy_relate_function_to_source(chan);
	} else {
		maths_chan = chan;
		chan = lecroy_relate_function_to_source(maths_chan);
	}
	if (no_averages > 1) {
		lecroy_scope_channel_str(maths_chan, maths_chan_str);
		lecroy_scope_channel_str(chan, source);
		sprintf(cmd,
			"%s:DEF EQN, 'AVG(%s)',AVERAGETYPE,SUMMED,SWEEPS,%d SWEEP",
			maths_chan_str, source, no_averages);
		vxi11_send_str(clink, cmd);
		lecroy_display_channel(clink, maths_chan, 1);
		return maths_chan;
	} else {
		lecroy_display_channel(clink, maths_chan, 0);
		lecroy_display_channel(clink, chan, 1);
		return chan;
	}
}

int lecroy_get_averages(VXI11_CLINK * clink, char chan)
{
	char maths_chan;
	char maths_chan_str[20];
	char source[20];
	char cmd[256];

	if (lecroy_is_maths_chan(chan) == 0) {
		maths_chan = lecroy_relate_function_to_source(chan);
	} else {
		maths_chan = chan;
		chan = lecroy_relate_function_to_source(maths_chan);
	}
	lecroy_scope_channel_str(maths_chan, maths_chan_str);
	sprintf(cmd, "VBS? 'Return=app.Math.%s.Operator1Setup.Sweeps'",
		maths_chan_str);
	return (int)vxi11_obtain_long_value(clink, cmd);
}

char lecroy_set_segmented_averages(VXI11_CLINK * clink, char chan,
				   int no_averages)
{
	return lecroy_set_segmented_averages(clink, chan, no_averages, 1);
}

char lecroy_set_segmented_averages(VXI11_CLINK * clink, char chan,
				   int no_averages, int arm)
{
	char maths_chan;
	int actual_no_averages;

	actual_no_averages = lecroy_set_segmented(clink, no_averages, arm);
	maths_chan = lecroy_set_averages(clink, chan, actual_no_averages);
	return maths_chan;
}

int lecroy_get_segmented_status(VXI11_CLINK * clink)
{
	char buf[256];
	int segmented_status;

	vxi11_send_and_receive(clink,
			       "VBS? 'Return=app.Acquisition.Horizontal.SampleMode'",
			       buf, 256, VXI11_READ_TIMEOUT);
	segmented_status = strncmp(buf, "Sequence", 8);
	if (segmented_status == 0)
		return 1;
	else
		return 0;
}

/* Checks to see if we are in segmented mode, if we are then return the number of
 * segments (minimum = 2), if not then return 1 */
int lecroy_get_segmented(VXI11_CLINK * clink)
{
	if (lecroy_get_segmented_status(clink) == 1) {
		return (int)vxi11_obtain_long_value(clink,
						    "VBS? 'Return=app.Acquisition.Horizontal.NumSegments'");
	} else {
		return 1;
	}
}

int lecroy_set_segmented(VXI11_CLINK * clink, int no_segments)
{
	return lecroy_set_segmented(clink, no_segments, 1);
}

int lecroy_set_segmented(VXI11_CLINK * clink, int no_segments, int arm)
{
	char cmd[256];
	int actual_no_segments;

	if (arm == 0)
		sprintf(cmd, "SEQ ON,%d", no_segments);
	else
		sprintf(cmd, "SEQ ON,%d;ARM", no_segments);
	vxi11_send_str(clink, cmd);
	actual_no_segments = lecroy_get_segmented(clink);
	return actual_no_segments;
}

/* Turns a channel on or off (pass "1" for on, "0" for off)*/
int lecroy_display_channel(VXI11_CLINK * clink, char chan, int on_or_off)
{
	char source[20];
	char cmd[256];

	memset(source, 0, 20);
	lecroy_scope_channel_str(chan, source);
	if (on_or_off == 0)
		sprintf(cmd, "%s:TRACE OFF", source);
	else
		sprintf(cmd, "%s:TRACE ON", source);
	return vxi11_send_str(clink, cmd);
}

/* Set the sample rate, either directly or by inference by the number of points specified.
 * If both are specified, then sample rate takes precedence. Returns the actual sample rate. */
double lecroy_set_sample_rate(VXI11_CLINK * clink, double s_rate, long n_points,
			      long timeout)
{
	double actual_s_rate;
	double expected_s_rate;
	double time_range;
	char cmd[256];
	if (n_points > 0) {
		time_range =
		    vxi11_obtain_double_value_timeout(clink, "TIME_DIV?",
						      timeout) * 10.0;
		expected_s_rate = (double)n_points / time_range;
		sprintf(cmd, "VBS 'app.Acquisition.Horizontal.SampleRate=%g'",
			expected_s_rate);
		vxi11_send_str(clink, cmd);
	}

	if (s_rate > 0) {
		sprintf(cmd, "VBS 'app.Acquisition.Horizontal.SampleRate=%g'",
			s_rate);
		vxi11_send_str(clink, cmd);
	}
	actual_s_rate =
	    vxi11_obtain_double_value_timeout(clink,
					      "VBS? 'Return=app.Acquisition.Horizontal.SampleRate'",
					      timeout);
	return actual_s_rate;
}

/* Sets the trigger source channel (sets it to be an EDGE trigger too) */
int lecroy_set_trigger_channel(VXI11_CLINK * clink, char chan)
{
	char source[20];
	char cmd[256];

	memset(source, 0, 20);
	lecroy_scope_channel_str(chan, source);
	sprintf(cmd, "TRSE EDGE,SR,%s", source);
	return vxi11_send_str(clink, cmd);
}

/* In the library we tend to use a single char to denote a channel. This works
 * fairly well, and is rooted in the old days when LeCroy called their maths
 * channels A, B, C, and D. So channel 'A' is actually maths function "F1". */
void lecroy_scope_channel_str(char chan, char *source)
{
	switch (chan) {
	case 'A':
	case 'a':
		strcpy(source, "F1");
		break;
	case 'B':
	case 'b':
		strcpy(source, "F2");
		break;
	case 'C':
	case 'c':
		strcpy(source, "F3");
		break;
	case 'D':
	case 'd':
		strcpy(source, "F4");
		break;
	case 'E':
	case 'e':
		strcpy(source, "F5");
		break;
	case 'F':
	case 'f':
		strcpy(source, "F6");
		break;
	case 'G':
	case 'g':
		strcpy(source, "F7");
		break;
	case 'H':
	case 'h':
		strcpy(source, "F8");
		break;
	case 'S':
	case 's':
		strcpy(source, "M1");
		break;
	case 'T':
	case 't':
		strcpy(source, "M2");
		break;
	case 'U':
	case 'u':
		strcpy(source, "M3");
		break;
	case 'V':
	case 'v':
		strcpy(source, "M4");
		break;
	case 'W':
	case 'w':
		strcpy(source, "M5");
		break;
	case 'X':
	case 'x':
		strcpy(source, "M6");
		break;
	case 'Y':
	case 'y':
		strcpy(source, "M7");
		break;
	case 'Z':
	case 'z':
		strcpy(source, "M8");
		break;
	case '1':
		strcpy(source, "C1");
		break;
	case '2':
		strcpy(source, "C2");
		break;
	case '3':
		strcpy(source, "C3");
		break;
	case '4':
		strcpy(source, "C4");
		break;
	default:
		printf("error: unknown channel '%c', using channel 1\n", chan);
		strcpy(source, "C1");
		break;
	}
}

/* This function finds the relates maths function channels to their equivalent
 * acquisition channels, ie if you pass 'A' then you get returned '1'; if you
 * pass '3' you get returned 'C'. This function is used when setting up maths
 * function parameters, for instance, setting the number of averages. Not only
 * do you need to specify the maths function channel itself, unfortunately you
 * must also specify the acquisition channel it works on.
 *
 * This function allows a certain amount of flexibility in the way the user
 * refers to channels. For instance, they may say "I want 1000 averages of
 * channel 1," in which case maths channel A (F1) is set up to do the averaging.
 * Alternatively, the user may have already set up channel A (F1) to do
 * averaging (hopefully of channel 1!) and may wish to change the number of
 * averages. The LeCroy programming commands do not allow us to say, "Set the
 * number of averages on maths channel F1 to 1000", it only allows us to say,
 * "Set the source of F1 to be C1, and set the number of averages to 1000."
 * This function allows the library to make a sensible guess about the channel
 * source for a given maths function channel. */
char lecroy_relate_function_to_source(char chan)
{
	switch (chan) {
	case 'A':
	case 'a':
		return '1';
		break;
	case 'B':
	case 'b':
		return '2';
		break;
	case 'C':
	case 'c':
		return '3';
		break;
	case 'D':
	case 'd':
		return '4';
		break;
	case 'E':
	case 'e':
	case 'F':
	case 'f':
	case 'G':
	case 'g':
	case 'H':
	case 'h':
		printf
		    ("error: Functions F5-F8 (E-H) don't have an associated channel, using channel 1\n");
		return '1';
	case '1':
		return 'A';
		break;
	case '2':
		return 'B';
		break;
	case '3':
		return 'C';
		break;
	case '4':
		return 'D';
		break;
	default:
		printf("error: unknown channel '%c', using channel 1\n", chan);
		return '1';
		break;
	}
}

int lecroy_is_maths_chan(char chan)
{
	if (chan == '1' || chan == '2' || chan == '3' || chan == '4')
		return 0;
	else
		return 1;
}

/* The following function takes data which as been acquired from the scope as a
 * bunch of segmented traces, then averages the traces and puts the averages 
 * into "out_buf". Although "in_buf" and "out_buf" are (unsigned) chars, the
 * stream of bytes contained within them represent signed chars (if
 * bytes_per_point == 1) or signed shorts (if bytes_per_point == 2). The order
 * of the 16-bit words that are sent from the scope to the PC is determined by
 * the "COMM_ORDER LO" command (issued in the lecroy_init() function, and which
 * the scope should remember unless it's had a factory reset), i.e. little
 * endian: LSB then MSB. Note that if you elect to use 16 bit transfers and the
 * data contains only 8 bits of information, i.e. traces from channels 1--4 
 * rather than maths channels, then all the LSBs are zeros.
 *
 * The function does not talk to the scope in any way, it purely moves data
 * around. It could go in any library, I happen to need it for this one.
 */
long lecroy_average_segmented_data(char *in_buf, unsigned long in_buf_len,
				   char *out_buf, unsigned long out_buf_len,
				   int no_of_segments, int bytes_per_point)
{
	int i, j, points_per_trace;
// need a temporary buffer to store the running total in, this needs to be >16 bits long, an int will do
	int *int_buf;
	short *short_in_buf, *short_out_buf;
	signed char *signed_char_in_buf, *signed_char_out_buf;

	points_per_trace =
	    (int)(in_buf_len / (bytes_per_point * no_of_segments));

	int_buf = new int[points_per_trace];
	// Set the VALUES to be zero (memset with zeros does NOT work!)
	for (i = 0; i < points_per_trace; i++)
		int_buf[i] = 0;

	/* We average a stack of segmented traces by adding up the values of 
	 * each point, then dividing the sum (of each point) by the number of
	 * segments. In order to do this, the PC needs to know what sort of
	 * numbers the bytes or words represent. At the moment they are stored in
	 * an array of (unsigned) chars; however we know that for 8-bit transfers
	 * these correspond to int8 (signed char) or int16 (short int) numbers.
	 * How to tell the PC this? Well I tried converting from 2's compliment
	 * back to the numbers they represent and storing them in new arrays, but
	 * it was messy. It turns out the easiest way is to make a new array of
	 * the right sort (signed char or short) and right size, and use memcpy()
	 * to simply move the bytes over to the new array, and when they are 
	 * read from the array they are interpreted correctly. We also need to
	 * do this once we've done the averaging. The flow chart is as follows:
	 *
	 *                      (unsigned) char
	 *                             |
	 *                           memcpy
	 *                       ______|______
	 *                      /             \
	 *     8-bit---> signed char       short int <---16-bit
	 *                      \             /
	 *                       \           /
	 *            int (signed 32 bit) to do maths
	 *                       /           \
	 *                      /             \
	 *     8-bit---> signed char       short int <---16-bit
	 *                      \             /
	 *                       -------------
	 *                             |
	 *                           memcpy
	 *                             |
	 *                      (unsigned) char
	 *
	 */
	if (bytes_per_point == 1) {
		signed_char_in_buf = new signed char[in_buf_len];
		signed_char_out_buf = new signed char[points_per_trace];
		memcpy(signed_char_in_buf, in_buf, in_buf_len);
		// Go through each point in the trace. This isn't the order they're stored in
		// in the array, but it's the most logical way of doing the maths.
		for (i = 0; i < points_per_trace; i++) {
			for (j = 0; j < no_of_segments; j++) {	// running everage
				int_buf[i] +=
				    signed_char_in_buf[(j * points_per_trace) +
						       i];
			}
			int_buf[i] /= no_of_segments;
			if ((int_buf[i] > 127) || (int_buf[i] < -128)) {
				printf("int_buf[%d] = %d\n", i, int_buf[i]);
			}
			signed_char_out_buf[i] = (signed char)int_buf[i];
		}
		memcpy(out_buf, signed_char_out_buf, out_buf_len);
		delete[]signed_char_out_buf;
		delete[]signed_char_in_buf;
	} else {
		short_in_buf = new short[in_buf_len / bytes_per_point];
		short_out_buf = new short[points_per_trace];
		memcpy(short_in_buf, in_buf, in_buf_len);
		for (i = 0; i < points_per_trace; i++) {
			for (j = 0; j < no_of_segments; j++) {
				int_buf[i] +=
				    short_in_buf[(j * points_per_trace) + i];
			}
			int_buf[i] /= no_of_segments;
			if ((int_buf[i] > 32767) || (int_buf[i] < -32768)) {
				printf("int_buf[%d] = %d\n", i, int_buf[i]);
			}
			short_out_buf[i] = (short)int_buf[i];
		}
		memcpy(out_buf, short_out_buf, out_buf_len);
		delete[]short_out_buf;
		delete[]short_in_buf;
	}
	delete[]int_buf;
}

/* Generic functions to subtract two arrays: A-B = OUT. A, B and OUT can be any
 * mixture of 8-bit or 16-bit signed integers, but as they are passed to the
 * function they are (unsigned) chars. See fn above for more info on conversion.
 */
long lecroy_subtract_char_arrays(char *in_buf_a, char *in_buf_b, char *out_buf,
				 int bytes_per_point_a, int bytes_per_point_b,
				 int bytes_per_point_out, int points_per_trace)
{
	int i, j;
	int in_buf_len_a, in_buf_len_b, out_buf_len;
// need a temporary buffer to store the running total in, this needs to be >16 bits long, an int will do
	int *int_buf;
	short *short_in_buf_a, *short_in_buf_b, *short_out_buf;
	signed char *signed_char_tmp, *signed_char_in_buf, *signed_char_out_buf;

	in_buf_len_a = points_per_trace * bytes_per_point_a;
	in_buf_len_b = points_per_trace * bytes_per_point_b;
	out_buf_len = points_per_trace * bytes_per_point_out;

	int_buf = new int[points_per_trace];
	short_in_buf_a = new short[points_per_trace];
	short_in_buf_b = new short[points_per_trace];
	short_out_buf = new short[points_per_trace];

	signed_char_tmp = new signed char[2 * points_per_trace];
	signed_char_in_buf = new signed char[points_per_trace];
	signed_char_out_buf = new signed char[points_per_trace];

	if (bytes_per_point_a == 1) {
		memcpy(signed_char_in_buf, in_buf_a, in_buf_len_a);	// memcpy "does conversion" unsigned char to signed char
		for (i = 0; i < points_per_trace; i++) {
			signed_char_tmp[2 * i] = 0;	// LSB = 0
			signed_char_tmp[(2 * i) + 1] = signed_char_in_buf[i];	// MSB = 8-bit data (ie "convert" to 16 bit)
		}
		memcpy(short_in_buf_a, signed_char_tmp, 2 * points_per_trace);	// memcpy "does conversion" to short int
	} else {
		memcpy(short_in_buf_a, in_buf_a, in_buf_len_a);
	}
	for (i = 0; i < points_per_trace; i++) {
		int_buf[i] = (int)short_in_buf_a[i];	// copy A into int_buf
	}

	if (bytes_per_point_b == 1) {
		memcpy(signed_char_in_buf, in_buf_b, in_buf_len_b);
		for (i = 0; i < points_per_trace; i++) {
			signed_char_tmp[2 * i] = 0;
			signed_char_tmp[(2 * i) + 1] = signed_char_in_buf[i];
		}
		memcpy(short_in_buf_b, signed_char_tmp, 2 * points_per_trace);
	} else {
		memcpy(short_in_buf_b, in_buf_b, in_buf_len_b);
	}
	for (i = 0; i < points_per_trace; i++) {
		int_buf[i] -= (int)short_in_buf_b[i];	// subtract B from int_buf (containing A)
		if (int_buf[i] < -32768)
			int_buf[i] = -32768;	// Limit the range of numbers to those...
		if (int_buf[i] > 32767)
			int_buf[i] = 32767;	// ...capable of being stored in a short int
		short_out_buf[i] = (short)int_buf[i];
	}

	if (bytes_per_point_out == 1) {
		memcpy(signed_char_tmp, short_out_buf, 2 * points_per_trace);	// copy all bytes to a temporary signed char array. The LSB bytes are nonsense.
		for (i = 0; i < points_per_trace; i++) {
			signed_char_out_buf[i] = signed_char_tmp[(2 * i) + 1];	// MSB only (throw away LSB)
		}
		memcpy(out_buf, signed_char_out_buf, out_buf_len);
	} else {
		memcpy(out_buf, short_out_buf, out_buf_len);
	}

	delete[]int_buf;
	delete[]short_in_buf_a;
	delete[]short_in_buf_b;
	delete[]short_out_buf;
	delete[]signed_char_tmp;
	delete[]signed_char_in_buf;
	delete[]signed_char_out_buf;
}
