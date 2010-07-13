/* lecroy_user.h
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

#include "../../vxi11/vxi11_user.h"

void	lecroy_scope_channel_str(char chan, char *source);
char	lecroy_relate_function_to_source(char chan);
int	lecroy_open(char *ip, CLINK *clink);
int	lecroy_close(char *ip, CLINK *clink);
int	lecroy_init(CLINK *clink);
long	lecroy_obtain_insp_long(CLINK *clink, const char *cmd, unsigned long timeout);
double	lecroy_obtain_insp_double(CLINK *clink, const char *cmd, unsigned long timeout);
long	lecroy_receive_data_block(CLINK *clink, char *buffer, unsigned long len, unsigned long timeout);
long	lecroy_calculate_no_of_bytes(CLINK *clink, char chan, unsigned long timeout);
long	lecroy_get_data(CLINK *clink, char chan, int clear_sweeps, char *buf, unsigned long buf_len,unsigned long timeout);
long	lecroy_get_data(CLINK *clink, char chan, int clear_sweeps, char *buf, unsigned long buf_len, int arm_and_wait, unsigned long timeout);
void	lecroy_set_for_auto(CLINK *clink);
void	lecroy_set_for_norm(CLINK *clink);
void	lecroy_clear_sweeps(CLINK *clink);
int	lecroy_wait_all_averages(CLINK *clink, unsigned long timeout);
long	lecroy_write_wfi_file(CLINK *clink, char *wfiname, char chan, char *captured_by, int no_of_traces, int bytes_per_point, unsigned long timeout);
char	lecroy_set_averages(CLINK *clink, char chan, int no_averages);
char	lecroy_set_segmented_averages(CLINK *clink, char chan, int no_averages);
int	lecroy_get_segmented_status(CLINK *clink);
int	lecroy_get_segmented(CLINK *clink);
int	lecroy_set_segmented(CLINK *clink, int no_segments);
int	lecroy_display_channel(CLINK *clink, char chan, int on_or_off);
double	lecroy_set_sample_rate(CLINK *clink, double s_rate, long n_points, long timeout);
/*int	lecroy_report_status(CLINK *clink, unsigned long timeout);
int	lecroy_get_setup(CLINK *clink, char *buf, unsigned long buf_len);
int	lecroy_send_setup(CLINK *clink, char *buf, unsigned long buf_len);
long	lecroy_get_screen_data(CLINK *clink, char chan, char *buf, unsigned long buf_len, unsigned long timeout, double s_rate, long npoints);
int	lecroy_set_for_capture(CLINK *clink, double s_rate, long npoints, unsigned long timeout);
long	lecroy_get_data(CLINK *clink, char chan, int digitise, char *buf, unsigned long buf_len,unsigned long timeout);
int	lecroy_get_preamble(CLINK *clink, char *buf, unsigned long buf_len);
long	lecroy_write_wfi_file(CLINK *clink, char *wfiname, long no_of_bytes, char *captured_by, int no_of_traces, unsigned long timeout);
int	lecroy_set_averages(CLINK *clink, int no_averages);
long	lecroy_get_averages(CLINK *clink);
double	lecroy_get_sample_rate(CLINK *clink);
long	lecroy_get_n_points(CLINK *clink);
int	lecroy_display_channel(CLINK *clink, char chan, int on_or_off);*/
