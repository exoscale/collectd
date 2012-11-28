/**
 * collectd - src/zfs_health.c
 * Copyright (C) 2009  Anthony Dewhurst
 * Copyright (C) 2012  Aurelien Rougemont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; only version 2 of the License is applicable.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Authors:
 *   Anthony Dewhurst <dewhurst at gmail>
 *   Aurelien Rougemont <beorn at gandi.net>
 **/

#include "collectd.h"
#include "common.h"
#include "plugin.h"

/*
 * Global variables
 */

extern kstat_ctl_t *kc;

static void zh_submit (const char* type, const char* type_instance, value_t* values, int values_len)
{
	value_list_t vl = VALUE_LIST_INIT;

	vl.values = values;
	vl.values_len = values_len;

	sstrncpy (vl.host, hostname_g, sizeof (vl.host));
	sstrncpy (vl.plugin, "zfs_health", sizeof (vl.plugin));
	sstrncpy (vl.type, type, sizeof (vl.type));
	sstrncpy (vl.type_instance, type_instance, sizeof (vl.type_instance));

	plugin_dispatch_values (&vl);
}

static void zh_submit_gauge (const char* type, const char* type_instance, gauge_t value)
{
	value_t vv;

	vv.gauge = value;
	zh_submit (type, type_instance, &vv, 1);
}

static int zh_read_derive (kstat_t *ksp, const char *kstat_value,
    const char *type, const char *type_instance)
{
  long long tmp;
  value_t v;

  tmp = get_kstat_value (ksp, (char *)kstat_value);
  if (tmp == -1LL)
  {
    ERROR ("zfs_health plugin: Reading kstat value \"%s\" failed.", kstat_value);
    return (-1);
  }

  v.derive = (derive_t) tmp;
  zh_submit (type, type_instance, /* values = */ &v, /* values_num = */ 1);
  return (0);
}

static int zh_read_gauge (kstat_t *ksp, const char *kstat_value,
    const char *type, const char *type_instance)
{
  long long tmp;
  value_t v;

  tmp = get_kstat_value (ksp, (char *)kstat_value);
  if (tmp == -1LL)
  {
    ERROR ("zfs_health plugin: Reading kstat value \"%s\" failed.", kstat_value);
    return (-1);
  }

  v.gauge = (gauge_t) tmp;
  zh_submit (type, type_instance, /* values = */ &v, /* values_num = */ 1);
  return (0);
}

static void zh_submit_ratio (const char* type_instance, gauge_t hits, gauge_t misses)
{
	gauge_t ratio = NAN;

	if (!isfinite (hits) || (hits < 0.0))
		hits = 0.0;
	if (!isfinite (misses) || (misses < 0.0))
		misses = 0.0;

	if ((hits != 0.0) || (misses != 0.0))
		ratio = hits / (hits + misses);

	zh_submit_gauge ("cache_ratio", type_instance, ratio);
}

static int zh_read (void)
{
	gauge_t  arc_hits, arc_misses, l2_hits, l2_misses;
	value_t  zfs_io[2];
	kstat_t	 *ksp	= NULL;

	get_kstat (&ksp, "unix", 0, "vopstats_zfs");
	if (ksp == NULL)
	{
		ERROR ("zfs_health plugin: Cannot find unix:0:vopstats_zfs kstat.");
		return (-1);
	}

	/* Sizes */ 
	/* zh_read_gauge (ksp, "size",    "cache_size", "arc");
	* zh_read_gauge (ksp, "l2_size", "cache_size", "L2");
	*/
        /* Operations */
	/* zh_read_derive (ksp, "allocated","cache_operation", "allocated");
	* zh_read_derive (ksp, "deleted",  "cache_operation", "deleted");
	* zh_read_derive (ksp, "stolen",   "cache_operation", "stolen");
	*/

	/* I/O */
	zfs_io[0].derive = get_kstat_value(ksp, "read_bytes");
	zfs_io[1].derive = get_kstat_value(ksp, "write_bytes");

	zh_submit ("io_octets", "ZFS", zfs_io, /* num values = */ 2);

	return (0);
} /* int zh_read */

static int zh_init (void) /* {{{ */
{
	/* kstats chain already opened by update_kstat (using *kc), verify everything went fine. */
	if (kc == NULL)
	{
		ERROR ("zfs_health plugin: kstat chain control structure not available.");
		return (-1);
	}

	return (0);
} /* }}} int zh_init */

void module_register (void)
{
	plugin_register_init ("zfs_health", zh_init);
	plugin_register_read ("zfs_health", zh_read);
} /* void module_register */

/* vmi: set sw=8 noexpandtab fdm=marker : */
