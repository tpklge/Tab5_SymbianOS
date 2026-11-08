// SPDX-License-Identifier: MIT
#include "camera_format.hpp"
#include <iostream>
static int count = 0;
static void check(bool ok, const char *why) {
  ++count;
  if (!ok) {
    std::cerr << "FAIL: " << why << '\n';
    exit(1);
  }
}
int main() {
  v4l2_format actual{};
  const char *stage = "";
  int sets = 0;
  auto driver = [&](unsigned long op, v4l2_format *f) {
    if (op == VIDIOC_G_FMT) {
      check(f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE, "G_FMT routes capture");
      *f = {};
      f->fmt.pix.width = 1280;
      f->fmt.pix.height = 720;
      f->fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
      return 0;
    }
    ++sets;
    return -1;
  };
  check(tab5::cameraCaptureFormat(driver, actual, stage),
        "default RGB565 remains usable when G_FMT clears type");
  check(actual.type == V4L2_BUF_TYPE_VIDEO_CAPTURE &&
            actual.fmt.pix.width == 1280 && actual.fmt.pix.height == 720,
        "capture selector and dimensions restored");
  check(sets == 0, "valid CSI default is not unnecessarily reset");
  auto raw = [&](unsigned long op, v4l2_format *f) {
    if (op == VIDIOC_G_FMT) {
      *f = {};
      f->fmt.pix.width = 1600;
      f->fmt.pix.height = 1200;
      f->fmt.pix.pixelformat = V4L2_PIX_FMT_SBGGR8;
      f->fmt.pix.sizeimage = 123;
      return 0;
    }
    check(op == VIDIOC_S_FMT && f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE,
          "S_FMT request is explicitly capture");
    check(f->fmt.pix.pixelformat == V4L2_PIX_FMT_RGB565 &&
              f->fmt.pix.width == 1600 && f->fmt.pix.height == 1200,
          "S_FMT retains sensor dimensions");
    check(f->fmt.pix.sizeimage == 0, "S_FMT has no stale RAW metadata");
    return 0;
  };
  check(tab5::cameraCaptureFormat(raw, actual, stage),
        "RAW output negotiates RGB565");
  check(!tab5::cameraCaptureFormat(
            [](unsigned long, v4l2_format *) {
              errno = EIO;
              return -1;
            },
            actual, stage) &&
            errno == EIO,
        "G_FMT failure propagated");
  check(!tab5::cameraCaptureFormat(
            [](unsigned long, v4l2_format *f) {
              *f = {};
              return 0;
            },
            actual, stage) &&
            errno == EINVAL,
        "zero dimensions refused");
  auto fail = [](unsigned long op, v4l2_format *f) {
    if (op == VIDIOC_G_FMT) {
      *f = {};
      f->fmt.pix.width = 1280;
      f->fmt.pix.height = 720;
      return 0;
    }
    errno = EINVAL;
    return -1;
  };
  check(!tab5::cameraCaptureFormat(fail, actual, stage) &&
            std::string(stage) == "Configurar RGB565" && errno == EINVAL,
        "S_FMT error keeps stage and errno");
  std::cout << count << " camera protocol checks passed\n";
}
