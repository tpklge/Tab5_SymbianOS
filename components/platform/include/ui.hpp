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
  Executor worker_{98304,3,true}, tone_{32768};
  std::atomic<bool> toneDone_{true};
  std::deque<bool> tones_;
  std::atomic<bool> completed_{false};
  bool recoveryCleaning_ = false;
  bool busy_ = false, result_ = false, retryJob_ = false, cardRemoved_ = false;
  struct StatusWidgets {lv_obj_t *value,*bar;std::string last;};
  std::vector<StatusWidgets> statusWidgets_;
  bool statusFieldsOnly_=false;
  bool readerTextOnly_=false;
  int brightness_ = 100;
  lv_obj_t *sleepCover_ = nullptr;
  void wakeScreen();
  Keyboard keys_;
  Runtime &runtime_;
  lv_obj_t *screen_ = nullptr, *edit_ = nullptr, *keyboard_ = nullptr,
           *message_ = nullptr, *chatSend_ = nullptr;
  lv_obj_t *packagePane_=nullptr;
  std::vector<lv_obj_t *> packageRows_;
  std::string packageKey_;
  std::map<std::string,int> packageScroll_;
  std::map<std::string,std::string> packageFocus_;
  lv_obj_t *diagnosticPane_=nullptr,*diagnosticLabel_=nullptr;
  lv_obj_t *battery_ = nullptr, *batteryGauge_ = nullptr;
  lv_group_t *group_ = nullptr;
  lv_font_t *font_ = nullptr, *largeFont_ = nullptr, *compactFont_ = nullptr;
  std::string iconData_;
  std::shared_ptr<const Picture> displayedPicture_;
  lv_image_dsc_t pictureDescriptor_{}, wallpaperDescriptor_{};
  std::shared_ptr<const Picture> wallpaperPicture_;
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
  uint32_t lastTick_ = 0, lastClock_ = 0,foregroundStarted_=0;
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
    bool explicitValue=false;
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
  std::vector<std::string> documentTargets_;
  static void documentLink(lv_event_t *);
  static void clicked(lv_event_t *);
  static void changed(lv_event_t *);
  static void virtualKey(lv_event_t *);
  static void slide(lv_event_t *);
  struct Slider {lv_obj_t *caption;std::string kind;int initial;};
  std::map<lv_obj_t *,Slider> sliders_;
  bool refreshed_=false;
  uint32_t lastRefresh_=0;
  lv_obj_t *digitalClock_=nullptr, *processing_=nullptr, *homeDate_=nullptr, *lockMask_=nullptr, *lockTitle_=nullptr, *wifiDot_=nullptr, *wifiCross_=nullptr;
  lv_obj_t *wifiRings_[3]{};
  void updateWifi();
  lv_obj_t *analogHour_=nullptr,*analogMinute_=nullptr;
  lv_point_precise_t hourPoints_[2]{},minutePoints_[2]{};
  void updateAnalog();
  lv_obj_t *slider(lv_obj_t *,const std::string &,int,int,int,int,int);
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
