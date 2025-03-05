export PATH=$PATH:/dls_sw/work/tools/RHEL6-x86_64/gstreamer/prefix/bin
export GST_PLUGIN_PATH=/home/tmc43/test/gstreamer-v4-plugin/PVASource:/dls_sw/work/tools/RHEL6-x86_64/gst-plugins-base/prefix/lib:/dls_sw/work/tools/RHEL6-x86_64/gst-plugins-good/prefix/lib:/dls_sw/work/tools/RHEL6-x86_64/gst-plugins-bad/prefix/lib:/dls_sw/work/tools/RHEL6-x86_64/gst-libav/prefix/lib 

gst-launch-1.0 -v pvasource pv_name=adImage ! tee name=t ! videoconvert ! queue ! avenc_mpeg4 ! rtpmp4vpay ! udpsink host=127.0.0.1 port=5000 t. ! queue ! kaleidoscope ! videoconvert ! autovideosink | grep -m 1 -e  "rtpmp4vpay0.GstPad:src: .*config" | sed -e 's/.*\(caps.*\)/\1/' > ~/rtpcaps


