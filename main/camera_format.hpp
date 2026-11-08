// SPDX-License-Identifier: MIT
#pragma once
#include <cerrno>
#include <cstdint>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
namespace tab5 {
// ESP Video G_FMT copies a stream structure whose type may be zero. Never
// reuse that whole structure as an S_FMT request: type is a routing selector.
template <class Control>
bool cameraCaptureFormat(Control control, v4l2_format &actual,
                         const char *&stage) {
  v4l2_format native{};
  native.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  stage = "Consultar formato";
  if (control(VIDIOC_G_FMT, &native) != 0)
    return false;
  if (!native.fmt.pix.width || !native.fmt.pix.height ||
      uint64_t(native.fmt.pix.width) * native.fmt.pix.height >
          2 * 1024 * 1024) {
    stage = "Resolução da câmera inválida";
    errno = EINVAL;
    return false;
  }
  // The CSI/ISP already defaults to RGB565. Preserve a valid default instead
  // of issuing a redundant reconfiguration. Other formats are negotiated using
  // a fresh capture request with explicit dimensions and no stale metadata.
  actual = {};
  actual.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  actual.fmt.pix.width = native.fmt.pix.width;
  actual.fmt.pix.height = native.fmt.pix.height;
  actual.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
  if (native.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565) {
    stage = "Configurar RGB565";
    if (control(VIDIOC_S_FMT, &actual) != 0)
      return false;
  } else {
    actual.fmt.pix.bytesperline = native.fmt.pix.bytesperline;
    actual.fmt.pix.sizeimage = native.fmt.pix.sizeimage;
  }
  actual.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  return true;
}
} // namespace tab5
