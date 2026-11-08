// SPDX-License-Identifier: MIT
#pragma once
#include "executor.hpp"
#include "storage.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
namespace tab5 {
struct Picture {
  unsigned width = 0, height = 0;
  std::string pixels;
};
class Media {
  Storage &disk_;
  std::atomic<int> volume_{60};
  Executor worker_{65536, 5}, cameraWorker_{32768, 2};
  std::atomic<bool> shutdown_{false}, interrupt_{false};
  mutable std::mutex mutex_;
  std::string state_ = "idle", path_, error_;
  uint64_t offset_ = 0, end_ = 0, recorded_ = 0;
  unsigned rate_ = 16000, channels_ = 1;
  bool mp3_ = false, au_ = false, auRecord_ = false, lastWasRecording_ = false;
  uint64_t playedSamples_ = 0, capturedSamples_ = 0;
  unsigned level_ = 0;
  bool cameraOn_ = false, videoRecording_ = false, videoPlaying_ = false,
       videoStop_ = false;
  std::string videoPath_, cameraError_;
  uint64_t videoBytes_ = 0, videoFrames_ = 0, videoOffset_ = 0;
  unsigned videoFrameMs_ = 200;
  std::chrono::steady_clock::time_point videoStart_;
  void cameraLoop();
  std::vector<std::string> queue_;
  size_t queueIndex_ = 0;
  uint64_t generation_ = 0;
  std::shared_ptr<Picture> picture_;
  void loop();

public:
  struct Device {
    std::function<bool(const int16_t *, size_t, unsigned, unsigned)> write;
    std::function<bool(const int16_t *, size_t, unsigned, unsigned, uint64_t)>
        writeTagged;
    std::function<bool(std::vector<int16_t> &)> read;
    std::function<void()> flush;
    std::function<bool()> drained;
    std::function<void(bool)> pause;
    std::function<bool(Picture &)> capture;
    std::function<void()> closeCamera;
    std::function<std::string()> cameraDiagnostic;
    std::function<std::string()> diagnostic;
  };
  explicit Media(Storage &disk);
  ~Media();
  Device device;
  bool play(const std::string &, bool continuing = false);
  bool queue(const std::vector<std::string> &, size_t start = 0);
  void recover();
  bool record(const std::string &);
  void setVolume(int v) { volume_ = std::max(0, std::min(100, v)); }
  int volume() const { return volume_; }
  bool playbackGeneration(uint64_t generation) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return generation == generation_ &&
           (state_ == "playing" || state_ == "paused");
  }
  void stop();
  void pause();
  void skip(int);
  struct Info {
    std::string state, path, error;
    double seconds = 0, total = 0;
    unsigned level = 0;
    unsigned progress = 0;
    bool filming = false;
  };
  Info info() const;
  bool camera(bool);
  bool video(const std::string &, bool playback = false);
  void stopVideo();
  std::string cameraStatus() const;
  void interrupt(bool on) { interrupt_ = on; }
  std::string status() const;
  bool image(const std::string &, unsigned zoom = 1);
  bool imageData(const std::string &, unsigned zoom = 1,
                 std::shared_ptr<const Picture> *out = nullptr);
  std::shared_ptr<const Picture> thumbnail(const std::string &);
  bool photograph(const std::string &, bool save);
  std::shared_ptr<const Picture> picture() const;
};
} // namespace tab5
