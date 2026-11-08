// SPDX-License-Identifier: MIT
#pragma once
#include "executor.hpp"
#include "input.hpp"
#include "lvgl.h"
#include "runtime.hpp"
#include <atomic>
#include <deque>
namespace tab5 {
class UI {
  Executor worker_, tone_{32768};
  std::atomic<bool> toneDone_{true};
  std::deque<bool> tones_;
  std::atomic<bool> completed_{false};
  bool busy_ = false, result_ = false, retryJob_ = false, cardRemoved_ = false;
  Keyboard keys_;
  Runtime &runtime_;
  lv_obj_t *screen_ = nullptr, *edit_ = nullptr, *keyboard_ = nullptr,
           *message_ = nullptr;
  lv_obj_t *battery_ = nullptr, *batteryGauge_ = nullptr;
  lv_group_t *group_ = nullptr;
  lv_font_t *font_ = nullptr, *largeFont_ = nullptr, *compactFont_ = nullptr;
  std::string iconData_;
  std::shared_ptr<const Picture> displayedPicture_;
  lv_image_dsc_t pictureDescriptor_{};
  std::vector<lv_image_dsc_t> thumbnailDescriptors_;
  std::vector<std::shared_ptr<const Picture>> thumbnailPictures_;
  lv_obj_t *liveImage_ = nullptr, *mediaTime_ = nullptr, *mediaLevel_ = nullptr,
           *mediaProgress_ = nullptr, *transportImage_ = nullptr;
  int imageWidth_ = 0, imageHeight_ = 0;
  bool mediaFineTime_ = false;
  uint32_t lastMedia_ = 0;
  void showPicture(std::shared_ptr<const Picture>, lv_obj_t *, int, int,
                   bool zoom = false);
  std::map<std::string, lv_image_dsc_t> icons_;
  std::vector<lv_obj_t *> editors_;
  std::vector<lv_obj_t *> formWidgets_;
  std::map<lv_obj_t *, std::vector<std::string>> choices_;
  std::map<lv_obj_t *, std::string> masks_;
  bool formatting_ = false;
  int editHeight_ = 100, keyboardEditY_ = 0, keyboardEditH_ = 0;
  lv_obj_t *keyboardEdit_ = nullptr;
  void showKeyboard();
  void restoreKeyboardField();
  void icon(lv_obj_t *, const std::string &, int, int, int);
  std::string fontData_, clipboard_, selected_;
  uint32_t lastTick_ = 0, lastClock_ = 0;
  bool backgroundJob_ = false;
  std::string notice_, toneError_;
  std::map<std::string, std::pair<int, int>> editorPositions_;
  bool all_ = false, virtualShift_ = false;
  int anchor_ = -1;
  void selection(int, int);
  void eraseSelection();
  std::string selectedText() const;
  struct Action {
    std::string key, value, owner;
  };
  std::deque<Action> pending_;
  struct DeferredInput {
    Input event;
    lv_obj_t *focus;
  };
  std::deque<DeferredInput> deferredInput_;
  std::string renderOwner_;
  std::vector<std::string> ids_, keyLabels_;
  std::vector<const char *> keyMap_;
  static void clicked(lv_event_t *);
  static void changed(lv_event_t *);
  static void virtualKey(lv_event_t *);
  lv_obj_t *status_ = nullptr, *liveStatusLabel_ = nullptr;
  std::string mediaStatusText_;
  lv_obj_t *jobControl_ = nullptr;
  lv_obj_t *button(lv_obj_t *, const std::string &, const std::string &, int,
                   int, int, int);
  void enqueue(const std::string &);
  std::string text() const;

public:
  explicit UI(Runtime &r) : runtime_(r) {}
  ~UI();
  bool start();
  void render();
  void recovery(const std::string &);
  void poll();
  void input(const Input &);
  bool idle() const { return !busy_ && pending_.empty(); }
  bool editing() const { return edit_ != nullptr; }
};
} // namespace tab5
