// SPDX-License-Identifier: MIT
#include "media.hpp"
#include "vendor/stb_image_write.h"
#include <algorithm>
#include <chrono>
#include <thread>
namespace tab5 {
static void u32(std::string &s, uint32_t n) {
  for (int i = 0; i < 4; ++i) {
    s += char(n & 255);
    n >>= 8;
  }
}
static void set32(std::string &s, size_t at, uint32_t n) {
  for (int i = 0; i < 4; ++i) {
    s[at + i] = n & 255;
    n >>= 8;
  }
}
static uint32_t get32(const std::string &s, size_t at) {
  uint32_t n = 0;
  for (int i = 3; i >= 0; --i)
    n = (n << 8) | uint8_t(s[at + i]);
  return n;
}
static std::string chunk(const char *tag, const std::string &data) {
  std::string s = tag;
  u32(s, data.size());
  s += data;
  if (data.size() & 1)
    s += '\0';
  return s;
}
static std::string aviHeader(unsigned width, unsigned height, uint32_t frames,
                             uint32_t bytes, uint32_t frameUs = 200000) {
  std::string avih;
  for (auto n :
       std::initializer_list<uint32_t>{frameUs, 0u, 0u, 0u, frames, 0u, 1u,
                                       262144u, width, height, 0u, 0u, 0u, 0u})
    u32(avih, n);
  std::string stream = "vidsMJPG";
  for (auto n : std::initializer_list<uint32_t>{
           0u, 0u, 0u, frameUs, 1000000u, 0u, frames, 262144u, 0xffffffffu, 0u})
    u32(stream, n);
  stream.append(8, '\0');
  stream[52] = width & 255;
  stream[53] = width >> 8;
  stream[54] = height & 255;
  stream[55] = height >> 8;
  std::string bitmap;
  u32(bitmap, 40);
  u32(bitmap, width);
  u32(bitmap, height);
  u32(bitmap, 0x00180001);
  bitmap += "MJPG";
  u32(bitmap, width * height * 3);
  bitmap.append(16, '\0');
  std::string strl = "strl";
  strl += chunk("strh", stream) + chunk("strf", bitmap);
  std::string hdrl = "hdrl";
  hdrl += chunk("avih", avih) + chunk("LIST", strl);
  std::string header = "RIFF";
  u32(header, 0);
  header += "AVI ";
  header += chunk("LIST", hdrl);
  header += "LIST";
  u32(header, bytes + 4);
  header += "movi";
  set32(header, 4, header.size() + bytes - 8);
  return header;
}
static bool jpeg(const Picture &pic, std::string &out) {
  if (!pic.width || !pic.height || pic.width * pic.height > 1280 * 720 ||
      pic.pixels.size() != pic.width * pic.height * 2)
    return false;
  std::string rgb(pic.width * pic.height * 3, '\0');
  for (size_t i = 0; i < pic.width * pic.height; ++i) {
    uint16_t c =
        uint8_t(pic.pixels[2 * i]) | (uint8_t(pic.pixels[2 * i + 1]) << 8);
    rgb[3 * i] = (c >> 11) * 255 / 31;
    rgb[3 * i + 1] = ((c >> 5) & 63) * 255 / 63;
    rgb[3 * i + 2] = (c & 31) * 255 / 31;
  }
  struct Sink {
    std::string bytes;
    bool full = false;
  } sink;
  auto write = [](void *arg, void *data, int n) {
    auto *s = static_cast<Sink *>(arg);
    if (n < 0 || s->bytes.size() + n > 262144)
      s->full = true;
    else
      s->bytes.append(static_cast<char *>(data), n);
  };
  bool ok = stbi_write_jpg_to_func(write, &sink, pic.width, pic.height, 3,
                                   rgb.data(), 70) &&
            !sink.full;
  out = std::move(sink.bytes);
  return ok;
}
bool Media::camera(bool on) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (on && !cameraOn_)
    picture_.reset();
  cameraOn_ = on;
  if (!on)
    videoStop_ = true;
  else
    cameraError_.clear();
  return cameraWorker_.joinable() && bool(device.capture);
}
bool Media::video(const std::string &path, bool playback) {
  if (!Storage::validPath(path) || path.rfind("/home/Pictures/", 0) != 0)
    return false;
  std::lock_guard<std::mutex> lock(mutex_);
  if (videoRecording_ || videoPlaying_) {
    cameraError_ = "Pare o vídeo atual";
    return false;
  }
  videoPath_ = path;
  videoStart_ = std::chrono::steady_clock::now();
  videoFrames_ = videoBytes_ = 0;
  videoOffset_ = aviHeader(640, 360, 0, 0).size();
  videoStop_ = false;
  cameraError_.clear();
  if (playback) {
    std::string header;
    if (!disk_.readRange(path, 0, 512, header) || header.size() < 64 ||
        header.substr(0, 4) != "RIFF" || header.substr(8, 4) != "AVI " ||
        header.find("vidsMJPG") == std::string::npos) {
      cameraError_ = "Somente AVI/MJPEG é suportado";
      return false;
    }
    auto movi = header.find("movi");
    if (movi == std::string::npos) {
      cameraError_ = "Vídeo inválido";
      return false;
    }
    videoOffset_ = movi + 4;
    videoFrameMs_ =
        std::max(50u, std::min(2000u, unsigned(get32(header, 32) / 1000)));

    picture_.reset();
    videoPlaying_ = true;
    cameraOn_ = false;
  } else {
    std::vector<File> files;
    if (disk_.listDir("/home/Pictures", files))
      for (auto &f : files)
        if (f.path == path || f.path == path + ".part") {
          cameraError_ = "Arquivo já existe";
          return false;
        }
    if (!disk_.write(path + ".part", aviHeader(640, 360, 0, 0))) {
      cameraError_ = disk_.lastError();
      return false;
    }
    videoRecording_ = true;
    cameraOn_ = true;
  }
  return cameraWorker_.joinable();
}
void Media::stopVideo() {
  std::lock_guard<std::mutex> lock(mutex_);
  videoStop_ = true;
}
std::string Media::cameraStatus() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return !cameraError_.empty()
             ? cameraError_
             : (videoRecording_
                    ? "Gravando vídeo • " +
                          std::to_string(
                              std::chrono::duration_cast<std::chrono::seconds>(
                                  std::chrono::steady_clock::now() -
                                  videoStart_)
                                  .count()) +
                          " s"
                : videoPlaying_ ? "Reproduzindo vídeo"
                                : "Câmera pronta");
}
void Media::cameraLoop() {
  unsigned recordWidth = 640, recordHeight = 360;
  bool cameraOpened = false;
  auto nextVideoFrame = std::chrono::steady_clock::time_point::min();
  std::string recordingPath;
  while (!shutdown_) {
    if(sleepRequested_) {
      cameraParked_=true;
      while(sleepRequested_&&!shutdown_)workerWait(std::chrono::milliseconds(5));
      cameraParked_=false;
      continue;
    }
    auto start = std::chrono::steady_clock::now();
    bool on, recording, playing, stop;
    std::string path;
    uint64_t offset;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      on = cameraOn_;
      recording = videoRecording_;
      playing = videoPlaying_;
      stop = videoStop_;
      path = videoPath_;
      offset = videoOffset_;
    }
    if (stop && (recording || playing)) {
      bool ok = true;
      if (recording) {
        uint64_t count, bytes;
        {
          std::lock_guard<std::mutex> lock(mutex_);
          count = videoFrames_;
          bytes = videoBytes_;
        }
        ok = disk_.patch(
                 path + ".part", 0,
                 aviHeader(recordWidth, recordHeight, count, bytes,
                           count ? std::max<uint64_t>(
                                       1, std::chrono::duration_cast<
                                              std::chrono::microseconds>(
                                              std::chrono::steady_clock::now() -
                                              videoStart_)
                                                  .count() /
                                              count)
                                 : 200000)) &&
             disk_.move(path + ".part", path);
      }
      {
        std::lock_guard<std::mutex> lock(mutex_);
        videoRecording_ = videoPlaying_ = videoStop_ = false;
        if (!ok)
          cameraError_ = "Vídeo preservado em .part: " + disk_.lastError();
      }
    } else if (playing) {
      std::string header;
      if (!disk_.readRange(path, offset, 8, header) || header.size() != 8 ||
          header.substr(0, 4) != "00dc") {
        std::lock_guard<std::mutex> lock(mutex_);
        videoPlaying_ = false;
        continue;
      }
      uint32_t size = get32(header, 4);
      std::string encoded;
      bool ok = size > 0 && size <= 262144;
      for (size_t at = 0; ok && at < size;) {
        std::string part;
        size_t n = std::min<size_t>(65536, size - at);
        ok =
            disk_.readRange(path, offset + 8 + at, n, part) && part.size() == n;
        encoded += part;
        at += n;
      }
      ok = ok && imageData(encoded);
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (ok) {
          videoOffset_ += 8 + size + (size & 1);
          ++videoFrames_;
        } else {
          videoPlaying_ = false;
          cameraError_ = "Falha ao ler vídeo MJPEG";
        }
      }
    } else if (on) {
      Picture raw;
      cameraOpened = true;
      bool ok = device.capture && device.capture(raw);
      if (ok && raw.width && raw.height &&
          raw.pixels.size() == raw.width * raw.height * 2) {
        auto frame = std::make_shared<Picture>();
        if (raw.width == 640 && raw.height == 360) {
          *frame = std::move(raw);
        } else {
          frame->width = 640;
          frame->height = 360;
          frame->pixels.resize(640 * 360 * 2);
          for (unsigned y = 0; y < 360; ++y)
            for (unsigned x = 0; x < 640; ++x) {
              size_t from = (size_t(y) * raw.height / 360 * raw.width +
                             size_t(x) * raw.width / 640) *
                            2,
                     to = (y * 640 + x) * 2;
              frame->pixels[to] = raw.pixels[from];
              frame->pixels[to + 1] = raw.pixels[from + 1];
            }
        }
        {
          std::lock_guard<std::mutex> lock(mutex_);
          if (cameraOn_)
            picture_ = frame;
          cameraError_.clear();
        }
        if (!recording || recordingPath != path) {
          nextVideoFrame = std::chrono::steady_clock::time_point::min();
          recordingPath = recording ? path : "";
        }
        if (recording && start >= nextVideoFrame) {
          // MJPEG/SD throughput is independent from live preview cadence.
          // Do not catch up with bursts after a slow encode or SD write.
          nextVideoFrame = start + std::chrono::milliseconds(200);
          recordWidth = frame->width;
          recordHeight = frame->height;
          std::string encoded;
          ok = jpeg(*frame, encoded);
          std::string data = ok ? chunk("00dc", encoded) : "";
          {
            std::lock_guard<std::mutex> lock(mutex_);
            if (videoBytes_ + data.size() > 64 * 1024 * 1024) {
              videoStop_ = true;
              ok = false;
            }
          }
          for (size_t at = 0; ok && at < data.size(); at += 65536)
            ok = disk_.append(path + ".part", data.substr(at, 65536));
          {
            std::lock_guard<std::mutex> lock(mutex_);
            if (ok) {
              ++videoFrames_;
              videoBytes_ += data.size();
            } else {
              cameraError_ =
                  "Gravação de vídeo interrompida: " + disk_.lastError();
              videoStop_ = true;
            }
            if (videoBytes_ >= 64 * 1024 * 1024)
              videoStop_ = true;
          }
        }
      } else {
        std::string failure = device.cameraDiagnostic
                                  ? device.cameraDiagnostic()
                                  : "Câmera indisponível";
        std::lock_guard<std::mutex> lock(mutex_);
        cameraOn_ = false;
        cameraError_ = failure;
        if (recording)
          videoStop_ = true;
      }
    }
    if (!on && !playing && cameraOpened && device.closeCamera) {
      device.closeCamera();
      cameraOpened = false;
    }
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto delay = std::chrono::milliseconds(playing ? videoFrameMs_
                                           : on    ? 50
                                                   : 200);
    if (elapsed < delay)
      workerWait(std::chrono::duration_cast<std::chrono::microseconds>(
          delay - elapsed));
  }
  // Complete a controlled shutdown recording without inventing missing frames.
  std::string path;
  uint64_t count = 0, bytes = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (videoRecording_) {
      path = videoPath_;
      count = videoFrames_;
      bytes = videoBytes_;
    }
  }
  if (cameraOpened && device.closeCamera)
    device.closeCamera();
  if (!path.empty() && disk_.online() &&
      disk_.patch(
          path + ".part", 0,
          aviHeader(
              recordWidth, recordHeight, count, bytes,
              count ? std::max<uint64_t>(
                          1,
                          std::chrono::duration_cast<std::chrono::microseconds>(
                              std::chrono::steady_clock::now() - videoStart_)
                                  .count() /
                              count)
                    : 200000)))
    disk_.move(path + ".part", path);
}
} // namespace tab5
