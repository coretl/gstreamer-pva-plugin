/* GStreamer
 * Copyright (C) 2014 FIXME <fixme@example.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_PVASOURCE_H_
#define _GST_PVASOURCE_H_

#include <gst/base/gstpushsrc.h>
#include <pv/monitor.h>

G_BEGIN_DECLS

class BrainDeadMonitorRequester;
typedef std::tr1::shared_ptr<BrainDeadMonitorRequester> BrainDeadMonitorRequesterPtr;

#define GST_TYPE_PVASOURCE   (gst_pvasource_get_type())
#define GST_PVASOURCE(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PVASOURCE,GstPVASource))
#define GST_PVASOURCE_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PVASOURCE,GstPVASourceClass))
#define GST_IS_PVASOURCE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PVASOURCE))
#define GST_IS_PVASOURCE_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PVASOURCE))

typedef struct _GstPVASource GstPVASource;
typedef struct _GstPVASourceClass GstPVASourceClass;

struct _GstPVASource
{
	GstPushSrc base_pvasource;
	// pv name and subscription object
	char *pv_name;
	BrainDeadMonitorRequesterPtr monitorRequester;
	int width;
	int height;
	int off;
	guint64 timestamp_offset;
	guint64 last_timestamp;
};

struct _GstPVASourceClass
{
	GstPushSrcClass base_pvasource_class;
};

GType gst_pvasource_get_type (void);

G_END_DECLS

#endif
