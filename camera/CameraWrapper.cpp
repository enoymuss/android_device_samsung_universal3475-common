/*
 * Copyright (C) 2017, The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_NDEBUG 0
#define LOG_PARAMETERS

#define LOG_TAG "CameraWrapper"
#include <cutils/log.h>

#include "CameraWrapper.h"
#include "Camera2Wrapper.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * j2lte: the legacy Exynos camera HAL leaves /dev/video101 (and/or video102)
 * open after get_number_of_cameras()/get_camera_info() probing. The FIMC-IS
 * driver then refuses every real open with "already open" (-24 / EMFILE).
 * Close those leaked fds, but only before the first real camera open so we
 * never touch a live camera.
 */
static bool gAnyCameraOpened = false;

static void close_leaked_video_fds(const char *why)
{
    if (gAnyCameraOpened)
        return;

    DIR *d = opendir("/proc/self/fd");
    if (!d)
        return;

    int dfd = dirfd(d);
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.')
            continue;
        int fd = atoi(e->d_name);
        if (fd == dfd)
            continue;

        char link[64];
        char target[128];
        snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
        ssize_t n = readlink(link, target, sizeof(target) - 1);
        if (n <= 0)
            continue;
        target[n] = '\0';

        if (!strcmp(target, "/dev/video101") || !strcmp(target, "/dev/video102")) {
            ALOGW("%s: closing leaked fd %d (%s)", why, fd, target);
            close(fd);
        }
    }
    closedir(d);
}

static int camera_device_open(const hw_module_t* module, const char* name,
                hw_device_t** device);
static int camera_get_number_of_cameras(void);
static int camera_get_camera_info(int camera_id, struct camera_info *info);
static int camera_set_callbacks(const camera_module_callbacks_t *callbacks);
static void camera_get_vendor_tag_ops(vendor_tag_ops_t* ops);
static int camera_open_legacy(const struct hw_module_t* module, const char* id, uint32_t halVersion, struct hw_device_t** device);
static int camera_set_torch_mode(const char* camera_id, bool enabled);

static camera_module_t *gVendorModule = 0;

static int check_vendor_module()
{
    int rv = 0;
    ALOGV("%s", __FUNCTION__);

    if(gVendorModule)
        return 0;

    rv = hw_get_module_by_class("camera", "vendor", (const hw_module_t **)&gVendorModule);
    if (rv)
        ALOGE("failed to open vendor camera module");
    return rv;
}

static struct hw_module_methods_t camera_module_methods = {
    .open = camera_device_open
};

camera_module_t HAL_MODULE_INFO_SYM = {
    .common = {
         .tag = HARDWARE_MODULE_TAG,
         .module_api_version = CAMERA_MODULE_API_VERSION_2_4,
         .hal_api_version = HARDWARE_HAL_API_VERSION,
         .id = CAMERA_HARDWARE_MODULE_ID,
         .name = "universal3475 Camera Wrapper",
         .author = "The LineageOS Project",
         .methods = &camera_module_methods,
         .dso = NULL,
         .reserved = {0},
    },
    .get_number_of_cameras = camera_get_number_of_cameras,
    .get_camera_info = camera_get_camera_info,
    .set_callbacks = camera_set_callbacks,
    .get_vendor_tag_ops = camera_get_vendor_tag_ops,
    .open_legacy = camera_open_legacy,
    .set_torch_mode = camera_set_torch_mode,
    .init = NULL,
    .reserved = {0},
};

static int camera_device_open(const hw_module_t* module, const char* name,
                hw_device_t** device)
{
    int rv = -EINVAL;

    if (name != NULL) {
        if (check_vendor_module())
            return -EINVAL;
        gAnyCameraOpened = true;
        rv = camera2_device_open(module, name, device);
    }

    return rv;
}

static int camera_get_number_of_cameras(void)
{
    ALOGV("%s", __FUNCTION__);
    if (check_vendor_module())
        return 0;
    int n = gVendorModule->get_number_of_cameras();
    close_leaked_video_fds("get_number_of_cameras");
    return n;
}

static int camera_get_camera_info(int camera_id, struct camera_info *info)
{
    ALOGV("%s", __FUNCTION__);
    if (check_vendor_module())
        return 0;
    int r = gVendorModule->get_camera_info(camera_id, info);
    close_leaked_video_fds("get_camera_info");
    return r;
}

static int camera_set_callbacks(const camera_module_callbacks_t *callbacks)
{
    ALOGV("%s", __FUNCTION__);
    if (check_vendor_module())
        return 0;
    return gVendorModule->set_callbacks(callbacks);
}

static void camera_get_vendor_tag_ops(vendor_tag_ops_t* ops)
{
    ALOGV("%s", __FUNCTION__);
    if (check_vendor_module())
        return;
    return gVendorModule->get_vendor_tag_ops(ops);
}

static int camera_open_legacy(const struct hw_module_t* module, const char* id, uint32_t halVersion, struct hw_device_t** device)
{
    (void)halVersion;
    ALOGV("%s", __FUNCTION__);
    if (check_vendor_module())
        return 0;
    return camera2_device_open(module, id, device);
}

static int camera_set_torch_mode(const char* camera_id, bool enabled)
{
    ALOGV("%s", __FUNCTION__);
    if (check_vendor_module())
        return 0;
    return gVendorModule->set_torch_mode(camera_id, enabled);
}
