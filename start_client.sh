export PATH=$PATH:/dls_sw/work/tools/RHEL6-x86_64/gstreamer/prefix/bin
export GST_PLUGIN_PATH=/home/tmc43/test/gstreamer-v4-plugin/PVASource:/dls_sw/work/tools/RHEL6-x86_64/gst-plugins-base/prefix/lib:/dls_sw/work/tools/RHEL6-x86_64/gst-plugins-good/prefix/lib:/dls_sw/work/tools/RHEL6-x86_64/gst-plugins-bad/prefix/lib:/dls_sw/work/tools/RHEL6-x86_64/gst-libav/prefix/lib 

gst-launch-1.0 udpsrc port=5000 $(cat ~/rtpcaps) ! rtpmp4vdepay ! avdec_mpeg4 !  autovideosink


