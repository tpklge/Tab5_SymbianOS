// SPDX-License-Identifier: MIT
#include "media.hpp"
#include "vendor/minimp3.h"
#include "vendor/stb_image.h"
#include "vendor/stb_image_write.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <thread>
namespace tab5 {
static uint32_t little(const std::string &s, size_t at, unsigned n) {
  uint32_t v = 0;
  for (int i = int(n) - 1; i >= 0; --i)
    v = (v << 8) | uint8_t(s[at + i]);
  return v;
}
static void put(std::string &s, size_t at, uint32_t v, unsigned n) {
  for (unsigned i = 0; i < n; ++i) {
    s[at + i] = v & 255;
    v >>= 8;
  }
}
static std::string wavHeader(uint32_t n) {
  std::string h(44, '\0');
  h.replace(0, 4, "RIFF");
  put(h, 4, n + 36, 4);
  h.replace(8, 8, "WAVEfmt ");
  put(h, 16, 16, 4);
  put(h, 20, 1, 2);
  put(h, 22, 1, 2);
  put(h, 24, 16000, 4);
  put(h, 28, 32000, 4);
  put(h, 32, 2, 2);
  put(h, 34, 16, 2);
  h.replace(36, 4, "data");
  put(h, 40, n, 4);
  return h;
}
static std::string auHeader(uint32_t);
Media::Media(Storage &disk) : disk_(disk) {
  cameraWorker_.submit([this] { cameraLoop(); });
  if (!worker_.submit([this] { loop(); }))
    error_ = "media worker unavailable";
}
Media::~Media() {
  shutdown_ = true;
  worker_.join();
  cameraWorker_.join();
  stop();
}
std::string Media::status() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return state_ + " • " + path_ + " • Vol " + std::to_string(volume()) + "%" +
         (state_ == "recording"
              ? " • " + std::to_string(recorded_ / (auRecord_ ? 8000 : 32000)) +
                    " s"
              : "") +
         (error_.empty() ? "" : " • " + error_);
}
std::shared_ptr<const Picture> Media::picture() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return picture_;
}
void Media::stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    ++generation_;
    state_ = state_ == "recording" || state_ == "stopping" ||
                     state_ == "recording_paused"
                 ? "stopping"
                 : "idle";
    queue_.clear();
  }
  if (device.flush)
    device.flush();
}
void Media::pause() {
  if (device.pause) {
    auto state = info().state;
    device.pause(state == "playing");
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (state_ == "recording")
    state_ = "recording_paused";
  else if (state_ == "recording_paused")
    state_ = "recording";
  else if (state_ == "playing")
    state_ = "paused";
  else if (state_ == "paused")
    state_ = "playing";
}
bool Media::play(const std::string &path, bool continuing) {
  if (device.flush)
    device.flush();
  if (device.pause)
    device.pause(false);
  std::lock_guard<std::mutex> lock(mutex_);
  if (state_ == "recording" || state_ == "stopping" ||
      state_ == "recording_paused") {
    error_ = "Pare a gravação antes de reproduzir";
    return false;
  }
  state_ = "idle";
  if (!continuing)
    queue_.clear();
  error_.clear();
  if ((!device.write && !device.writeTagged) || !worker_.joinable()) {
    error_ = "audio output unavailable";
    return false;
  }
  std::vector<File> files;
  auto slash = path.find_last_of('/');
  if (!disk_.listDir(path.substr(0, slash), files)) {
    error_ = disk_.lastError();
    return false;
  }
  auto file = std::find_if(files.begin(), files.end(), [&](auto &f) {
    return f.path == path && !f.directory;
  });
  if (file == files.end()) {
    error_ = "audio file not found";
    return false;
  }
  std::string head;
  if (!disk_.readRange(path, 0, std::min<uint64_t>(65536, file->size), head)) {
    error_ = disk_.lastError();
    return false;
  }
  auto dot = path.find_last_of('.');
  std::string extension = dot == std::string::npos ? "" : path.substr(dot);
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  mp3_ = extension == ".mp3";
  au_ = extension == ".au";
  playedSamples_ = 0;
  lastWasRecording_ = false;
  offset_ = 0;
  end_ = file->size;
  if (mp3_ && head.size() >= 10 && head.substr(0, 3) == "ID3") {
    uint32_t size = 0;
    for (int i = 6; i < 10; ++i) {
      if (uint8_t(head[i]) & 128) {
        error_ = "invalid ID3";
        return false;
      }
      size = (size << 7) | uint8_t(head[i]);
    }
    offset_ = 10 + size + ((uint8_t(head[5]) & 16) ? 10 : 0);
    if (offset_ >= end_) {
      error_ = "truncated ID3";
      return false;
    }
  }
  if (au_) {
    auto be = [&](size_t at) {
      uint32_t n = 0;
      for (int i = 0; i < 4; ++i)
        n = (n << 8) | uint8_t(head[at + i]);
      return n;
    };
    if (head.size() < 24 || head.substr(0, 4) != ".snd" || be(12) != 1 ||
        be(16) != 8000 || be(20) != 1) {
      error_ = "requires AU G.711 8 kHz mono";
      return false;
    }
    offset_ = be(4);
    end_ = be(8) == 0xffffffff ? file->size : offset_ + be(8);
    rate_ = 8000;
    channels_ = 1;
    if (offset_ < 24 || offset_ > end_ || end_ > file->size) {
      error_ = "truncated AU";
      return false;
    }
  }
  if (!mp3_ && !au_) {
    if (head.size() < 44 || head.substr(0, 4) != "RIFF" ||
        head.substr(8, 4) != "WAVE") {
      error_ = "requires PCM16 WAV or MP3";
      return false;
    }
    bool fmt = false, data = false;
    for (size_t at = 12; at + 8 <= head.size();) {
      auto n = little(head, at + 4, 4);
      auto tag = head.substr(at, 4);
      if (tag == "fmt " && n >= 16 && at + 24 <= head.size()) {
        fmt = little(head, at + 8, 2) == 1 && little(head, at + 22, 2) == 16;
        channels_ = little(head, at + 10, 2);
        rate_ = little(head, at + 12, 4);
      }
      if (tag == "data") {
        offset_ = at + 8;
        end_ = offset_ + n;
        data = true;
        break;
      }
      if (n > 65536 || at + 8 + n < at)
        break;
      at += 8 + n + (n & 1);
    }
    if (!fmt || !data || end_ > file->size || channels_ < 1 || channels_ > 2 ||
        rate_ < 8000 || rate_ > 48000 || ((end_ - offset_) % (channels_ * 2))) {
      error_ = "unsupported/truncated WAV";
      return false;
    }
  }
  path_ = path;
  ++generation_;
  state_ = "playing";
  return true;
}
bool Media::record(const std::string &path) {
  if (device.flush)
    device.flush();
  std::lock_guard<std::mutex> lock(mutex_);
  if (state_ == "recording" || state_ == "stopping" ||
      state_ == "recording_paused") {
    error_ = "Gravação já em andamento";
    return false;
  }
  ++generation_;
  error_.clear();
  if (!device.read || !worker_.joinable() ||
      path.rfind("/home/Recordings/", 0) != 0 || path.size() < 5 ||
      (path.substr(path.size() - 4) != ".wav" &&
       path.substr(path.size() - 3) != ".au")) {
    error_ = "recording unavailable or invalid destination";
    return false;
  }
  std::vector<File> all;
  if (!disk_.listDir("/home/Recordings", all)) {
    error_ = disk_.lastError();
    return false;
  }
  for (auto &f : all)
    if (f.path == path || f.path == path + ".part") {
      error_ = "recording destination exists";
      return false;
    }
  auRecord_ = path.size() > 3 && path.substr(path.size() - 3) == ".au";
  if (!disk_.write(path + ".part", auRecord_ ? auHeader(0) : wavHeader(0))) {
    error_ = disk_.lastError();
    return false;
  }
  path_ = path;
  rate_ = 16000;
  channels_ = 1;
  end_ = 0;
  playedSamples_ = 0;
  recorded_ = 0;
  capturedSamples_ = 0;
  lastWasRecording_ = true;
  state_ = "recording";
  return true;
}
bool Media::queue(const std::vector<std::string> &paths, size_t start) {
  if (paths.empty() || paths.size() > 256)
    return false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_ = paths;
    queueIndex_ = std::min(start, paths.size() - 1);
    if(shuffle_) {
      std::swap(queue_[0],queue_[queueIndex_]);queueIndex_=0;
      for(size_t i=queue_.size();i>2;--i) {shuffleSeed_=shuffleSeed_*1664525u+1013904223u;std::swap(queue_[i-1],queue_[1+shuffleSeed_%(i-1)]);}
    }
  }
  return play(paths[std::min(start, paths.size() - 1)], true);
}
void Media::recover() {
  std::vector<File> files;
  if (!disk_.listDir("/home/Recordings", files))
    return;
  for (const auto &f : files)
    if (!f.directory && f.path.size() > 5 &&
        f.path.substr(f.path.size() - 5) == ".part" && f.size >= 44 &&
        f.size <= 16 * 1024 * 1024 + 44) {
      std::string header;
      if (f.path.size() > 8 && f.path.substr(f.path.size() - 8) == ".au.part") {
        if (disk_.readRange(f.path, 0, 24, header) && header.size() == 24 &&
            header.substr(0, 4) == ".snd" &&
            disk_.patch(f.path, 0, auHeader(f.size - 24)))
          disk_.move(f.path, f.path.substr(0, f.path.size() - 5));
        continue;
      }
      if (!disk_.readRange(f.path, 0, 44, header) || header.size() != 44 ||
          header.substr(0, 4) != "RIFF" || header.substr(8, 8) != "WAVEfmt " ||
          little(header, 24, 4) != 16000 || little(header, 22, 2) != 1)
        continue;
      std::string destination = f.path.substr(0, f.path.size() - 5);
      if (disk_.patch(f.path, 0, wavHeader((f.size - 44) & ~1ULL)))
        disk_.move(f.path, destination);
    }
}
static int16_t fromMuLaw(uint8_t value) {
  value = ~value;
  int n = ((value & 15) << 3) + 132;
  n <<= (value >> 4) & 7;
  return (value & 128) ? 132 - n : n - 132;
}
static uint8_t toMuLaw(int16_t sample) {
  int n = sample, sign = n < 0 ? 128 : 0;
  if (n < 0)
    n = -n;
  n = std::min(32635, n) + 132;
  int exponent = 7;
  for (int mask = 16384; exponent > 0 && !(n & mask); mask >>= 1)
    --exponent;
  return uint8_t(~(sign | (exponent << 4) | ((n >> (exponent + 3)) & 15)));
}
static std::string auHeader(uint32_t bytes) {
  std::string s(24, '\0');
  s.replace(0, 4, ".snd");
  auto be = [&](int at, uint32_t n) {
    for (int i = 3; i >= 0; --i) {
      s[at + i] = n & 255;
      n >>= 8;
    }
  };
  be(4, 24);
  be(8, bytes);
  be(12, 1);
  be(16, 8000);
  be(20, 1);
  return s;
}
Media::Info Media::info() const {
  std::lock_guard<std::mutex> lock(mutex_);
  Info value{state_, path_, error_};
  value.seconds = lastWasRecording_
                      ? double(capturedSamples_) / 16000.0
                      : double(playedSamples_) / std::max(1u, rate_);
  value.total =
      mp3_ ? 0
           : double(end_) / (au_ ? 8000 : std::max(1u, rate_ * channels_ * 2));
  value.level = level_;
  value.progress = lastWasRecording_ ? 0
                   : end_ ? std::min<uint64_t>(100, offset_ * 100 / end_)
                          : 0;
  value.filming = videoRecording_;value.videoFrames=videoFrames_;value.videoBytes=videoBytes_;
  return value;
}
void Media::skip(int direction) {
  std::string next;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty())
      return;
    queueIndex_ = direction > 0
                      ? (queueIndex_ + 1) % queue_.size()
                      : (queueIndex_ + queue_.size() - 1) % queue_.size();
    next = queue_[queueIndex_];
  }
  play(next, true);
}
bool Media::prepareSleep() {
  auto current=info();
  if(current.filming||current.state=="recording"||current.state=="recording_paused"||current.state=="stopping")return false;
  stop();stopVideo();camera(false);
  sleepRequested_=true;
  auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
  while(!audioParked_||!cameraParked_) {
    if(std::chrono::steady_clock::now()>=end){sleepRequested_=false;return false;}
    workerWait(std::chrono::milliseconds(5));
  }
  return true;
}
void Media::loop() {
  mp3dec_t decoder{};
  uint64_t decoderGeneration = ~uint64_t(0), cacheStart = 0;
  std::string cache, recordBuffer;
  while (!shutdown_) {
    if(sleepRequested_) {
      audioParked_=true;
      while(sleepRequested_&&!shutdown_)workerWait(std::chrono::milliseconds(5));
      audioParked_=false;
      continue;
    }
    std::string state, path;
    uint64_t generation, offset, end;
    unsigned rate, channels;
    bool mp3, au, compressed;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      state = state_;
      path = path_;
      generation = generation_;
      offset = offset_;
      end = end_;
      rate = rate_;
      channels = channels_;
      mp3 = mp3_;
      au = au_;
      compressed = auRecord_;
    }
    if (state == "stopping") {
      if (!recordBuffer.empty()) {
        bool written = disk_.append(path + ".part", recordBuffer);
        std::lock_guard<std::mutex> lock(mutex_);
        if (written)
          recorded_ += recordBuffer.size();
        else
          error_ = "Espaço ou cartão indisponível; gravação parcial preservada";
        recordBuffer.clear();
      }
      std::string header;
      uint64_t bytes;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        bytes = recorded_;
        header = compressed ? auHeader(bytes) : wavHeader(bytes);
      }
      if (compressed && bytes == 0) {
        disk_.remove(path + ".part");
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = "idle";
        if (error_.empty())
          error_ = "Gravação muito curta: nenhum áudio salvo";
        continue;
      }
      bool ok = disk_.online() && disk_.patch(path + ".part", 0, header) &&
                disk_.move(path + ".part", path);
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == "stopping" && path_ == path) {
          state_ = "idle";
          capturedSamples_ =
              recorded_ * (auRecord_ ? 2 : 1) / (auRecord_ ? 1 : 2);
          if (!ok)
            error_ = "Gravação preservada em .part: " + disk_.lastError();
        }
      }
      continue;
    }
    if (!disk_.online() && (state == "playing" || state == "recording")) {
      std::lock_guard<std::mutex> lock(mutex_);
      state_ = "idle";
      error_ = "Cartão indisponível";
      continue;
    }
    if (state == "playing" && !interrupt_) {
      if (generation != decoderGeneration) {
        mp3dec_init(&decoder);
        decoderGeneration = generation;
        cache.clear();
        cacheStart = offset;
      }
      if (offset >= end) {
        if (device.drained && !device.drained()) {
          workerWait(std::chrono::milliseconds(10));
          continue;
        }
        std::string next;
        {
          std::lock_guard<std::mutex> lock(mutex_);
          if (generation == generation_ && state_ == "playing") {
            state_ = "idle";
            if(!queue_.empty()) {
              if(repeat_==2) next=queue_[queueIndex_];
              else if(queueIndex_+1<queue_.size()) next=queue_[++queueIndex_];
              else if(repeat_==1) {queueIndex_=0;next=queue_[0];}
            }
          }
        }
        if (!next.empty())
          play(next, true);
        continue;
      }
      if (offset < cacheStart || offset > cacheStart + cache.size()) {
        cache.clear();
        cacheStart = offset;
      }
      size_t consumed = offset - cacheStart;
      if (cache.size() - consumed < 4096 && cacheStart + cache.size() < end) {
        cache.erase(0, consumed);
        cacheStart = offset;
        std::string more;
        if (!disk_.readRange(
                path, cacheStart + cache.size(),
                std::min<uint64_t>(65536 - cache.size(),
                                   end - cacheStart - cache.size()),
                more)) {
          std::lock_guard<std::mutex> lock(mutex_);
          if (generation == generation_) {
            state_ = "idle";
            error_ = disk_.lastError();
          }
          continue;
        }
        cache += more;
        consumed = 0;
      }
      size_t available = cache.size() - consumed;
      int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
      unsigned frames = 0;
      size_t bytes = 0;
      bool valid = true;
      if (mp3) {
        mp3dec_frame_info_t frame{};
        frames = mp3dec_decode_frame(
            &decoder,
            reinterpret_cast<const uint8_t *>(cache.data() + consumed),
            available, pcm, &frame);
        bytes = std::max(0, frame.frame_bytes);
        if (frames) {
          rate = frame.hz;
          channels = frame.channels;
        }
        if (!bytes) {
          if (available <= 128 && cache.compare(consumed, 3, "TAG") == 0)
            bytes = available;
          else
            valid = false;
        }
      } else if (au) {
        bytes = std::min(available, size_t(512));
        frames = bytes;
        for (size_t i = 0; i < bytes; ++i)
          pcm[i] = fromMuLaw(uint8_t(cache[consumed + i]));
      } else {
        bytes = std::min(available, sizeof(pcm));
        bytes -= bytes % (channels * 2);
        memcpy(pcm, cache.data() + consumed, bytes);
        frames = bytes / (channels * 2);
      }
      bool current;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        current = generation == generation_ && state_ == "playing";
      }
      unsigned peak = 0;
      for (unsigned i = 0; i < frames * channels; ++i)
        peak = std::max(peak, unsigned(std::abs(int(pcm[i]))));
      bool written = true;
      if (current && frames && !interrupt_)
        written = device.writeTagged
                      ? device.writeTagged(pcm, frames * channels, rate,
                                           channels, generation)
                      : device.write && device.write(pcm, frames * channels,
                                                     rate, channels);
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (generation == generation_) {
          if (!valid || !written) {
            state_ = "idle";
            error_ = !valid ? "Arquivo de áudio inválido"
                            : "Falha na saída de áudio";
          } else if (current && !interrupt_) {
            offset_ += bytes;
            rate_ = rate;
            channels_ = channels;
            playedSamples_ += frames;
            level_ = std::min(100u, peak * 100 / 32768);
          }
        }
      }
      workerWait(std::chrono::microseconds(1));
      continue;
    }
    if (state == "recording") {
      std::vector<int16_t> pcm;
      bool ok =
          device.read && device.read(pcm) && !pcm.empty() && pcm.size() <= 8192;
      std::string bytes;
      unsigned peak = 0;
      if (ok) {
        for (auto sample : pcm)
          peak = std::max(peak, unsigned(std::abs(int(sample))));
        if (compressed) {
          for (size_t i = 0; i < pcm.size(); i += 2)
            bytes += char(toMuLaw(pcm[i]));
        } else
          bytes.assign(reinterpret_cast<const char *>(pcm.data()),
                       pcm.size() * 2);
      }
      bool current;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        current =
            path == path_ && (state_ == "recording" || state_ == "stopping" ||
                              state_ == "recording_paused");
      }
      size_t committed = 0;
      if (current && ok) {
        recordBuffer += bytes;
        if (recordBuffer.size() >= 4096) {
          ok = disk_.append(path + ".part", recordBuffer);
          if (ok) {
            committed = recordBuffer.size();
            recordBuffer.clear();
          } else
            recordBuffer.clear();
        }
      }
      std::string failure;
      if (!ok)
        failure = device.diagnostic ? device.diagnostic() : disk_.lastError();
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (current && path == path_) {
          if (ok) {
            capturedSamples_ += pcm.size();
            recorded_ += committed;
            level_ = std::min(100u, peak * 100 / 32768);
          } else {
            state_ = "stopping";
            error_ = "Gravação interrompida: " + failure;
          }
          if (recorded_ >= 16 * 1024 * 1024)
            state_ = "stopping";
        }
      }
      workerWait(std::chrono::microseconds(1));
      continue;
    }
    workerWait(std::chrono::milliseconds(5));
  }
  // Finalization occurs after outstanding device I/O, never while a UI command
  // owns the state lock.
  std::string path, header;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == "recording" || state_ == "stopping" ||
        state_ == "recording_paused") {
      path = path_;
      header = auRecord_ ? auHeader(recorded_) : wavHeader(recorded_);
    }
  }
  if (!path.empty() && !recordBuffer.empty() &&
      disk_.append(path + ".part", recordBuffer)) {
    std::lock_guard<std::mutex> lock(mutex_);
    recorded_ += recordBuffer.size();
    header = auRecord_ ? auHeader(recorded_) : wavHeader(recorded_);
  }
  if (!path.empty() && disk_.online() && disk_.patch(path + ".part", 0, header))
    disk_.move(path + ".part", path);
}
bool Media::image(const std::string &path, unsigned zoom) {
  std::string encoded;
  if (!disk_.read(path, encoded, 4 * 1024 * 1024)) {
    std::lock_guard<std::mutex> lock(mutex_);
    error_ = disk_.lastError();
    return false;
  }
  return imageData(encoded, zoom);
}
bool Media::imageData(const std::string &encoded, unsigned zoom,
                      std::shared_ptr<const Picture> *out) {
  if (encoded.size() > 4 * 1024 * 1024)
    return false;
  int width, height, channels;
  if (!stbi_info_from_memory(reinterpret_cast<const uint8_t *>(encoded.data()),
                             encoded.size(), &width, &height, &channels) ||
      width <= 0 || height <= 0 || uint64_t(width) * height > 2 * 1024 * 1024) {
    std::lock_guard<std::mutex> lock(mutex_);
    error_ = "image dimensions/format limit";
    return false;
  }
  auto *rgb =
      stbi_load_from_memory(reinterpret_cast<const uint8_t *>(encoded.data()),
                            encoded.size(), &width, &height, &channels, 4);
  if (!rgb) {
    std::lock_guard<std::mutex> lock(mutex_);
    error_ = "JPEG/PNG decoding failed";
    return false;
  }
  unsigned maxW = zoom == 0   ? 160
                  : zoom == 2 ? 960
                              : 640,
           maxH = zoom == 0   ? 120
                  : zoom == 2 ? 600
                              : 400;
  double scale =
      std::min(1.0, std::min(double(maxW) / width, double(maxH) / height));
  auto pic = std::make_shared<Picture>();
  pic->width = std::max(1u, unsigned(width * scale));
  pic->height = std::max(1u, unsigned(height * scale));
  pic->pixels.resize(pic->width * pic->height * 2);
  if(channels==2||channels==4)pic->alpha.resize(pic->width*pic->height);
  for (unsigned y = 0; y < pic->height; ++y)
    for (unsigned x = 0; x < pic->width; ++x) {
      unsigned src = (uint64_t(y) * height / pic->height * width +
                      uint64_t(x) * width / pic->width) *
                     4;
      uint16_t c = ((rgb[src] >> 3) << 11) | ((rgb[src + 1] >> 2) << 5) |
                   (rgb[src + 2] >> 3);
      size_t at = (y * pic->width + x) * 2;
      pic->pixels[at] = c & 255;
      pic->pixels[at + 1] = c >> 8;
      if(!pic->alpha.empty())pic->alpha[y*pic->width+x]=rgb[src+3];
    }
  stbi_image_free(rgb);
  std::lock_guard<std::mutex> lock(mutex_);
  if (out)
    *out = pic;
  else
    picture_ = pic;
  error_.clear();
  return true;
}
std::shared_ptr<const Picture> Media::thumbnail(const std::string &path) {
  std::string data;
  bool avi = path.size() > 4 && path.substr(path.size() - 4) == ".avi";
  if (avi) {
    std::string header;
    if (!disk_.readRange(path, 0, 512, header) || header.size() < 12 ||
        header.substr(0, 4) != "RIFF" || header.substr(8, 4) != "AVI ")
      return {};
    auto movi = header.find("movi");
    if (movi == std::string::npos || movi + 12 > header.size() ||
        header.substr(movi + 4, 4) != "00dc")
      return {};
    uint32_t size = little(header, movi + 8, 4);
    if (!size || size > 262144)
      return {};
    for (size_t at = 0; at < size;) {
      std::string part;
      size_t n = std::min<size_t>(65536, size - at);
      if (!disk_.readRange(path, movi + 12 + at, n, part) || part.size() != n)
        return {};
      data += part;
      at += n;
    }
  } else if (!disk_.read(path, data, 4 * 1024 * 1024))
    return {};
  std::shared_ptr<const Picture> image;
  if (!imageData(data, 0, &image))
    return {};
  return image;
}
bool Media::photograph(const std::string &path, bool save) {
  Picture pic;
  if (!device.capture || !device.capture(pic) || pic.width == 0 ||
      pic.height == 0 || uint64_t(pic.width) * pic.height > 1280 * 720 ||
      pic.pixels.size() != pic.width * pic.height * 2) {
    std::lock_guard<std::mutex> lock(mutex_);
    error_ = "camera unavailable or capture failed";
    return false;
  }
  if (save) {
    if (path.rfind("/home/Pictures/", 0) != 0) {
      std::lock_guard<std::mutex> lock(mutex_);
      error_ = "photo destination denied";
      return false;
    }
    std::string rgb;
    rgb.resize(pic.width * pic.height * 3);
    for (size_t i = 0; i < pic.width * pic.height; ++i) {
      uint16_t c =
          uint8_t(pic.pixels[i * 2]) | (uint8_t(pic.pixels[i * 2 + 1]) << 8);
      rgb[i * 3] = (c >> 11) * 255 / 31;
      rgb[i * 3 + 1] = ((c >> 5) & 63) * 255 / 63;
      rgb[i * 3 + 2] = (c & 31) * 255 / 31;
    }
    struct Output {
      std::string bytes;
      bool overflow = false;
    } output;
    auto sink = [](void *p, void *bytes, int n) {
      auto *out = static_cast<Output *>(p);
      if (n < 0 || out->bytes.size() + n > 2 * 1024 * 1024)
        out->overflow = true;
      else
        out->bytes.append(static_cast<char *>(bytes), n);
    };
    if (!stbi_write_jpg_to_func(sink, &output, pic.width, pic.height, 3,
                                rgb.data(), 85) ||
        output.overflow || output.bytes.empty() ||
        !disk_.write(path, output.bytes)) {
      std::lock_guard<std::mutex> lock(mutex_);
      error_ = "photo save failed: " + disk_.lastError();
      return false;
    }
    return image(path);
  }
  std::lock_guard<std::mutex> lock(mutex_);
  picture_ = std::make_shared<Picture>(std::move(pic));
  error_.clear();
  return true;
}
} // namespace tab5
