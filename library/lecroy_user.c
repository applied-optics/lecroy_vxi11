/* lecroy_user.c
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

#include "lecroy_user.h"

/* In the library we tend to use a single char to denote a channel. This works
 * fairly well, and is rooted in the old days when LeCroy called their maths
 * channels A, B, C, and D. So channel 'A' is actually maths function "F1", and
 * we add the extra ":" at the end because that is the format of the remote
 * control commands for LeCroy. */
void	lecroy_scope_channel_str(char chan, char *source){
	switch (chan) {
		case 'A' :
		case 'a' : strcpy(source,"F1:");
			break;
		case 'B' :
		case 'b' : strcpy(source,"F2:");
			break;
		case 'C' :
		case 'c' : strcpy(source,"F3:");
			break;
		case 'D' :
		case 'd' : strcpy(source,"F4:");
			break;
		case 'E' :
		case 'e' : strcpy(source,"F5:");
			break;
		case 'F' :
		case 'f' : strcpy(source,"F6:");
			break;
		case 'G' :
		case 'g' : strcpy(source,"F7:");
			break;
		case 'H' :
		case 'h' : strcpy(source,"F8:");
			break;
		case '1' : strcpy(source,"C1:");
			break;
		case '2' : strcpy(source,"C2:");
			break;
		case '3' : strcpy(source,"C3:");
			break;
		case '4' : strcpy(source,"C4:");
			break;
		default :  printf("error: unknown channel '%c', using channel 1\n",chan);
			   strcpy(source,"C1:");
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
char	lecroy_relate_function_to_source(char chan) {
	switch(chan) {
		case 'A' :
		case 'a' : return '1';
			break;
		case 'B' :
		case 'b' : return '2';
			break;
		case 'C' :
		case 'c' : return '3';
			break;
		case 'D' :
		case 'd' : return '4';
			break;
		case 'E' :
		case 'e' :
		case 'F' :
		case 'f' :
		case 'G' :
		case 'g' :
		case 'H' :
		case 'h' : printf("error: Functions F5-F8 (E-H) don't have an associated channel, using channel 1\n");
			   return '1';
		case '1' : return 'A';
			break;
		case '2' : return 'B';
			break;
		case '3' : return 'C';
			break;
		case '4' : return 'D';
			break;
		default :  printf("error: unknown channel '%c', using channel 1\n",chan);
			   return '1';
			break;
		}
	}

/* This really is just a wrapper. Only here because folk might be uncomfortable
 * using commands from the vxi11_user library directly! */
int	lecroy_open(char *ip, CLINK *clink) {
	return vxi11_open_device(ip, clink);
	}

/* Again, just a wrapper */
int	lecroy_close(char *ip, CLINK *clink) {
	vxi11_send(clink, "MSG"); /* remove message on bottom of screen */
	return vxi11_close_device(ip, clink);
	}

/* Set up some fundamental settings for data transfer. It's possible
 * (although not certain) that some or all of these would be reset after
 * a system reset. It's a very tiny overhead right at the beginning of your
 * acquisition that's performed just once. */
int	lecroy_init(CLINK *clink) {
char	cmd[256];
int	ret;
	/* Sets DEF9 (defines arbitrary data block header), 16-bit data
	 * (needed when averaging), binary format (more efficient than ascii) */
	ret=vxi11_send(clink, "COMM_FORMAT DEF9,WORD,BIN");
	if(ret < 0) {
		printf("ERROR in lecroy_init, could not send very first command.\n");
		return ret;
		}
	vxi11_send(clink, "COMM_HEADER OFF"); /* much easier parsing of responses */
	vxi11_send(clink, "COMM_ORDER LO"); /* sets endian-ness to Intel, ie LSB, MSB */
	vxi11_send(clink, "MSG \"STEVE'S LINUX VXI-11 LECROY DRIVER\""); /* message on bottom of screen */
	return 0;
	}

/* Annoyingly, INSP? queries don't just return a number, they also return the
 * paramater name you want to inspect, followed by spaces, a ":", then a space.
 * In order to parse these, we look for the ":" */
long	lecroy_obtain_insp_long(CLINK *clink, const char *cmd, unsigned long timeout) {
char	buf[256]; /* 256=arbitrary length...  */
int	l = 0;
	memset(buf, 0, 256);
	if (vxi11_send_and_receive(clink, cmd, buf, 256, timeout) != 0) {
		printf("Error: lecroy_obtain_insp_long returning 0\n");
		return 0;
		}
	while((buf[l] != ':') && (l < 256) ) l++;
	if(l == 256) {
		printf("Error: problem parsing returned string in lecroy_obtain_insp_long. String:\n");
		printf("%s\nReturning 0\n",buf);
		return 0;
		}
	return strtol(buf+l+2, (char **)NULL, 10);
	}

/* Annoyingly, INSP? queries don't just return a number, they also return the
 * paramater name you want to inspect, followed by spaces, a ":", then a space.
 * In order to parse these, we look for the ":" */
double	lecroy_obtain_insp_double(CLINK *clink, const char *cmd, unsigned long timeout) {
char	buf[256]; /* 256=arbitrary length...  */
int	l = 0;
	memset(buf, 0, 256);
	if (vxi11_send_and_receive(clink, cmd, buf, 256, timeout) != 0) {
		printf("Error: lecroy_obtain_insp_double returning 0.0\n");
		return 0.0;
		}
	while((buf[l] != ':') && (l < 256) ) l++;
	if(l == 256) {
		printf("Error: problem parsing returned string in lecroy_obtain_insp_double. String:\n");
		printf("%s\nReturning 0.0\n",buf);
		return 0.0;
		}
	return strtod(buf+l+2, (char **)NULL);
	}

/* Why can't everything be this easy? */
/* ...
 * Famous last words... turns out you have to ask twice, as if you've recently
 * changed the sample rate then the changes don't propagate through unless you've
 * asked a couple of times. Way to go, lecroy! */
long	lecroy_calculate_no_of_bytes(CLINK *clink, char chan, unsigned long timeout) {
char	cmd[256];
char	source[20];
	lecroy_scope_channel_str(chan, source);
	sprintf(cmd, "%sINSP? WAVE_ARRAY_1", source);
	lecroy_obtain_insp_long(clink, cmd, timeout);
	return lecroy_obtain_insp_long(clink, cmd, timeout);
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
long	lecroy_receive_data_block(CLINK *clink, char *buffer, unsigned long len, unsigned long timeout) {
/* I'm not sure what the maximum length of this header is, I'll assume it's
 * 24 (DATA_ARRAY_1,#9 + 9 digits) */
unsigned long	necessary_buffer_size;
char		*in_buffer;
int		ret;
int		ndigits;
unsigned long	returned_bytes;
int		l = 0;
char		scan_cmd[20];
	necessary_buffer_size=len+25;
	in_buffer=new char[necessary_buffer_size];
	ret=vxi11_receive(clink, in_buffer, necessary_buffer_size, timeout);
	if (ret < 0) return ret;
	while((in_buffer[l] != '#') && (l < 25)) l++;
	if (in_buffer[l] != '#') {
		printf("lecroy_user: data block error: data block does not begin with '#'\n");
		printf("First 25 characters received were: '");
		for(l=0;l<25;l++) {
			printf("%c",in_buffer[l]);
			}
		printf("'\n");
		return -3;
		}

	/* first find out how many digits */
	sscanf(in_buffer+l,"#%1d",&ndigits);
	/* some instruments, if there is a problem acquiring the data, return only "#0" */
	if (ndigits > 0) {
		/* now that we know, we can convert the next <ndigits> bytes into an unsigned long */
		sprintf(scan_cmd,"#%%1d%%%dlu",ndigits);
		sscanf(in_buffer+l,scan_cmd,&ndigits,&returned_bytes);
		memcpy(buffer, in_buffer+(ndigits+l+2), returned_bytes);
		delete[] in_buffer;
		return (long) returned_bytes;
		}
	else return 0;
	}



/* Wrapper. Most times we want to arm and wait... unless we've already set this up and returned
 * control to some other process (eg moving a motorised stage), and all we want to do now is
 * grab the data */
long	lecroy_get_data(CLINK *clink, char chan, int clear_sweeps, char *buf, unsigned long buf_len, unsigned long timeout) {
	return lecroy_get_data(clink, chan, clear_sweeps, buf, buf_len, 1, timeout);
	}

long	lecroy_get_data(CLINK *clink, char chan, int clear_sweeps, char *buf, unsigned long buf_len, int arm_and_wait, unsigned long timeout) {
char	cmd[256];
char	source[20];
long	ret;
	if(chan=='1' || chan=='2' || chan=='3' || chan=='4') {
		if(arm_and_wait == 1)	ret = vxi11_obtain_long_value(clink, "ARM;WAIT;*OPC?", timeout);
		else			ret = vxi11_obtain_long_value(clink, "*OPC?", timeout);
		if(ret != 1) {
			printf("lecroy_get_data: error, *OPC? did not return 1\n");
			return 0;
			}
		}
	else {
		if(clear_sweeps == 1) {
			lecroy_clear_sweeps(clink);
			lecroy_wait_all_averages(clink,timeout);
			}
		}
	lecroy_scope_channel_str(chan, source);
	sprintf(cmd, "%sWF? DAT1", source);
	vxi11_send(clink, cmd);
	return lecroy_receive_data_block(clink, buf, buf_len, timeout);
	}

void	lecroy_set_for_auto(CLINK *clink) {
	vxi11_send(clink, "TRMD AUTO");
	}

void	lecroy_set_for_norm(CLINK *clink) {
	vxi11_send(clink, "TRMD NORM");
	}

void	lecroy_clear_sweeps(CLINK *clink) {
	/* Needs to send an INR? query, in order to reset the registers
	 * (we don't care what the value is) */
	vxi11_obtain_long_value(clink, "INR?");
	vxi11_send(clink, "CLSW");
	}

int	lecroy_wait_all_averages(CLINK *clink, unsigned long timeout) {
char	cmd[256];
char	buf[256];
int	chan_on[4];
int	mask;
int	l;
int	inr = 0;
int	old_inr = 0;
int	test = 0;
	/* Go through all maths channels, see if they're turned on or not */
	for(l=0;l<4;l++) {
		sprintf(cmd, "F%d:TRACE?", l + 1);
		memset(buf, 0, 256);
		vxi11_send_and_receive(clink, cmd, buf, 256, timeout);
		if(strstr(buf,"ON") != NULL) {
			chan_on[l] = 1;
			}
		else {
			chan_on[l] = 0;
			}
		}

	/* Now investigate which maths channels (that are turned on) are averaging */
	l = 0;
	while(l < 4) {
		if(chan_on[l] == 1) {
			sprintf(cmd, "F%d:DEF?", l + 1);
			memset(buf, 0, 256);
			vxi11_send_and_receive(clink, cmd, buf, 256, timeout);
			if(strstr(buf,"AVG") == NULL) {
				chan_on[l] = 0;
				}
			}
		l++;
		}
	/* make the appropriate mask */
	mask = (chan_on[0] * 256) + (chan_on[1] * 512) + (chan_on[2] * 1024) + (chan_on[3] * 2048);

	while(test == 0)
	{
		inr = (int)vxi11_obtain_long_value(clink, "INR?", timeout);
		old_inr = old_inr | inr;
		if((old_inr & mask) == mask) test = 1;
		else test = 0;
		}
	return 0;
	}

long	lecroy_write_wfi_file(CLINK *clink, char *wfiname, char chan, char *captured_by, int no_of_traces, int bytes_per_point, unsigned long timeout) {
FILE	*wfi;
double	vgain,voffset,hinterval,hoffset;
long	no_of_bytes;
int	ret;
char	cmd[256];
char	source[20];
int	no_of_segments;

	lecroy_scope_channel_str(chan, source);

	no_of_bytes = lecroy_calculate_no_of_bytes(clink, chan, timeout);
	sprintf(cmd, "%sINSP? HORIZ_INTERVAL", source);
	hinterval = lecroy_obtain_insp_double(clink, cmd, timeout);
	sprintf(cmd, "%sINSP? HORIZ_OFFSET", source);
	hoffset = lecroy_obtain_insp_double(clink, cmd, timeout);
	sprintf(cmd, "%sINSP? VERTICAL_GAIN", source);
	vgain = lecroy_obtain_insp_double(clink, cmd, timeout);
	sprintf(cmd, "%sINSP? VERTICAL_OFFSET", source);
	voffset = lecroy_obtain_insp_double(clink, cmd, timeout);

	if(chan=='1' || chan=='2' || chan=='3' || chan=='4') {
		if(lecroy_get_segmented_status(clink) == 1)	no_of_segments = lecroy_get_segmented(clink);
		}
	else {
		no_of_segments = 1;
		}

	wfi = fopen(wfiname,"w");
	if (wfi > 0) {
		fprintf(wfi,"%% %s\n",wfiname);
		fprintf(wfi,"%% Waveform captured using %s\n\n",captured_by);
		if(no_of_segments==0){
			fprintf(wfi,"%% Number of bytes:\n%d\n\n",no_of_bytes);
			}
		else{
			fprintf(wfi,"%% Number of bytes:\n%d\n\n",(no_of_bytes / no_of_segments));
			}
		fprintf(wfi,"%% Vertical gain:\n%g\n\n",vgain);
		fprintf(wfi,"%% Vertical offset:\n%g\n\n",-voffset);
		fprintf(wfi,"%% Horizontal interval:\n%g\n\n",hinterval);
		fprintf(wfi,"%% Horizontal offset:\n%g\n\n",hoffset);
		if(no_of_segments==0){
			fprintf(wfi,"%% Number of traces:\n%d\n\n",no_of_traces);
			}
		else{
			fprintf(wfi,"%% Number of traces:\n%d\n\n",(no_of_traces * no_of_segments));
			}
		fprintf(wfi,"%% Number of bytes per data-point:\n%d\n\n", bytes_per_point); /* always 2... at least that's the intention */
		fprintf(wfi,"%% Keep all datapoints (0 or missing knocks off 1 point, legacy lecroy):\n%d\n\n",1);
		fclose(wfi);
		}
	else {
		printf("error: agilent_write_wfi_file: could not open %s for writing\n",wfiname);
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

char	lecroy_set_averages(CLINK *clink, char chan, int no_averages) {
char	maths_chan;
char	maths_chan_str[20];
char	source[20];
char	cmd[256];

	if(chan=='1' || chan=='2' || chan=='3' || chan=='4') {
		maths_chan = lecroy_relate_function_to_source(chan);
		}
	else {
		maths_chan = chan;
		chan = lecroy_relate_function_to_source(maths_chan);
		}
	if(no_averages > 1) {
		lecroy_scope_channel_str(maths_chan, maths_chan_str);
		sprintf(source,"C%c", chan);
		sprintf(cmd, "%sDEF EQN, 'AVG(%s)',AVERAGETYPE,SUMMED,SWEEPS,%d SWEEP", maths_chan_str, source, no_averages);
		vxi11_send(clink, cmd);
		lecroy_display_channel(clink, maths_chan, 1);
		return maths_chan;
		}
	else {
		lecroy_display_channel(clink, maths_chan, 0);
		lecroy_display_channel(clink, chan, 1);
		return chan;
		}
	}

char	lecroy_set_segmented_averages(CLINK *clink, char chan, int no_averages) {
char	maths_chan;
int	actual_no_averages;

	actual_no_averages = lecroy_set_segmented(clink, no_averages);
	maths_chan = lecroy_set_averages(clink, chan, actual_no_averages);
	return maths_chan;
	}

int	lecroy_get_segmented_status(CLINK *clink) {
char	buf[256];
int	segmented_status;

	vxi11_send_and_receive(clink, "VBS? 'Return=app.Acquisition.Horizontal.SampleMode'", buf, 256, VXI11_READ_TIMEOUT);
	segmented_status = strncmp(buf, "Sequence", 8);
	if(segmented_status == 0)	return 1;
	else				return 0;
	}

int	lecroy_get_segmented(CLINK *clink) {
	return (int) vxi11_obtain_long_value(clink, "VBS? 'Return=app.Acquisition.Horizontal.NumSegments'");
	}

int	lecroy_set_segmented(CLINK *clink, int no_segments) {
char	cmd[256];
int	actual_no_segments;

	sprintf(cmd, "SEQ ON,%d;ARM", no_segments);
	vxi11_send(clink, cmd);
	actual_no_segments = lecroy_get_segmented(clink);
	return actual_no_segments;
	}

/* Turns a channel on or off (pass "1" for on, "0" for off)*/
int	lecroy_display_channel(CLINK *clink, char chan, int on_or_off) {
char	source[20];
char	cmd[256];

	memset(source,0,20);
	lecroy_scope_channel_str(chan, source);
	if(on_or_off == 0)	sprintf(cmd, "%sTRACE OFF", source);
	else			sprintf(cmd, "%sTRACE ON", source);
	return vxi11_send(clink, cmd);
	}

/* Set the sample rate, either directly or by inference by the number of points specified.
 * If both are specified, then sample rate takes precedence. Returns the actual sample rate. */
double	lecroy_set_sample_rate(CLINK *clink, double s_rate, long n_points, long timeout) {
double	actual_s_rate;
double	expected_s_rate;
double	time_range;
char	cmd[256];
	if(n_points > 0) {
		time_range = vxi11_obtain_double_value(clink, "TIME_DIV?", timeout) * 10.0;
		expected_s_rate = (double) n_points / time_range;
		sprintf(cmd, "VBS 'app.Acquisition.Horizontal.SampleRate=%g'", expected_s_rate);
		vxi11_send(clink, cmd);
		}

	if(s_rate > 0) {
		sprintf(cmd, "VBS 'app.Acquisition.Horizontal.SampleRate=%g'", s_rate);
		vxi11_send(clink, cmd);
		}
	actual_s_rate = vxi11_obtain_double_value(clink, "VBS? 'Return=app.Acquisition.Horizontal.SampleRate'", timeout);
	return actual_s_rate;
	}
