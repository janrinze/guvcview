/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#                             Add UYVY color support(Macbook iSight)            #
#           Flemming Frandsen <dren.dk@gmail.com>                               #
#                             Add VU meter OSD                                  #
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

#ifndef GUI_H
#define GUI_H

#include "gviewv4l2core.h"

#define GUI_NONE   (0)
#define GUI_GTK3   (1)

#define DEF_ACTION_IMAGE  (0)
#define DEF_ACTION_VIDEO  (1)

/*
 * gets the default camera button action
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: default camera button action
 */
int get_default_camera_button_action();

/*
 * sets the default camera button action
 * args:
 *   action: camera button default action
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void set_default_camera_button_action(int action);

/*
 * gets the control profile file name
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: control profile file name
 */
const char *get_profile_name();

/*
 * sets the control profile file name
 * args:
 *   name: control profile file name
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void set_profile_name(const char *name);

/*
 * gets the control profile path (to dir)
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: control profile file name
 */
const char *get_profile_path();

/*
 * sets the control profile path (to dir)
 * args:
 *   path: control profile path
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void set_profile_path(const char *path);

/*
 * gets video sufix flag
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: video sufix flag
 */
int get_video_sufix_flag();

/*
 * sets the video sufix flag
 * args:
 *   flag: video sufix flag
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void set_video_sufix_flag(int flag);

/*
 * gets video muxer
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: video muxer
 */
int get_video_muxer();

/*
 * sets video muxer
 * args:
 *   muxer - video muxer (ENCODER_MUX_[MKV|WEBM|AVI])
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void set_video_muxer(int muxer);

/*
 * gets the video file basename
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: video file basename
 */
const char *get_video_name();

/*
 * sets the video file basename
 * args:
 *   name: video file basename
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void set_video_name(const char *name);

/*
 * gets the video file path (to dir)
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: video file path
 */
const char *get_video_path();

/*
 * sets video path (to dir)
 * args:
 *   path: video file path
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void set_video_path(const char *path);

/*
 * gets photo sufix flag
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: photo sufix flag
 */
int get_photo_sufix_flag();

/*
 * sets the photo sufix flag
 * args:
 *   flag: photo sufix flag
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void set_photo_sufix_flag(int flag);

/*
 * gets photo format
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: photo format
 */
int get_photo_format();

/*
 * sets photo format
 * args:
 *   format - photo format (IMG_FMT_[JPG|BMP|PNG|RAW])
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void set_photo_format(int format);

/*
 * gets the photo file basename
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: photo file basename
 */
const char *get_photo_name();

/*
 * sets the photo file basename
 * args:
 *   name: photo file basename
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void set_photo_name(const char *name);

/*
 * gets the photo file path (to dir)
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: photo file path
 */
const char *get_photo_path();

/*
 * sets photo path (to dir)
 * args:
 *   path: photo file path
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void set_photo_path(const char *path);

/*
 * sets the Image capture button label
 * args:
 *   label: Image capture button label
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void gui_set_image_capture_button_label(const char *label);

/*
 * GUI initialization
 * args:
 *   device - pointer to device data we want to attach the gui for
 *   gui - gui API to use (GUI_NONE, GUI_GTK3, ...)
 *   width - window width
 *   height - window height
 *   control_panel - flag control panel mode (1 -set; 0 -no)
 *
 * asserts:
 *   device is not null
 *
 * returns: error code
 */
int gui_attach(v4l2_dev_t *device, int gui, int width, int height, int control_panel);

/*
 * run the GUI loop
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: error code
 */
int gui_run();

/*
 * closes and cleans the GUI
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void gui_close();

#endif
