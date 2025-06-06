/* tap-srt.c
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <epan/packet.h>
#include <epan/srt_table.h>
#include <epan/timestamp.h>
#include <epan/stat_tap_ui.h>
#include <wsutil/cmdarg_err.h>
#include <ui/cli/tshark-tap.h>

#define NANOSECS_PER_SEC 1000000000

typedef struct _srt_t {
	const char *type;
	const char *filter;
	srt_data_t data;
} srt_t;

static void
draw_srt_table_data(srt_stat_table *rst, bool draw_footer, const char *subfilter)
{
	int i;
	uint64_t td;
	uint64_t sum;

	if (rst->num_procs > 0) {
		if (rst->filter_string != NULL && subfilter != NULL) {
			printf("Filter: %s and (%s)\n", rst->filter_string, subfilter);
		} else if (subfilter != NULL) {
			/* Print (subfilter) to disambiguate from just rst->filter_string. */
			printf("Filter: (%s)\n", subfilter);
		} else {
			printf("Filter: %s\n", rst->filter_string ? rst->filter_string : "");
		}
		printf("Index  %-22s Calls    Min SRT    Max SRT    Avg SRT    Sum SRT\n", (rst->proc_column_name != NULL) ? rst->proc_column_name : "Procedure");
	}
	for(i=0;i<rst->num_procs;i++){
		/* ignore procedures with no calls (they don't have rows) */
		if(rst->procedures[i].stats.num==0){
			continue;
		}
		/* Scale the average SRT in units of 1us and round to the nearest us.
		   tot.secs is a time_t which may be 32 or 64 bits (or even floating)
		   depending uon the platform.  After casting tot.secs to 64 bits, it
		   would take a capture with a duration of over 136 *years* to
		   overflow the secs portion of td. */
		td = ((uint64_t)(rst->procedures[i].stats.tot.secs))*NANOSECS_PER_SEC + rst->procedures[i].stats.tot.nsecs;
		sum = (td + 500) / 1000;
		td = ((td / rst->procedures[i].stats.num) + 500) / 1000;

		printf("%5d  %-22s %6u %3d.%06d %3d.%06d %3d.%06d %3d.%06d\n",
		       i, rst->procedures[i].procedure,
		       rst->procedures[i].stats.num,
		       (int)rst->procedures[i].stats.min.secs, (rst->procedures[i].stats.min.nsecs+500)/1000,
		       (int)rst->procedures[i].stats.max.secs, (rst->procedures[i].stats.max.nsecs+500)/1000,
		       (int)(td/1000000), (int)(td%1000000),
		       (int)(sum/1000000), (int)(sum%1000000)
		);
	}

	if (draw_footer)
		printf("==================================================================\n");
}

static void
srt_draw(void *arg)
{
	unsigned i = 0;
	srt_data_t* data = (srt_data_t*)arg;
	srt_t *ui = (srt_t *)data->user_data;
	srt_stat_table *srt_table;
	bool need_newline = false;

	printf("\n");
	printf("===================================================================\n");
	printf("%s SRT Statistics:\n", ui->type);

	srt_table = g_array_index(data->srt_array, srt_stat_table*, i);
	draw_srt_table_data(srt_table, data->srt_array->len == 1, ui->filter);
	if (srt_table->num_procs > 0) {
		need_newline = true;
	}

	for (i = 1; i < data->srt_array->len; i++)
	{
		if (need_newline)
		{
			printf("\n");
			need_newline = false;
		}
		srt_table = g_array_index(data->srt_array, srt_stat_table*, i);
		draw_srt_table_data(srt_table, i == data->srt_array->len-1, ui->filter);
		if (srt_table->num_procs > 0) {
			need_newline = true;
		}
	}
}

static GArray* global_srt_array;

static bool
init_srt_tables(register_srt_t* srt, const char *filter)
{
	srt_t *ui;
	GString *error_string;

	ui = g_new0(srt_t, 1);
	ui->type = proto_get_protocol_short_name(find_protocol_by_id(get_srt_proto_id(srt)));
	ui->filter = g_strdup(filter);
	ui->data.srt_array = global_srt_array;
	ui->data.user_data = ui;

	error_string = register_tap_listener(get_srt_tap_listener_name(srt), &ui->data, filter, 0, NULL, get_srt_packet_func(srt), srt_draw, NULL);
	if (error_string) {
		free_srt_table(srt, global_srt_array);
		g_free(ui);
		cmdarg_err("Couldn't register srt tap: %s", error_string->str);
		g_string_free(error_string, TRUE);
		return false;
	}

	return true;
}

static bool
dissector_srt_init(const char *opt_arg, void* userdata)
{
	register_srt_t *srt = (register_srt_t*)userdata;
	const char *filter=NULL;
	char* err;

	srt_table_get_filter(srt, opt_arg, &filter, &err);
	if (err != NULL)
	{
		char* cmd_str = srt_table_get_tap_string(srt);
		cmdarg_err("invalid \"-z %s,%s\" argument", cmd_str, err);
		g_free(cmd_str);
		g_free(err);
		return false;
	}

	/* Need to create the SRT array now */
	global_srt_array = g_array_new(false, true, sizeof(srt_stat_table*));

	srt_table_dissector_init(srt, global_srt_array);
	return init_srt_tables(srt, filter);
}

/* Set GUI fields for register_srt list */
bool
register_srt_tables(const void *key _U_, void *value, void *userdata _U_)
{
	register_srt_t *srt = (register_srt_t*)value;
	const char* short_name = proto_get_protocol_short_name(find_protocol_by_id(get_srt_proto_id(srt)));
	stat_tap_ui ui_info;
	char *cli_string;

	/* XXX - CAMEL dissector hasn't been converted over due seemingly different tap packet
	   handling functions.  So let the existing TShark CAMEL tap keep its registration */
	if (strcmp(short_name, "CAMEL") == 0)
		return false;

	cli_string = srt_table_get_tap_string(srt);
	ui_info.group = REGISTER_STAT_GROUP_RESPONSE_TIME;
	ui_info.title = NULL;   /* construct this from the protocol info? */
	ui_info.cli_string = cli_string;
	ui_info.tap_init_cb = dissector_srt_init;
	ui_info.nparams = 0;
	ui_info.params = NULL;
	register_stat_tap_ui(&ui_info, srt);
	g_free(cli_string);
	return false;
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: t
 * End:
 *
 * vi: set shiftwidth=8 tabstop=8 noexpandtab:
 * :indentSize=8:tabSize=8:noTabs=false:
 */
