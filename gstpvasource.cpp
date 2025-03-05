/* GStreamer
 * Copyright (C) 2014 Tom Cobb, Diamond Light Source <tom.cobb@diamond.ac.uk>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gstpvasource
 *
 * The pvasource element monitors an EPICS v4 pv that produces NTNDArrays
 * and makes GstBuffers from them
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v pvasource ! video/x-raw,format=GRAY8 ! autovideosink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include "gstpvasource.h"
#include <unistd.h>
#include <pv/pvAccess.h>
#include <pv/clientFactory.h>
using namespace epics::pvAccess;
#include <pv/pvData.h>
#include <pv/event.h>
using namespace epics::pvData;
#include "iostream"
using namespace std;

GST_DEBUG_CATEGORY_STATIC (gst_pvasource_debug_category);
#define GST_CAT_DEFAULT gst_pvasource_debug_category

enum
{
	PROP_0,
	PROP_PV_NAME
};

/* pad templates */

static GstStaticPadTemplate gst_pvasource_src_template =
		GST_STATIC_PAD_TEMPLATE ("src",
				GST_PAD_SRC,
				GST_PAD_ALWAYS,
				GST_STATIC_CAPS ("ANY")
		);


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstPVASource, gst_pvasource, GST_TYPE_PUSH_SRC,
		GST_DEBUG_CATEGORY_INIT (gst_pvasource_debug_category, "pvasource", 0,
				"debug category for pvasource element"));

static void
gst_pvasource_init (GstPVASource * pvasource)
{
	gst_base_src_set_live (GST_BASE_SRC (pvasource), TRUE);
	gst_base_src_set_format (GST_BASE_SRC (pvasource), GST_FORMAT_TIME);
	cout << "****Init" << endl;

	pvasource->pv_name = NULL;
	cout << pvasource->monitorRequester << " is monitorRequester" << endl;
	pvasource->width = 0;
	pvasource->height = 0;
	pvasource->off = 0;
	pvasource->timestamp_offset = 0;
	pvasource->last_timestamp = 0;
}

void
gst_pvasource_finalize (GObject * object)
{
	GstPVASource *pvasource = GST_PVASOURCE (object);

	GST_DEBUG_OBJECT (pvasource, "finalize");

	g_free (pvasource->pv_name);
	pvasource->pv_name = NULL;

	// FIXME: close subscription

	G_OBJECT_CLASS (gst_pvasource_parent_class)->finalize (object);
}

void
gst_pvasource_set_property (GObject * object, guint property_id,
		const GValue * value, GParamSpec * pspec)
{
	GstPVASource *pvasource = GST_PVASOURCE (object);

	GST_DEBUG_OBJECT (pvasource, "set_property");

	switch (property_id) {
	case PROP_PV_NAME:
		g_free (pvasource->pv_name);
		/* check if we are currently monitoring and block changing pv name if so */
		if (pvasource->monitorRequester == NULL) {
			pvasource->pv_name = g_strdup (g_value_get_string (value));
			GST_LOG_OBJECT (pvasource, "Set pv name to %s", pvasource->pv_name);
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

void
gst_pvasource_get_property (GObject * object, guint property_id,
		GValue * value, GParamSpec * pspec)
{
	GstPVASource *pvasource = GST_PVASOURCE (object);

	GST_DEBUG_OBJECT (pvasource, "get_property");

	switch (property_id) {
	case PROP_PV_NAME:
		g_value_set_string (value, pvasource->pv_name);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gst_pvasource_get_times (GstBaseSrc * basesrc, GstBuffer * buffer,
		      GstClockTime * start, GstClockTime * end)
{
	if (gst_base_src_is_live (basesrc)) {
		GstClockTime timestamp = GST_BUFFER_PTS (buffer);

		if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
			GstClockTime duration = GST_BUFFER_DURATION (buffer);

			if (GST_CLOCK_TIME_IS_VALID (duration)) {
				*end = timestamp + duration;
			}
			*start = timestamp;
		}
	} else {
		*start = -1;
		*end = -1;
	}
}

class BrainDeadChannelRequester;
typedef std::tr1::shared_ptr<BrainDeadChannelRequester> BrainDeadChannelRequesterPtr;

class BrainDeadChannelRequester :
    public ChannelRequester
{
public:
	BrainDeadChannelRequester() {}
    virtual ~BrainDeadChannelRequester(){}
    virtual string getRequesterName() { return "BrainDeadChannelRequester";}
    virtual void message(string const & message, MessageType messageType) {};
    virtual void channelCreated(const Status& status, Channel::shared_pointer const & channel) {
    	this->status = status;
    	this->event.signal();
    }
    virtual void channelStateChange(Channel::shared_pointer const & channel, Channel::ConnectionState connectionState) {}
    Status status;
    Event event;
};

class BrainDeadMonitorRequester :
    public MonitorRequester
{
public:
	BrainDeadMonitorRequester() {}
    virtual ~BrainDeadMonitorRequester(){}
    virtual string getRequesterName() { return "BrainDeadMonitorRequester";}
    virtual void monitorConnect(Status const & status,
        MonitorPtr const & monitor, StructureConstPtr const & structure) {
    	this->monitor = monitor;
    	this->structure = structure;
    	this->status = status;
    	//cout << "monitorConnect" << endl;
    	this->monitor->start();
    }
    virtual void monitorEvent(MonitorPtr const & monitor) {
    	//cout << "monitorEvent" << endl;
    	this->element = monitor->poll();
    	cerr << "element " << element << endl;
    	if (this->element) event.signal();
    }
    virtual void unlisten(MonitorPtr const & monitor) {}
    virtual void message(string const & message, MessageType messageType) {cout << message << endl;};
    Event event;
    MonitorPtr monitor;
	MonitorElement::shared_pointer element;
    StructureConstPtr structure;
    Status status;
};

class WrappedElement {
public:
	WrappedElement(MonitorElement::shared_pointer element, MonitorPtr monitor) :
		element(element), monitor(monitor) {}
	virtual ~WrappedElement() {
		this->monitor->release(this->element);
	}
	MonitorElement::shared_pointer element;
	MonitorPtr monitor;
};

void element_destroy(gpointer data) {
	WrappedElement *wrappedElement = (WrappedElement *) data;
	delete wrappedElement;
	//cout << "released" << endl;
}

static gboolean
gst_pvasource_start (GstBaseSrc * src)
{
	GstPVASource *pvasource = GST_PVASOURCE (src);

	GST_DEBUG_OBJECT (pvasource, "start");
	GST_WARNING_OBJECT(pvasource, "start");

	// init channel provider for pvAccess
	ClientFactory::start();
    ChannelProvider::shared_pointer channelProvider = getChannelProviderRegistry()->getProvider("pva");

    // Now request a channel connection
    BrainDeadChannelRequesterPtr channelRequester = BrainDeadChannelRequesterPtr(new BrainDeadChannelRequester());
    Channel::shared_pointer channel = channelProvider->createChannel(pvasource->pv_name, channelRequester, 0);
//    cout << channel << endl;
    channelRequester->event.wait();
    if (!channelRequester->status.isOK()) return false;
//    cout << "got request" << endl;

    // Connect to the channel
    CreateRequest::shared_pointer createRequest = CreateRequest::create();
    PVStructurePtr pvRequest = createRequest->createRequest("record[queueSize=50]field()");
    if(pvRequest==NULL) return false;
//    cout << "got connect" << endl;

    // Create the monitor
    pvasource->monitorRequester = BrainDeadMonitorRequesterPtr(new BrainDeadMonitorRequester());
    channel->createMonitor(pvasource->monitorRequester, pvRequest);
//    cout << "got monitor" << endl;
	return TRUE;
}

static gboolean
gst_pvasource_stop (GstBaseSrc * src)
{
	GstPVASource *pvasource = GST_PVASOURCE (src);

	GST_DEBUG_OBJECT (pvasource, "stop");
	GST_WARNING_OBJECT(pvasource, "stop");

	//destroy pv object and monitor here

	return TRUE;
}


static GstFlowReturn
gst_pvasource_create (GstPushSrc * src, GstBuffer ** buf)
{
	GstPVASource *pvasource = GST_PVASOURCE (src);
	GST_WARNING_OBJECT(pvasource, pvasource->pv_name);
	// get the latest value from the buffer and fill and return the
	// gst buffer here

	// wait for a new frame
	pvasource->monitorRequester->event.wait();

	// extract it
	const PVStructurePtr & pvStructurePtr = pvasource->monitorRequester->element->pvStructurePtr;

	// get the width and height
    PVStructureArrayPtr dimensionField = pvStructurePtr->getStructureArrayField("dimension");
    int ndims = dimensionField->getLength();
    if (ndims == 0)
    {
        std::cerr << "ndims = 0:" << std::endl;
        return GST_FLOW_NOT_SUPPORTED;
    }
    int width = dimensionField->view()[0]->getIntField("size")->get();
    int height = dimensionField->view()[1]->getIntField("size")->get();

    // notify change in image size
    if ((width != pvasource->width) || (height != pvasource->height)) {
    	pvasource->width = width;
    	pvasource->height = height;
    	GstCaps *caps = gst_caps_new_simple ("video/x-raw",
    			      "format", G_TYPE_STRING, "GRAY8",
    			      "width", G_TYPE_INT, width,
    			      "height", G_TYPE_INT, height,
    			      "framerate", GST_TYPE_FRACTION, 0, 1,
    			      NULL);
    	gst_base_src_set_caps(GST_BASE_SRC(src), caps);
    }

    // output buffer
    PVByteArrayPtr byteValueField = tr1::static_pointer_cast<PVByteArray>(pvStructurePtr->getSubField("value"));;
    PVByteArray::const_svector data = byteValueField->view();

    // get timestamps
	PVStructure::shared_pointer dataTimeStampField = pvStructurePtr->getStructureField("dataTimeStamp");
	long seconds = dataTimeStampField->getLongField("secondsPastEpoch")->get();
	int ns = dataTimeStampField->getIntField("nanoseconds")->get();
	unsigned long timestamp_ns = seconds * 1000000000 + ns;

	/* make empty buffer */
#if 1
    //cout << "Produce" << endl;
	*buf = gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY,
			(void*)&data[0], width*height, 0, width*height,
			new WrappedElement(pvasource->monitorRequester->element, pvasource->monitorRequester->monitor),
			element_destroy);
#else
	*buf = gst_buffer_new();

	/* allocate width*height bytes */
	GstMemory *mem = gst_allocator_alloc (NULL, width*height, NULL);

	/* Add to buffer */
	gst_buffer_append_memory (*buf, mem);

	/* get access to the memory in write mode */
	GstMapInfo info;
	unsigned int i;
	gst_memory_map (mem, &info, GST_MAP_WRITE);

	/* fill with pattern */
	for (i = 0; i < info.size; i++)
		info.data[i] = (i + pvasource->off) % 255;
	pvasource->off++;

	/* release memory */
	gst_memory_unmap (mem, &info);

	pvasource->monitorRequester->monitor->release(pvasource->monitorRequester->element);

#endif
	if (!gst_base_src_get_do_timestamp(GST_BASE_SRC(src))) {
		if (pvasource->timestamp_offset == 0) {
			pvasource->timestamp_offset = timestamp_ns;
			pvasource->last_timestamp = timestamp_ns;
		}

		GST_BUFFER_PTS (*buf) = timestamp_ns - pvasource->timestamp_offset;
		//GST_BUFFER_DTS (*buf) = GST_BUFFER_PTS (*buf);
		//cout << "pts " << GST_BUFFER_PTS (*buf) << endl;
		GST_BUFFER_DURATION (*buf) = (timestamp_ns - pvasource->last_timestamp);
		//cout << "duration " << GST_BUFFER_DURATION (*buf) << endl;

		pvasource->last_timestamp = timestamp_ns;
	}
	return GST_FLOW_OK;
}

static void
gst_pvasource_class_init (GstPVASourceClass * klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);
	GstPushSrcClass *push_src_class = GST_PUSH_SRC_CLASS (klass);

	gobject_class->finalize = gst_pvasource_finalize;
	gobject_class->set_property = gst_pvasource_set_property;
	gobject_class->get_property = gst_pvasource_get_property;

	g_object_class_install_property
	(gobject_class,
			PROP_PV_NAME,
			g_param_spec_string ("pv-name",
					"PV name",
					"EPICS V4 PV Name serving up NTNDArrays",
					NULL,
					(GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

	/* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
	gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
			gst_static_pad_template_get (&gst_pvasource_src_template));

	gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
			"FIXME Long name", "Generic", "FIXME Description",
			"FIXME <fixme@example.com>");

	//base_src_class->get_caps = GST_DEBUG_FUNCPTR (gst_pvasource_get_caps);
	//base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_pvasource_set_caps);
	//base_src_class->fixate = GST_DEBUG_FUNCPTR (gst_pvasource_fixate);
	base_src_class->start = GST_DEBUG_FUNCPTR (gst_pvasource_start);
	base_src_class->stop = GST_DEBUG_FUNCPTR (gst_pvasource_stop);

	base_src_class->get_times = GST_DEBUG_FUNCPTR (gst_pvasource_get_times);

	push_src_class->create = GST_DEBUG_FUNCPTR (gst_pvasource_create);

	/*
	gobject_class->dispose = gst_pvasource_dispose;
	base_src_class->negotiate = GST_DEBUG_FUNCPTR (gst_pvasource_negotiate);
	base_src_class->get_size = GST_DEBUG_FUNCPTR (gst_pvasource_get_size);
	base_src_class->is_seekable = GST_DEBUG_FUNCPTR (gst_pvasource_is_seekable);
	base_src_class->unlock = GST_DEBUG_FUNCPTR (gst_pvasource_unlock);
	base_src_class->event = GST_DEBUG_FUNCPTR (gst_pvasource_event);
	base_src_class->do_seek = GST_DEBUG_FUNCPTR (gst_pvasource_do_seek);
	base_src_class->query = GST_DEBUG_FUNCPTR (gst_pvasource_query);
	base_src_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_pvasource_unlock_stop);
	base_src_class->prepare_seek_segment =
			GST_DEBUG_FUNCPTR (gst_pvasource_prepare_seek_segment);
			*/
}

static gboolean
plugin_init (GstPlugin * plugin)
{

	/* FIXME Remember to set the rank if it's an element that is meant
     to be autoplugged by decodebin. */
	return gst_element_register (plugin, "pvasource", GST_RANK_NONE,
			GST_TYPE_PVASOURCE);
}

/* FIXME: these are normally defined by the GStreamer build system.
   If you are creating an element to be included in gst-plugins-*,
   remove these, as they're always defined.  Otherwise, edit as
   appropriate for your external plugin package. */
#ifndef VERSION
#define VERSION "0.0.1"
#endif
#ifndef PACKAGE
#define PACKAGE "PVAsource"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "PVAsource"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://controls.diamond.ac.uk"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
		GST_VERSION_MINOR,
		pvasource,
		"FIXME plugin description",
		plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)


#if 0
static gboolean
gst_pvasource_get_size (GstBaseSrc * src, guint64 * size)
{
	GstPVASource *pvasource = GST_PVASOURCE (src);

	GST_DEBUG_OBJECT (pvasource, "get_size");

	return TRUE;
}

static gboolean
gst_pvasource_is_seekable (GstBaseSrc * src)
{
	GstPVASource *pvasource = GST_PVASOURCE (src);

	GST_DEBUG_OBJECT (pvasource, "is_seekable");

	return FALSE;
}

static gboolean
gst_pvasource_unlock (GstBaseSrc * src)
{
	GstPVASource *pvasource = GST_PVASOURCE (src);

	GST_DEBUG_OBJECT (pvasource, "unlock");

	return TRUE;
}

static gboolean
gst_pvasource_event (GstBaseSrc * src, GstEvent * event)
{
	GstPVASource *pvasource = GST_PVASOURCE (src);

	GST_DEBUG_OBJECT (pvasource, "event");

	return TRUE;
}

static gboolean
gst_pvasource_do_seek (GstBaseSrc * src, GstSegment * segment)
{
	GstPVASource *pvasource = GST_PVASOURCE (src);

	GST_DEBUG_OBJECT (pvasource, "do_seek");

	return FALSE;
}

static gboolean
gst_pvasource_query (GstBaseSrc * src, GstQuery * query)
{
	GstPVASource *pvasource = GST_PVASOURCE (src);

	GST_DEBUG_OBJECT (pvasource, "query");

	return TRUE;
}

static gboolean
gst_pvasource_unlock_stop (GstBaseSrc * src)
{
	GstPVASource *pvasource = GST_PVASOURCE (src);

	GST_DEBUG_OBJECT (pvasource, "stop");

	return TRUE;
}

static gboolean
gst_pvasource_prepare_seek_segment (GstBaseSrc * src, GstEvent * seek,
		GstSegment * segment)
{
	GstPVASource *pvasource = GST_PVASOURCE (src);

	GST_DEBUG_OBJECT (pvasource, "seek_segment");

	return FALSE;
}


static gboolean
gst_pvasource_negotiate (GstBaseSrc * src)
{
	GstPVASource *pvasource = GST_PVASOURCE (src);

	GST_DEBUG_OBJECT (pvasource, "negotiate");

	return TRUE;
}

void
gst_pvasource_dispose (GObject * object)
{
	GstPVASource *pvasource = GST_PVASOURCE (object);

	GST_DEBUG_OBJECT (pvasource, "dispose");

	/* clean up as possible.  may be called multiple times */

	G_OBJECT_CLASS (gst_pvasource_parent_class)->dispose (object);
}


#endif
