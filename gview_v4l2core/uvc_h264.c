/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#                             Add UYVY color support(Macbook iSight)            #
#                                                                               #
# This program is free software; you can redistribute it and/or modify          #
# it under the terms of the GNU General Public License as published by          #
# the Free Software Foundation; either version 2 of the License, or             #
# (at your option) any later version.                                           #
#                                                                               #
# This program is distributed in the hope that it will be useful,               #
# but WITHOUT ANY WARRANTY; without even the implied warranty of                #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 #
# GNU General Public License for more details.                                  #
#                                                                               #
# You should have received a copy of the GNU General Public License             #
# along with this program; if not, write to the Free Software                   #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA     #
#                                                                               #
********************************************************************************/

/* support for internationalization - i18n */
#include <math.h>
#include <libusb.h>
#include <stdlib.h>
#include <stdio.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "gview.h"
#include "../config.h"

/*h264 decoder (libavcodec)*/
#ifdef HAS_AVCODEC_H
  #include <avcodec.h>
#else
  #ifdef HAS_LIBAVCODEC_AVCODEC_H
    #include <libavcodec/avcodec.h>
  #else
    #ifdef HAS_FFMPEG_AVCODEC_H
      #include <ffmpeg/avcodec.h>
    #else
      #include <libavcodec/avcodec.h>
    #endif
  #endif
#endif

#define LIBAVCODEC_VER_AT_LEAST(major,minor)  (LIBAVCODEC_VERSION_MAJOR > major || \
                                              (LIBAVCODEC_VERSION_MAJOR == major && \
                                               LIBAVCODEC_VERSION_MINOR >= minor))

#if !LIBAVCODEC_VER_AT_LEAST(54,25)
	#define AV_CODEC_ID_H264 CODEC_ID_H264
#endif

#include "uvc_h264.h"
#include "v4l2_formats.h"

#define USB_VIDEO_CONTROL		    0x01
#define USB_VIDEO_CONTROL_INTERFACE	0x24
#define USB_VIDEO_CONTROL_XU_TYPE	0x06

// GUID of the UVC H.264 extension unit: {A29E7641-DE04-47E3-8B2B-F4341AFF003B}
#define GUID_UVCX_H264_XU {0x41, 0x76, 0x9E, 0xA2, 0x04, 0xDE, 0xE3, 0x47, 0x8B, 0x2B, 0xF4, 0x34, 0x1A, 0xFF, 0x00, 0x3B}

extern int verbosity;

typedef struct _h264_decoder_context_t
{
	AVCodec *codec;
	AVCodecContext *context;
	AVFrame *picture;

	int width;
	int height;
	int pic_size;

} h264_decoder_context_t;

static h264_decoder_context_t *h264_ctx = NULL;

/*h264 support type*/
static int h264_support = H264_NONE; /*none by default*/

/*
 * get h264 support type
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: support type (H264_NONE; H264_MUXED; H264_FRAME)
 */
int h264_get_support()
{
	return h264_support;
}

/*
 * print probe/commit data
 * args:
 *   data - pointer to probe/commit config data
 *
 * asserts:
 *   data is not null
 *
 * returns: void
 */
static void print_probe_commit_data(uvcx_video_config_probe_commit_t *data)
{
	/*asserts*/
	assert(data != NULL);

	printf("uvcx_video_config_probe_commit:\n");
	printf("\tFrameInterval: %i\n", data->dwFrameInterval);
	printf("\tBitRate: %i\n", data->dwBitRate);
	printf("\tHints: 0x%X\n", data->bmHints);
	printf("\tConfigurationIndex: %i\n", data->wConfigurationIndex);
	printf("\tWidth: %i\n", data->wWidth);
	printf("\tHeight: %i\n", data->wHeight);
	printf("\tSliceUnits: %i\n", data->wSliceUnits);
	printf("\tSliceMode: %i\n", data->wSliceMode);
	printf("\tProfile: %i\n", data->wProfile);
	printf("\tIFramePeriod: %i\n", data->wIFramePeriod);
	printf("\tEstimatedVideoDelay: %i\n",data->wEstimatedVideoDelay);
	printf("\tEstimatedMaxConfigDelay: %i\n",data->wEstimatedMaxConfigDelay);
	printf("\tUsageType: %i\n",data->bUsageType);
	printf("\tRateControlMode: %i\n",data->bRateControlMode);
	printf("\tTemporalScaleMode: %i\n",data->bTemporalScaleMode);
	printf("\tSpatialScaleMode: %i\n",data->bSpatialScaleMode);
	printf("\tSNRScaleMode: %i\n",data->bSNRScaleMode);
	printf("\tStreamMuxOption: %i\n",data->bStreamMuxOption);
	printf("\tStreamFormat: %i\n",data->bStreamFormat);
	printf("\tEntropyCABAC: %i\n",data->bEntropyCABAC);
	printf("\tTimestamp: %i\n",data->bTimestamp);
	printf("\tNumOfReorderFrames: %i\n",data->bNumOfReorderFrames);
	printf("\tPreviewFlipped: %i\n",data->bPreviewFlipped);
	printf("\tView: %i\n",data->bView);
	printf("\tReserved1: %i\n",data->bReserved1);
	printf("\tReserved2: %i\n",data->bReserved2);
	printf("\tStreamID: %i\n",data->bStreamID);
	printf("\tSpatialLayerRatio: %i\n",data->bSpatialLayerRatio);
	printf("\tLeakyBucketSize: %i\n",data->wLeakyBucketSize);
}

/*
 * resets the h264 encoder
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: 0 on success or error code on fail
 */
static int uvcx_video_encoder_reset(v4l2_dev_t *vd)
{
	/*assertions*/
	assert(vd != NULL);

	uvcx_encoder_reset encoder_reset_req = {0};

	int err = 0;

	if((err = query_xu_control(vd, vd->h264_unit_id, UVCX_ENCODER_RESET, UVC_SET_CUR, &encoder_reset_req)) < 0)
		fprintf(stderr, "V4L2_CORE: (UVCX_ENCODER_RESET) error: %s\n", strerror(errno));

	return err;
}

/*
 * probes the h264 encoder config
 * args:
 *   vd - pointer to video device data
 *   query - probe query
 *   uvcx_video_config - pointer to probe/commit config data
 *
 * asserts:
 *   vd is not null
 *
 * returns: 0 on success or error code on fail
 */
static int uvcx_video_probe(v4l2_dev_t *vd, uint8_t query, uvcx_video_config_probe_commit_t *uvcx_video_config)
{
	/*assertions*/
	assert(vd != NULL);

	int err = 0;


	if((err = query_xu_control(vd, vd->h264_unit_id, UVCX_VIDEO_CONFIG_PROBE, query, uvcx_video_config)) < 0)
		fprintf(stderr, "V4L2_CORE: (UVCX_VIDEO_CONFIG_PROBE) error: %s\n", strerror(errno));

	return err;
}

/*
 * commits the h264 encoder config
 * args:
 *   vd - pointer to video device data
 *   uvcx_video_config - pointer to probe/commit config data
 *
 * asserts:
 *   vd is not null
 *
 * returns: 0 on success or error code on fail
 */
static int uvcx_video_commit(v4l2_dev_t *vd, uvcx_video_config_probe_commit_t *uvcx_video_config)
{
	/*assertions*/
	assert(vd != NULL);

	int err = 0;

	if((err = query_xu_control(vd, vd->h264_unit_id, UVCX_VIDEO_CONFIG_COMMIT, UVC_SET_CUR, uvcx_video_config)) < 0)
		fprintf(stderr, "V4L2_CORE: (UVCX_VIDEO_CONFIG_COMMIT) error: %s\n", strerror(errno));

	return err;
}


/*
 * gets the uvc h264 xu control unit id, if any
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *   vd->list_devices is not null
 *
 * returns: unit id or 0 if none
 *  (also sets vd->h264_unit_id)
 */
uint8_t get_uvc_h624_unit_id (v4l2_dev_t *vd)
{
	/*asserts*/
	assert(vd != NULL);
	assert(vd->list_devices != NULL);

	uint64_t busnum = vd->list_devices[vd->this_device].busnum;
	uint64_t devnum = vd->list_devices[vd->this_device].devnum;

    /* use libusb */
	libusb_context *usb_ctx = NULL;
    libusb_device **device_list = NULL;
    libusb_device *device = NULL;
    ssize_t cnt;
    int i, j, k;

	static const uint8_t guid[16] = GUID_UVCX_H264_XU;
	vd->h264_unit_id = 0;/*reset it*/

    if (usb_ctx == NULL)
      libusb_init (&usb_ctx);

    cnt = libusb_get_device_list (usb_ctx, &device_list);
    for (i = 0; i < cnt; i++)
    {
		uint64_t dev_busnum = libusb_get_bus_number (device_list[i]);
		uint64_t dev_devnum = libusb_get_device_address (device_list[i]);
		if (busnum == dev_busnum &&	devnum == dev_devnum)
		{
			device = libusb_ref_device (device_list[i]);
			break;
		}
	}

	libusb_free_device_list (device_list, 1);

	if (device)
	{
		if(verbosity > 1)
			printf("V4L2_CORE: (libusb) checking for H264 unit id\n");
		struct libusb_device_descriptor desc;

		 if (libusb_get_device_descriptor (device, &desc) == 0)
		 {
			for (i = 0; i < desc.bNumConfigurations; ++i)
			{
				struct libusb_config_descriptor *config = NULL;

				if (libusb_get_config_descriptor (device, i, &config) == 0)
				{
					for (j = 0; j < config->bNumInterfaces; j++)
					{
						for (k = 0; k < config->interface[j].num_altsetting; k++)
						{
							const struct libusb_interface_descriptor *interface;
							const uint8_t *ptr = NULL;

							interface = &config->interface[j].altsetting[k];
							if (interface->bInterfaceClass != LIBUSB_CLASS_VIDEO ||
								interface->bInterfaceSubClass != USB_VIDEO_CONTROL)
								continue;
							ptr = interface->extra;
							while (ptr - interface->extra +
								sizeof (xu_descriptor) < interface->extra_length)
							{
								xu_descriptor *desc = (xu_descriptor *) ptr;

								if (desc->bDescriptorType == USB_VIDEO_CONTROL_INTERFACE &&
									desc->bDescriptorSubType == USB_VIDEO_CONTROL_XU_TYPE &&
									memcmp (desc->guidExtensionCode, guid, 16) == 0)
								{
									vd->h264_unit_id = desc->bUnitID;

									libusb_unref_device (device);
									/*it's a match*/
									if(verbosity > 1)
										printf("V4L2_CORE: (libusb) found H264 unit id %i\n", vd->h264_unit_id);
									return vd->h264_unit_id;
								}
								ptr += desc->bLength;
							}
						}
					}
				}
				else
					fprintf(stderr, "V4L2_CORE: (libusb) couldn't get config descriptor for configuration %i\n", i);
			}
		}
		else
			fprintf(stderr, "V4L2_CORE: (libusb) couldn't get device descriptor\n");
		libusb_unref_device (device);
	}
	else
		fprintf(stderr, "V4L2_CORE: (libusb) couldn't get device\n");
	/*no match found*/
	return vd->h264_unit_id;
}

/*
 * check for uvc h264 support by querying UVCX_VERSION
 * although geting a unit id > 0 from xu_get_unit_id
 * should be enought
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *   vd->fd is valid ( > 0 )
 *
 * returns: 1 if support available or 0 otherwise
 */
int check_h264_support(v4l2_dev_t *vd)
{
	/*assertions*/
	assert(vd != NULL);
	assert(vd->fd > 0);

	if(vd->h264_unit_id <= 0)
	{
		if(verbosity > 0)
			printf("V4L2_CORE: device doesn't seem to support uvc H264 (%i)\n", vd->h264_unit_id);
		return 0;
	}

	uvcx_version_t uvcx_version;

	if(query_xu_control(vd, vd->h264_unit_id, UVCX_VERSION, UVC_GET_CUR, &uvcx_version) < 0)
	{
		if(verbosity > 0)
			printf("V4L2_CORE: device doesn't seem to support uvc H264 in unit_id %d\n", vd->h264_unit_id);
		return 0;
	}

	if(verbosity > 0)
		printf("V4L2_CORE: device seems to support uvc H264 (version: %d) in unit_id %d\n", uvcx_version.wVersion, vd->h264_unit_id);
	return 1;

}

/*
 * adds h264 to the format list, if supported by device
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *   vd->list_stream_formats is not null
 *
 * returns: void
 */
void add_h264_format(v4l2_dev_t *vd)
{
	/*assertions*/
	assert(vd != NULL);
	assert(vd->list_stream_formats != NULL);

	if(verbosity > 0)
		printf("V4L2_CORE: checking muxed H264 format support\n");

	if(get_frame_format_index(vd, V4L2_PIX_FMT_H264) >= 0)
	{
		if(verbosity > 0)
			printf("V4L2_CORE: H264 format already in list\n");

		h264_support = H264_FRAME;
		return; /*H264 is already in the list*/
	}

	if(get_uvc_h624_unit_id(vd) <= 0)
	{
		h264_support = H264_NONE;
		return; /*no unit id found for h264*/
	}

	if(!check_h264_support(vd))
	{
		h264_support = H264_NONE;
		return; /*no XU support for h264*/
	}

	int mjpg_index = get_frame_format_index(vd, V4L2_PIX_FMT_MJPEG);
	if(mjpg_index < 0) /*MJPG must be available for muxed uvc H264 streams*/
		return;

	/*add the format to the list*/
	if(verbosity > 0)
		printf("V4L2_CORE: adding muxed H264 format\n");

	/*if we got here then muxed h264 is supported*/
	h264_support = H264_MUXED;

	vd->numb_formats++; /*increment number of formats*/
	int fmtind = vd->numb_formats;

	vd->list_stream_formats = realloc(
		vd->list_stream_formats,
		fmtind * sizeof(v4l2_stream_formats_t));
	vd->list_stream_formats[fmtind-1].format = V4L2_PIX_FMT_H264;
	snprintf(vd->list_stream_formats[fmtind-1].fourcc , 5, "H264");
	vd->list_stream_formats[fmtind-1].list_stream_cap = NULL;
	vd->list_stream_formats[fmtind-1].numb_res = 0;


	/*add MJPG resolutions and frame rates for H264*/
	int numb_res = vd->list_stream_formats[mjpg_index].numb_res;

	int i=0, j=0;
	int res_index = 0;
	for(i=0; i < numb_res; i++)
	{
		int width = vd->list_stream_formats[mjpg_index].list_stream_cap[i].width;
		int height = vd->list_stream_formats[mjpg_index].list_stream_cap[i].height;

		res_index++;
		vd->list_stream_formats[fmtind-1].list_stream_cap = realloc(
			vd->list_stream_formats[fmtind-1].list_stream_cap,
			res_index * sizeof(v4l2_stream_cap_t));
		vd->list_stream_formats[fmtind-1].numb_res = res_index;
		vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].width = width;
		vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].height = height;
		vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].framerate_num = NULL;
		vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].framerate_denom = NULL;
		vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].numb_frates = 0;

		/*add frates*/
		int numb_frates =  vd->list_stream_formats[mjpg_index].list_stream_cap[i].numb_frates;
		int frate_index = 0;
		for(j=0; j < numb_frates; j++)
		{
			int framerate_num = vd->list_stream_formats[mjpg_index].list_stream_cap[i].framerate_num[j];
			int framerate_denom = vd->list_stream_formats[mjpg_index].list_stream_cap[i].framerate_denom[j];

			frate_index++;
			vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].numb_frates = frate_index;
			vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].framerate_num = realloc(
				vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].framerate_num,
				frate_index * sizeof(int));
			vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].framerate_num[frate_index-1] = framerate_num;
			vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].framerate_denom = realloc(
				vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].framerate_denom,
				frate_index * sizeof(int));
			vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].framerate_denom[frate_index-1] = framerate_denom;
		}
	}
}

/*
 * sets h264 muxed format (must not be called while streaming)
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: void
 */
void set_h264_muxed_format(v4l2_dev_t *vd)
{
	uvcx_video_config_probe_commit_t *config_probe_req = &(vd->h264_config_probe_req);

	/* reset the encoder*/
	uvcx_video_encoder_reset(vd);

	/*
	 * Get default values (safe)
	 */
	uvcx_video_probe(vd, UVC_GET_DEF, config_probe_req);

	/*set resolution*/
	config_probe_req->wWidth = vd->format.fmt.pix.width;
	config_probe_req->wHeight = vd->format.fmt.pix.height;
	/*set frame rate in 100ns units*/
	uint32_t frame_interval = (vd->fps_num * 1000000000LL / vd->fps_denom)/100;
	config_probe_req->dwFrameInterval = frame_interval;

	/*set the aux stream (h264)*/
	config_probe_req->bStreamMuxOption = STREAMMUX_H264;

	/*probe the format*/
	uvcx_video_probe(vd, UVC_SET_CUR, config_probe_req);
	uvcx_video_probe(vd, UVC_GET_CUR, config_probe_req);

	if(config_probe_req->wWidth != vd->format.fmt.pix.width)
	{
		fprintf(stderr, "V4L2_CORE: H264 config probe: requested width %i but got %i\n",
			vd->format.fmt.pix.width, config_probe_req->wWidth);

		vd->format.fmt.pix.width = config_probe_req->wWidth;
	}
	if(config_probe_req->wHeight != vd->format.fmt.pix.height)
	{
		fprintf(stderr, "V4L2_CORE: H264 config probe: requested height %i but got %i\n",
			vd->format.fmt.pix.height, config_probe_req->wHeight);

		vd->format.fmt.pix.height = config_probe_req->wHeight;
	}
	if(config_probe_req->dwFrameInterval != frame_interval)
	{
		fprintf(stderr, "V4L2_CORE: H264 config probe: requested frame interval %i but got %i\n",
			frame_interval, config_probe_req->dwFrameInterval);
	}
	/*commit the format*/
	uvcx_video_commit(vd, config_probe_req);

	/*print probe/commit data*/
	if(verbosity > 0)
		print_probe_commit_data(config_probe_req);
}

/*
 * request a frame of type wPictureType to the h264 encoder
 * args:
 *   vd - pointer to video device data
 *   type - frame type
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code
 */
int request_h264_frame_type(v4l2_dev_t *vd, uint16_t type)
{
	/*asserts*/
	assert(vd != NULL);

	if(vd->h264_unit_id <= 0)
	{
		if(verbosity > 0)
			printf("V4L2_CORE: device doesn't seem to support uvc H264 (%i)\n", vd->h264_unit_id);
		return E_NO_STREAM_ERR;
	}

	uvcx_picture_type_control_t picture_type_req;
	picture_type_req.wLayerID = 0;
	picture_type_req.wPicType = type;

	int err = E_OK;

	if((err = query_xu_control(vd, vd->h264_unit_id, UVCX_PICTURE_TYPE_CONTROL, UVC_SET_CUR, &picture_type_req)) < 0)
	{
		fprintf(stderr, "V4L2_CORE: (UVCX_PICTURE_TYPE_CONTROL) SET_CUR error: %s\n", strerror(errno));
	}

	return err;
}

/*
 * get the video rate control mode
 * args:
 *   vd - pointer to video device data
 *   query - query type
 *
 * asserts:
 *   vd is not null
 *
 * returns: video rate control mode (FIXME: 0xff on error)
 */
uint8_t get_h264_video_rate_control_mode(v4l2_dev_t *vd, uint8_t query)
{
	/*asserts*/
	assert(vd != NULL);

	if(vd->h264_unit_id <= 0)
	{
		if(verbosity > 0)
			printf("V4L2_CORE: device doesn't seem to support uvc H264 (%i)\n", vd->h264_unit_id);
		return 0xff;
	}

	uvcx_rate_control_mode_t rate_control_mode_req;
	rate_control_mode_req.wLayerID = 0;

	if((query_xu_control(vd, vd->h264_unit_id, UVCX_RATE_CONTROL_MODE, query, &rate_control_mode_req)) < 0)
	{
		fprintf(stderr, "V4L2_CORE: (UVCX_RATE_CONTROL_MODE) query (%u) error: %s\n", query, strerror(errno));
		return 0xff;
	}

	return rate_control_mode_req.bRateControlMode;
}

/*
 * set the video rate control mode
 * args:
 *   vd - pointer to video device data
 *   mode - rate mode
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code ( 0 -OK)
 */
int set_h264_video_rate_control_mode(v4l2_dev_t *vd, uint8_t mode)
{
	/*asserts*/
	assert(vd != NULL);

	if(vd->h264_unit_id <= 0)
	{
		if(verbosity > 0)
			printf("V4L2_CORE: device doesn't seem to support uvc H264 (%i)\n", vd->h264_unit_id);
		return E_NO_STREAM_ERR;
	}

	uvcx_rate_control_mode_t rate_control_mode_req;
	rate_control_mode_req.wLayerID = 0;
	rate_control_mode_req.bRateControlMode = mode;

	int err = E_OK;

	if((err = query_xu_control(vd, vd->h264_unit_id, UVCX_RATE_CONTROL_MODE, UVC_SET_CUR, &rate_control_mode_req)) < 0)
	{
		fprintf(stderr, "V4L2_CORE: (UVCX_RATE_CONTROL_MODE) SET_CUR error: %s\n", strerror(errno));
	}

	return err;
}

/*
 * get the temporal scale mode
 * args:
 *   vd - pointer to video device data
 *   query - query type
 *
 * asserts:
 *   vd is not null
 *
 * returns: temporal scale mode (FIXME: 0xff on error)
 */
uint8_t get_h264_temporal_scale_mode(v4l2_dev_t *vd, uint8_t query)
{
	/*asserts*/
	assert(vd != NULL);

	if(vd->h264_unit_id <= 0)
	{
		if(verbosity > 0)
			printf("V4L2_CORE: device doesn't seem to support uvc H264 (%i)\n", vd->h264_unit_id);
		return 0xff;
	}

	uvcx_temporal_scale_mode_t temporal_scale_mode_req;
	temporal_scale_mode_req.wLayerID = 0;

	int err = 0;

	if((err = query_xu_control(vd, vd->h264_unit_id, UVCX_TEMPORAL_SCALE_MODE, query, &temporal_scale_mode_req)) < 0)
	{
		fprintf(stderr, "V4L2_CORE: (UVCX_TEMPORAL_SCALE_MODE) query (%u) error: %s\n", query, strerror(errno));
		return 0xff;
	}

	return temporal_scale_mode_req.bTemporalScaleMode;
}

/*
 * set the temporal scale mode
 * args:
 *   vd - pointer to video device data
 *   mode - temporal scale mode
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code ( 0 -OK)
 */
int set_h264_temporal_scale_mode(v4l2_dev_t *vd, uint8_t mode)
{
	/*asserts*/
	assert(vd != NULL);

	if(vd->h264_unit_id <= 0)
	{
		if(verbosity > 0)
			printf("V4L2_CORE: device doesn't seem to support uvc H264 (%i)\n", vd->h264_unit_id);
		return E_NO_STREAM_ERR;
	}

	uvcx_temporal_scale_mode_t temporal_scale_mode_req;
	temporal_scale_mode_req.wLayerID = 0;
	temporal_scale_mode_req.bTemporalScaleMode = mode;

	int err = 0;

	if((err = query_xu_control(vd, vd->h264_unit_id, UVCX_TEMPORAL_SCALE_MODE, UVC_SET_CUR, &temporal_scale_mode_req)) < 0)
	{
		fprintf(stderr, "V4L2_CORE: (UVCX_TEMPORAL_SCALE_MODE) SET_CUR error: %s\n", strerror(errno));
	}

	return err;
}

/*
 * get the spatial scale mode
 * args:
 *   vd - pointer to video device data
 *   query - query type
 *
 * asserts:
 *   vd is not null
 *
 * returns: temporal scale mode (FIXME: 0xff on error)
 */
uint8_t get_h264_spatial_scale_mode(v4l2_dev_t *vd, uint8_t query)
{
	/*asserts*/
	assert(vd != NULL);

	if(vd->h264_unit_id <= 0)
	{
		if(verbosity > 0)
			printf("V4L2_CORE: device doesn't seem to support uvc H264 (%i)\n", vd->h264_unit_id);
		return 0xff;
	}

	uvcx_spatial_scale_mode_t spatial_scale_mode_req;
	spatial_scale_mode_req.wLayerID = 0;

	int err = 0;

	if((err = query_xu_control(vd, vd->h264_unit_id, UVCX_SPATIAL_SCALE_MODE, query, &spatial_scale_mode_req)) < 0)
	{
		fprintf(stderr, "V4L2_CORE: (UVCX_SPATIAL_SCALE_MODE) query (%u) error: %s\n", query, strerror(errno));
		return 0xff;
	}

	return spatial_scale_mode_req.bSpatialScaleMode;
}

/*
 * set the spatial scale mode
 * args:
 *   vd - pointer to video device data
 *   mode - spatial scale mode
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code ( 0 -OK)
 */
int set_h264_spatial_scale_mode(v4l2_dev_t *vd, uint8_t mode)
{
	/*asserts*/
	assert(vd != NULL);

	if(vd->h264_unit_id <= 0)
	{
		if(verbosity > 0)
			printf("V4L2_CORE: device doesn't seem to support uvc H264 (%i)\n", vd->h264_unit_id);
		return E_NO_STREAM_ERR;
	}

	uvcx_spatial_scale_mode_t spatial_scale_mode_req;
	spatial_scale_mode_req.wLayerID = 0;
	spatial_scale_mode_req.bSpatialScaleMode = mode;

	int err = 0;

	if((err = query_xu_control(vd, vd->h264_unit_id, UVCX_SPATIAL_SCALE_MODE, UVC_SET_CUR, &spatial_scale_mode_req)) < 0)
	{
		fprintf(stderr, "V4L2_CORE: (UVCX_SPATIAL_SCALE_MODE) SET_CUR error: %s\n", strerror(errno));
	}

	return err;
}

/*
 * get the frame rate config
 * args:
 *   vd - pointer to video device data
 *   query - query type
 *
 * asserts:
 *   vd is not null
 *
 * returns: temporal scale mode (FIXME: 0xffffffff on error)
 */
uint32_t uvcx_get_frame_rate_config(v4l2_dev_t *vd, uint8_t query)
{
	/*asserts*/
	assert(vd != NULL);

	if(vd->h264_unit_id <= 0)
	{
		if(verbosity > 0)
			printf("V4L2_CORE: device doesn't seem to support uvc H264 (%i)\n", vd->h264_unit_id);
		return 0xffffffff;
	}

	uvcx_framerate_config_t framerate_req;
	framerate_req.wLayerID = 0;

	int err = 0;

	if((err = query_xu_control(vd, vd->h264_unit_id, UVCX_FRAMERATE_CONFIG, query, &framerate_req)) < 0)
	{
		fprintf(stderr, "V4L2_CORE: (UVCX_FRAMERATE_CONFIG) query (%u) error: %s\n", query, strerror(errno));
		return 0xffffffff;
	}

	return framerate_req.dwFrameInterval;
}

/*
 * set the frame rate config
 * args:
 *   vd - pointer to video device data
 *   framerate - framerate
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code ( 0 -OK)
 */
int set_h264_frame_rate_config(v4l2_dev_t *vd, uint32_t framerate)
{
	/*asserts*/
	assert(vd != NULL);

	if(vd->h264_unit_id <= 0)
	{
		if(verbosity > 0)
			printf("V4L2_CORE: device doesn't seem to support uvc H264 (%i)\n", vd->h264_unit_id);
		return E_NO_STREAM_ERR;
	}

	uvcx_framerate_config_t framerate_req;
	framerate_req.wLayerID = 0;
	framerate_req.dwFrameInterval = framerate;

	int err = 0;

	if((err = query_xu_control(vd, vd->h264_unit_id, UVCX_FRAMERATE_CONFIG, UVC_SET_CUR, &framerate_req)) < 0)
	{
		fprintf(stderr, "V4L2_CORE: (UVCX_FRAMERATE_CONFIG) SET_CUR error: %s\n", strerror(errno));
	}

	return err;
}

/*
 * ############# H264 decoder ##############
 */

/*
 * check if h264 decoder is available from libavcodec
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: TRUE (1)
 *          FALSE(0)
 */
uint8_t h264_has_decoder()
{
	if(avcodec_find_decoder(CODEC_ID_H264))
		return TRUE;
	else
		return FALSE;
}

/*
 * init h264 decoder context
 * args:
 *    width - image width
 *    height - image height
 *
 * asserts:
 *    none
 *
 * returns: error code (0 - E_OK)
 */
int h264_init_decoder(int width, int height)
{
#if !LIBAVCODEC_VER_AT_LEAST(53,34)
	avcodec_init();
#endif
	/*
	 * register all the codecs (we can also register only the codec
	 * we wish to have smaller code)
	 */
	avcodec_register_all();

	if(h264_ctx != NULL)
		h264_close_decoder();

	h264_ctx = calloc(1, sizeof(h264_decoder_context_t));

	h264_ctx->codec = avcodec_find_decoder(CODEC_ID_H264);
	if(!h264_ctx->codec)
	{
		fprintf(stderr, "V4L2_CORE: (H264 decoder) codec not found (please install libavcodec-extra for H264 support)\n");
		free(h264_ctx);
		h264_ctx = NULL;
		return E_NO_CODEC;
	}

#if LIBAVCODEC_VER_AT_LEAST(53,6)
	h264_ctx->context = avcodec_alloc_context3(h264_ctx->codec);
	avcodec_get_context_defaults3 (h264_ctx->context, h264_ctx->codec);
#else
	h264_ctx->context = avcodec_alloc_context();
	avcodec_get_context_defaults(h264_ctx->context);
#endif


	h264_ctx->context->flags2 |= CODEC_FLAG2_FAST;
	h264_ctx->context->pix_fmt = PIX_FMT_YUV420P;
	h264_ctx->context->width = width;
	h264_ctx->context->height = height;
	//h264_ctx->context->dsp_mask = (FF_MM_MMX | FF_MM_MMXEXT | FF_MM_SSE);

#if LIBAVCODEC_VER_AT_LEAST(53,6)
	if (avcodec_open2(h264_ctx->context, h264_ctx->codec, NULL) < 0)
#else
	if (avcodec_open(h264_ctx->context, h264_ctx->codec) < 0)
#endif
	{
		fprintf(stderr, "V4L2_CORE: (H264 decoder) couldn't open codec\n");
		avcodec_close(h264_ctx->context);
		free(h264_ctx->context);
		free(h264_ctx);
		h264_ctx = NULL;
		return E_NO_CODEC;
	}

	h264_ctx->picture = avcodec_alloc_frame();
	avcodec_get_frame_defaults(h264_ctx->picture);
	h264_ctx->pic_size = avpicture_get_size(h264_ctx->context->pix_fmt, width, height);
	h264_ctx->width = width;
	h264_ctx->height = height;

	return E_OK;
}

/*
 * decode h264 frame
 * args:
 *    out_buf - pointer to decoded data
 *    in_buf - pointer to h264 data
 *    size - in_buf size
 *
 * asserts:
 *    h264_ctx is not null
 *    in_buf is not null
 *    out_buf is not null
 *
 * returns: decoded data size
 */
int h264_decode(uint8_t *out_buf, uint8_t *in_buf, int size)
{
	/*asserts*/
	assert(h264_ctx != NULL);
	assert(in_buf != NULL);
	assert(out_buf != NULL);

	AVPacket avpkt;

	avpkt.size = size;
	avpkt.data = in_buf;

	int got_picture = 0;
	int len = avcodec_decode_video2(h264_ctx->context, h264_ctx->picture, &got_picture, &avpkt);

	if(len < 0)
	{
		fprintf(stderr, "V4L2_CORE: (H264 decoder) error while decoding frame\n");
		return len;
	}

	if(got_picture)
	{
		avpicture_layout((AVPicture *) h264_ctx->picture, h264_ctx->context->pix_fmt
			, h264_ctx->width, h264_ctx->height, out_buf, h264_ctx->pic_size);
		return len;
	}
	else
		return 0;

}

/*
 * close h264 decoder context
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void h264_close_decoder()
{
	if(h264_ctx == NULL)
		return;

	avcodec_close(h264_ctx->context);

	free(h264_ctx->context);
	free(h264_ctx->picture);

	free(h264_ctx);

	h264_ctx = NULL;
}

