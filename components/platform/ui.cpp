#include "html_allocation_scope.hpp"
// SPDX-License-Identifier: MIT
#include "ui.hpp"
#include "ui_text.hpp"
#include "screen_power.hpp"
#include "network_signal.hpp"
#include "src/misc/cache/instance/lv_image_cache.h"
#include "src/misc/cache/instance/lv_image_header_cache.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include "text_lines.hpp"
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
namespace tab5 {
static std::string statusClock(Runtime &r) {
  auto s = r.clockText();
  if (s.size() > 20 && s[2] == '/')
    return s.substr(0, 10) + "   " + s.substr(12, 5);
  return "Ajustar relógio";
}

// Small, atomic SD breadcrumb; never logs editor contents or runs on LVGL
// thread.
static void traceAction(Runtime &r, const std::string &action,
                        const char *stage) {
#ifdef ESP_PLATFORM
  if (!r.storage().online())
    return;
  auto previousError = r.storage().lastError();
  std::string log =
      "runtime=1.0.0\naction=" + (r.current()=="lock" || r.current()=="settings" ? "private" : action) + "\nstage=" + stage +
      "\napp=" + r.current() + "\ninternal_free=" +
      std::to_string(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)) +
      "\ninternal_largest=" +
      std::to_string(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)) +
      "\npsram_free=" +
      std::to_string(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)) +
      "\nworker_stack_free_bytes=" +
      std::to_string(uxTaskGetStackHighWaterMark(nullptr)) + "\n";
  r.storage().write("/logs/last-action.txt", log);
  r.storage().restoreDiagnosticError(previousError);
#else
  (void)r;
  (void)action;
  (void)stage;
#endif
}
static bool pointerPressed() {
  for(auto *input=lv_indev_get_next(nullptr);input;input=lv_indev_get_next(input)) if(lv_indev_get_state(input)==LV_INDEV_STATE_PRESSED) return true;
  return false;
}
static bool sameLiveView(const View &a,const View &b) {
  if(a.picture!=b.picture || a.sidebar.size()!=b.sidebar.size() || a.footer.size()!=b.footer.size() || a.tools.size()!=b.tools.size())return false;
  for(size_t i=0;i<a.sidebar.size();++i)if(a.sidebar[i].id!=b.sidebar[i].id||a.sidebar[i].subtitle!=b.sidebar[i].subtitle)return false;
  for(size_t i=0;i<a.footer.size();++i)if(a.footer[i].id!=b.footer[i].id||a.footer[i].label!=b.footer[i].label)return false;
  for(size_t i=0;i<a.tools.size();++i)if(a.tools[i].id!=b.tools[i].id||a.tools[i].label!=b.tools[i].label)return false;
  if(a.title!=b.title || a.body!=b.body || a.text!=b.text || a.layout!=b.layout || a.buttons.size()!=b.buttons.size() || a.statusFields.size()!=b.statusFields.size() || a.controls.size()!=b.controls.size()) return false;
  for(size_t i=0;i<a.buttons.size();++i) if(a.buttons[i].id!=b.buttons[i].id || a.buttons[i].label!=b.buttons[i].label || a.buttons[i].subtitle!=b.buttons[i].subtitle || a.buttons[i].outgoing!=b.buttons[i].outgoing) return false;
  for(size_t i=0;i<a.statusFields.size();++i) if(a.statusFields[i].label!=b.statusFields[i].label || a.statusFields[i].value!=b.statusFields[i].value || a.statusFields[i].percent!=b.statusFields[i].percent) return false;
  for(size_t i=0;i<a.controls.size();++i) if(a.controls[i].options!=b.controls[i].options || a.controls[i].keys!=b.controls[i].keys) return false;
  return true;
}
UI::~UI() {
  if (tone_.joinable())
    tone_.join();
  if (worker_.joinable())
    worker_.join();
  if (screen_)
    lv_obj_delete(screen_);
  if (group_)
    lv_group_delete(group_);
  if (font_)
    lv_tiny_ttf_destroy(font_);
  if (largeFont_)
    lv_tiny_ttf_destroy(largeFont_);
  if (compactFont_)
    lv_tiny_ttf_destroy(compactFont_);
}
static std::vector<std::string> splitLines(const std::string &text) {
  std::vector<std::string> result;
  size_t at = 0;
  for (;;) {
    auto end = text.find('\n', at);
    result.push_back(
        text.substr(at, end == std::string::npos ? end : end - at));
    if (end == std::string::npos)
      break;
    at = end + 1;
  }
  return result;
}
std::string UI::text() const {
  if (!formWidgets_.empty()) {
    std::string result;
    for (auto *obj : formWidgets_) {
      if (obj != formWidgets_.front())
        result += '\n';
      auto found = choices_.find(obj);
      if (found != choices_.end()) {
        auto selected = lv_dropdown_get_selected(obj);
        if (selected < found->second.size())
          result += found->second[selected];
      } else if(sliders_.count(obj)) result+=std::to_string(lv_slider_get_value(obj));
      else
        result += lv_textarea_get_text(obj);
    }
    return result;
  }
  return edit_ ? lv_textarea_get_text(edit_) : "";
}
static size_t byteAt(const std::string &s, int count) {
  size_t i = 0;
  while (i < s.size() && count-- > 0) {
    ++i;
    while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xc0) == 0x80)
      ++i;
  }
  return i;
}
void UI::selection(int from, int to) {
  auto *l = lv_textarea_get_label(edit_);
  if (from == to) {
    lv_textarea_clear_selection(edit_);
    return;
  }
  lv_label_set_text_selection_start(l, std::min(from, to));
  lv_label_set_text_selection_end(l, std::max(from, to));
}
std::string UI::selectedText() const {
  if (!edit_)
    return "";
  auto *l = lv_textarea_get_label(edit_);
  auto a = lv_label_get_text_selection_start(l),
       b = lv_label_get_text_selection_end(l);
  if (a == LV_LABEL_TEXT_SELECTION_OFF || b == LV_LABEL_TEXT_SELECTION_OFF ||
      a == b)
    return "";
  std::string s = lv_textarea_get_text(edit_);
  auto first = byteAt(s, std::min(a, b)), last = byteAt(s, std::max(a, b));
  return s.substr(first, last - first);
}
void UI::eraseSelection() {
  if (!edit_)
    return;
  auto *l = lv_textarea_get_label(edit_);
  auto a = lv_label_get_text_selection_start(l),
       b = lv_label_get_text_selection_end(l);
  if (a != LV_LABEL_TEXT_SELECTION_OFF && b != LV_LABEL_TEXT_SELECTION_OFF &&
      a != b) {
    std::string s = lv_textarea_get_text(edit_);
    auto first = byteAt(s, std::min(a, b)), last = byteAt(s, std::max(a, b));
    s.erase(first, last - first);
    setEditorText(edit_, s.c_str());
    lv_textarea_set_cursor_pos(edit_, std::min(a, b));
  }
  lv_textarea_clear_selection(edit_);
  all_ = false;
  anchor_ = -1;
}
void UI::enqueue(const std::string &key) {
  if (edit_)
    editorPositions_[renderOwner_] = {lv_textarea_get_cursor_pos(edit_),
                                      lv_obj_get_scroll_y(edit_)};
  if (pending_.size() < 16)
    pending_.push_back({key, text(), renderOwner_});
}
void UI::documentLink(lv_event_t *e){
 auto *self=static_cast<UI*>(lv_event_get_user_data(e));if(self->busy_&&!self->backgroundJob_)return;
 auto *object=lv_event_get_target_obj(e);size_t index=reinterpret_cast<uintptr_t>(lv_obj_get_user_data(object));
 if(index<self->documentTargets_.size()&&self->pending_.size()<16)self->pending_.push_back({"navigate_url",self->documentTargets_[index],self->renderOwner_,true});
}
void UI::clicked(lv_event_t *e) {
  auto *self = static_cast<UI *>(lv_event_get_user_data(e));
  if(self->busy_ && self->renderOwner_=="lock") {
    auto *target=lv_event_get_target_obj(e);auto index=reinterpret_cast<uintptr_t>(lv_obj_get_user_data(target));
    if(index<self->ids_.size())self->enqueue(self->ids_[index]);
    return;
  }
  if (self->busy_ && self->backgroundJob_) {
    auto *target = lv_event_get_target_obj(e);
    auto index = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(target));
    if (index < self->ids_.size() && self->pending_.size() < 16)
      self->pending_.push_back(
          {self->ids_[index], self->text(), self->renderOwner_});
    return;
  }
  if (self->busy_) {
    auto *target = lv_event_get_target_obj(e);
    auto index = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(target));
    if (index < self->ids_.size() &&
        (target==self->jobControl_ || self->ids_[index] == "cancel_job"))
      self->runtime_.cancel();
    else if(index<self->ids_.size()) {
      const auto &id=self->ids_[index];
      if(id=="diagnostic" || id=="back" || id=="home" || id=="menu" || id=="switcher") {
        if(self->pending_.empty() || self->pending_.back().key!=id || self->pending_.back().owner!=self->renderOwner_)self->enqueue(id);
      }
    }
    return;
  }
  if (self->cardRemoved_)
    return;
  auto *o = lv_event_get_target_obj(e);
  auto index = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(o));
  if (index < self->ids_.size()) {
    auto id = self->ids_[index];
    if (self->runtime_.view().classic && id.rfind("open:", 0) == 0 &&
        self->selected_ != id) {
      self->selected_ = id;
      lv_obj_set_style_bg_color(o, lv_color_hex(0x6688aa), 0);
      return;
    }
    self->enqueue(id);
  }
}
void UI::changed(lv_event_t *e) {
  auto *self = static_cast<UI *>(lv_event_get_user_data(e));
  auto *obj = lv_event_get_target_obj(e);
  if (lv_event_get_code(e) == LV_EVENT_FOCUSED) {
    self->edit_ = obj;
    if (self->keyboard_ &&
        !lv_obj_has_flag(self->keyboard_, LV_OBJ_FLAG_HIDDEN))
      self->showKeyboard();
  }
  self->all_ = false;
  auto mask = self->masks_.find(obj);
  if (mask == self->masks_.end() || self->formatting_ ||
      lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED)
    return;
  std::string raw = lv_textarea_get_text(obj), digits, formatted;
  int cursor = lv_textarea_get_cursor_pos(obj), before = 0;
  for (size_t i = 0; i < raw.size(); ++i)
    if (raw[i] >= '0' && raw[i] <= '9') {
      digits += raw[i];
      if (int(i) < cursor)
        ++before;
    }
  size_t used = 0;
  int restored = 0;
  for (char c : mask->second) {
    if (used >= digits.size())
      break;
    if (c == '0') {
      formatted += digits[used++];
      if (int(used) <= before)
        restored = formatted.size();
    } else
      formatted += c;
  }
  if (formatted != raw) {
    self->formatting_ = true;
    setEditorText(obj, formatted.c_str());
    lv_textarea_set_cursor_pos(obj, restored);
    self->formatting_ = false;
  }
}
void UI::restoreKeyboardField() {
  if (keyboardEdit_ && lv_obj_is_valid(keyboardEdit_)) {
    lv_obj_set_y(keyboardEdit_, keyboardEditY_);
    lv_obj_set_height(keyboardEdit_, keyboardEditH_);
    if(chatSend_ && lv_obj_is_valid(chatSend_))lv_obj_set_y(chatSend_,keyboardEditY_);
  }
  keyboardEdit_ = nullptr;
}
void UI::showKeyboard() {
  if (!keyboard_ || !edit_)
    return;
  if (keyboardEdit_ != edit_) {
    restoreKeyboardField();
    keyboardEdit_ = edit_;
    keyboardEditY_ = lv_obj_get_y(edit_);
    keyboardEditH_ = lv_obj_get_height(edit_);
  }
  int height = std::min(keyboardEditH_, 112);
  lv_obj_set_height(edit_, height);
  lv_obj_set_y(edit_,
               std::min<int>(keyboardEditY_,
                             lv_display_get_vertical_resolution(nullptr) - 398 -
                                 height - 8));
  if(chatSend_ && lv_obj_is_valid(chatSend_)){lv_obj_set_y(chatSend_,lv_obj_get_y(edit_));lv_obj_move_foreground(chatSend_);}
  lv_obj_move_foreground(edit_);
  lv_obj_move_foreground(keyboard_);
  lv_obj_remove_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_update_layout(edit_);
  lv_textarea_set_cursor_pos(edit_, lv_textarea_get_cursor_pos(edit_));
}
void UI::virtualKey(lv_event_t *e) {
  auto *self = static_cast<UI *>(lv_event_get_user_data(e));
  auto *k = lv_event_get_target_obj(e);
  uint32_t id = lv_buttonmatrix_get_selected_button(k);
  const char *s = lv_buttonmatrix_get_button_text(k, id);
  if (!s)
    return;

  if (std::string(s) == "Shift") {
    self->virtualShift_ = !self->virtualShift_;
    return;
  }
  if (std::string(s) == "Fechar") {
    lv_obj_add_flag(k, LV_OBJ_FLAG_HIDDEN);
    self->restoreKeyboardField();
    return;
  }
  if (self->edit_)
    lv_group_focus_obj(self->edit_);
  Input in;
  in.text = s;
  in.key = s;
  if (in.text == "Apagar") {
    in.text.clear();
    in.key = "backspace";
  }
  if (in.text == "Espaço")
    in.text = " ";
  if (in.text == "Enter") {
    in.text.clear();
    in.key = "enter";
  }
  if (self->virtualShift_ && in.text.size() == 1 && in.text[0] >= 'a' &&
      in.text[0] <= 'z')
    in.text[0] -= 32;
  self->input(in);
}
lv_obj_t *UI::button(lv_obj_t *parent, const std::string &id,
                     const std::string &label, int x, int y, int w, int h) {
  auto *b = lv_button_create(parent);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_text_color(b, lv_color_hex(0xeeeeee), 0);
  lv_obj_set_style_radius(b, 3, 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, lv_color_hex(0x59636f), 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_grad_color(b, lv_color_hex(0x101115), 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(0x26323b), LV_STATE_FOCUSED);
  lv_obj_set_style_outline_width(b, 2, LV_STATE_FOCUSED);
  lv_obj_set_style_outline_color(b, lv_color_hex(0x23d4e9), LV_STATE_FOCUSED);
  if (runtime_.ready())
    lv_obj_set_style_bg_color(
        b, lv_color_hex(strtoul(runtime_.view().accent.c_str(), nullptr, 16)),
        0);
  lv_obj_set_user_data(b, reinterpret_cast<void *>(ids_.size()));
  ids_.push_back(id);
  lv_obj_add_event_cb(b, clicked, LV_EVENT_CLICKED, this);
  lv_group_add_obj(group_, b);
  auto *l = lv_label_create(b);
  lv_label_set_text(l, label.c_str());
  lv_obj_set_width(l, w - 20);
  lv_obj_set_height(l,
                    lv_font_get_line_height(font_ ? font_ : LV_FONT_DEFAULT));
  lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(l);
  return b;
}
bool UI::start() {
  if (compactFont_) {
    lv_tiny_ttf_destroy(compactFont_);
    compactFont_ = nullptr;
  }
  if (largeFont_) {
    lv_tiny_ttf_destroy(largeFont_);
    largeFont_ = nullptr;
  }
  if (font_) {
    lv_tiny_ttf_destroy(font_);
    font_ = nullptr;
  }
  if (!runtime_.ready()) {
    recovery(runtime_.error);
    return false;
  }
  if (!runtime_.asset("theme.ttf", fontData_)) {
    recovery(runtime_.error);
    return false;
  }
  std::string mapping;
  if (!runtime_.asset("keyboard.map", mapping, 16384) ||
      !keys_.configure(mapping)) {
    recovery("Mapa de teclado inválido");
    return false;
  }
  std::string layout;
  if (!runtime_.asset("onscreen.keys", layout, 4096)) {
    recovery("Teclado virtual ausente");
    return false;
  }
  keyLabels_.clear();
  keyMap_.clear();
  TextLines lines(layout);
  std::string line;
  while (nextLine(lines, line))
    keyLabels_.push_back(line == "ROW" ? "\n" : line);
  for (auto &label : keyLabels_)
    keyMap_.push_back(label.c_str());
  keyMap_.push_back("");
  font_ = lv_tiny_ttf_create_data_ex(fontData_.data(), fontData_.size(), 26,
                                     LV_FONT_KERNING_NONE, 192);
  if (!font_) {
    recovery("Falha ao carregar fonte externa");
    return false;
  }
  largeFont_ = lv_tiny_ttf_create_data_ex(fontData_.data(), fontData_.size(),
                                          48, LV_FONT_KERNING_NONE, 48);
  compactFont_ = lv_tiny_ttf_create_data_ex(fontData_.data(), fontData_.size(),
                                            20, LV_FONT_KERNING_NONE, 128);
  icons_.clear();
  if (runtime_.asset("icons.bin", iconData_,3145728) && iconData_.size() >= 8 &&
      iconData_.compare(0, 4, "T5IC") == 0) {
    auto byte = [this](size_t i) {
      return static_cast<unsigned char>(iconData_[i]);
    };
    unsigned side = byte(4) | (byte(5) << 8), count = byte(6) | (byte(7) << 8);
    if (side == 96 && count <= 96 &&
        iconData_.size() == 8 + count * (16 + 96 * 96 * 3)) {
      size_t offset = 8;
      for (unsigned i = 0; i < count; ++i) {
        std::string name(iconData_.data() + offset, 16);
        auto terminator = name.find('\0');
        if (terminator == std::string::npos)
          break;
        name.resize(terminator);
        offset += 16;
        lv_image_dsc_t image{};
        image.header.magic = LV_IMAGE_HEADER_MAGIC;
        image.header.cf = LV_COLOR_FORMAT_RGB565A8;
        image.header.w = 96;
        image.header.h = 96;
        image.header.stride = 192;
        image.data_size = 96 * 96 * 3;
        image.data =
            reinterpret_cast<const uint8_t *>(iconData_.data() + offset);
        icons_[name] = image;
        offset += image.data_size;
      }
    }
  }
  render();
  return true;
}
void UI::recovery(const std::string &why) {
  runtime_.network().cancel();
  renderOwner_.clear();
  deferredInput_.clear();
  status_ = nullptr;
  battery_ = nullptr;
  batteryGauge_ = nullptr;
  liveStatusLabel_ = nullptr;
  keys_ = Keyboard();
  auto *old = screen_;
  screen_ = lv_obj_create(nullptr);
  lv_screen_load(screen_);
  if (old)
    lv_obj_delete(old);
  old = nullptr;
  edit_ = keyboard_ = message_ = jobControl_ = chatSend_ = nullptr;
  diagnosticPane_=diagnosticLabel_=nullptr;packagePane_=nullptr;packageRows_.clear();
  battery_ = batteryGauge_ = nullptr;
  status_ = liveStatusLabel_ = liveImage_ = mediaTime_ = mediaLevel_ =
      mediaProgress_ = transportImage_ = nullptr;
  statusWidgets_.clear();
  thumbnailDescriptors_.clear();
  thumbnailPictures_.clear();
  imageWidth_ = imageHeight_ = 0;
  ids_.clear();
  keyboardEdit_ = nullptr;
  editors_.clear();
  formWidgets_.clear();
  choices_.clear();
  sliders_.clear();digitalClock_=nullptr;processing_=nullptr;homeDate_=nullptr;lockMask_=lockTitle_=wifiDot_=wifiCross_=nullptr;for(auto &ring:wifiRings_)ring=nullptr;analogHour_=analogMinute_=nullptr;
  masks_.clear();
  pending_.clear();
  if (group_)
    lv_group_delete(group_);
  group_ = lv_group_create();
  auto *l = lv_label_create(screen_);
  lv_obj_set_width(l, 650);
  lv_obj_set_pos(l, 30, 60);
  std::string msg =
      "Tab5Symbian runtime 1.0.0 / API 14\nRecuperacao\n\n" + why +
      "\n\nPrepare /SymbianOS no microSD FAT32.\nRestaure os arquivos e "
      "reinicie.\nNenhum cartao sera formatado.\nEnter: tentar novamente";
  lv_label_set_text(l, msg.c_str());
  lv_obj_set_style_bg_color(screen_, lv_color_hex(0x292b2f), 0);
  lv_obj_set_style_text_color(screen_, lv_color_hex(0xeeeeee), 0);
  button(screen_, "retry", "Tentar novamente", 30, 420, 300, 70);
  button(screen_, "restore_release", "Release anterior", 350, 420, 300, 70);
  lv_screen_load(screen_);
  if (old)
    lv_obj_delete(old);
  // Shutdown can wait for a pending HTTP/DNS operation. Keep LVGL alive
  // while the existing worker closes services; never join network on LVGL.
  recoveryCleaning_=true;busy_=true;completed_=false;
  if(!worker_.submit([this]{runtime_.shutdown();completed_.store(true,std::memory_order_release);})) {
    recoveryCleaning_=false;busy_=false;
  }
}
void UI::icon(lv_obj_t *parent, const std::string &name, int x, int y,
              int size) {
  auto found = icons_.find(name);
  if (found == icons_.end())
    found = icons_.find("menu");
  if (found == icons_.end())
    return;
  auto *image = lv_image_create(parent);
  lv_image_set_src(image, &found->second);
  lv_obj_set_pos(image, x, y);
  lv_obj_set_size(image,size,size);
  lv_image_set_inner_align(image,LV_IMAGE_ALIGN_STRETCH);
  lv_image_set_pivot(image, 0, 0);
  lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
}
void UI::showPicture(std::shared_ptr<const Picture> picture, lv_obj_t *parent,
                     int width, int height, bool zoom) {
  if (!picture)
    return;
  auto previous = displayedPicture_;
  lv_image_cache_drop(&pictureDescriptor_);
  lv_image_header_cache_drop(&pictureDescriptor_);
  displayedPicture_ = picture;
  pictureDescriptor_ = {};
  pictureDescriptor_.header.magic = LV_IMAGE_HEADER_MAGIC;
  pictureDescriptor_.header.cf = LV_COLOR_FORMAT_RGB565;
  pictureDescriptor_.header.w = picture->width;
  pictureDescriptor_.header.h = picture->height;
  pictureDescriptor_.header.stride = picture->width * 2;
  pictureDescriptor_.data_size = picture->pixels.size();
  pictureDescriptor_.data =
      reinterpret_cast<const uint8_t *>(picture->pixels.data());
  if (!liveImage_)
    liveImage_ = lv_image_create(parent);
  lv_image_set_src(liveImage_, &pictureDescriptor_);
  lv_image_set_pivot(liveImage_, 0, 0);
  double scale = zoom ? 1
                      : std::min(double(width) / picture->width,
                                 double(height) / picture->height);
  lv_image_set_scale(liveImage_, std::max(1, int(scale * 256)));
  lv_obj_set_pos(liveImage_,
                 std::max(0, int((width - picture->width * scale) / 2)),
                 std::max(0, int((height - picture->height * scale) / 2)));
}
void UI::slide(lv_event_t *event) {
  auto *self=static_cast<UI *>(lv_event_get_user_data(event));auto *object=lv_event_get_target_obj(event);
  auto found=self->sliders_.find(object);if(found==self->sliders_.end()) return;
  int value=lv_slider_get_value(object);lv_label_set_text(found->second.caption,(std::to_string(value)+"%").c_str());
  if(found->second.kind=="brightness") self->runtime_.brightness=value;
  if(found->second.kind=="volume" || found->second.kind=="player_volume") self->runtime_.media().setVolume(value);
  if(lv_event_get_code(event)==LV_EVENT_RELEASED && found->second.kind=="player_volume" && self->pending_.size()<16)
    self->pending_.push_back({"volume_set",std::to_string(value),self->renderOwner_,true});
}
lv_obj_t *UI::slider(lv_obj_t *parent,const std::string &kind,int value,int minimum,int x,int y,int width) {
  auto *object=lv_slider_create(parent);lv_obj_set_pos(object,x,y+16);lv_obj_set_size(object,width-90,18);
  lv_slider_set_range(object,minimum,100);lv_slider_set_value(object,value,LV_ANIM_OFF);value=lv_slider_get_value(object);lv_obj_set_ext_click_area(object,18);
  auto *caption=lv_label_create(parent);lv_obj_set_pos(caption,x+width-78,y+5);lv_obj_set_size(caption,78,40);
  lv_label_set_text(caption,(std::to_string(value)+"%").c_str());
  sliders_[object]={caption,kind,value};lv_group_add_obj(group_,object);
  lv_obj_add_event_cb(object,slide,LV_EVENT_VALUE_CHANGED,this);lv_obj_add_event_cb(object,slide,LV_EVENT_RELEASED,this);return object;
}
void UI::render() {
  HtmlAllocationScope allocation;
  refreshed_=false;
  if (!runtime_.ready()) {
    keys_.reset();
    recovery(runtime_.error);
    return;
  }
  if(packagePane_){
    if(packageScroll_.size()>32){packageScroll_.clear();packageFocus_.clear();}
    packageScroll_[packageKey_]=lv_obj_get_scroll_y(packagePane_);
    auto *focused=group_?lv_group_get_focused(group_):nullptr;
    if(focused&&lv_obj_get_parent(focused)==packagePane_){auto index=reinterpret_cast<uintptr_t>(lv_obj_get_user_data(focused));if(index<ids_.size())packageFocus_[packageKey_]=ids_[index];}
  }
  const auto &v = runtime_.view();
  auto translated=[&](const char *pt,const char *en,const char *es){return v.language=="en"?en:v.language=="es"?es:pt;};
  if(v.layout=="lock" && renderOwner_=="lock" && lockMask_ && lockTitle_) {
    lv_label_set_text(lockMask_,v.body.c_str());lv_label_set_text(lockTitle_,v.title.c_str());return;
  }
  keys_.reset();
  renderOwner_ = runtime_.current();
  if (group_)
    lv_group_delete(group_);
  group_ = lv_group_create();
  lv_group_set_default(group_);
  // Release the old tree before allocating widgets for the new view.
  auto *old = screen_;
  screen_ = lv_obj_create(nullptr);
  lv_screen_load(screen_);
  if (old)
    lv_obj_delete(old);
  edit_ = keyboard_ = message_ = jobControl_ = chatSend_ = nullptr;
  diagnosticPane_=diagnosticLabel_=nullptr;packagePane_=nullptr;packageRows_.clear();
  battery_ = batteryGauge_ = nullptr;
  status_ = liveStatusLabel_ = liveImage_ = mediaTime_ = mediaLevel_ =
      mediaProgress_ = transportImage_ = nullptr;
  statusWidgets_.clear();
  thumbnailDescriptors_.clear();
  thumbnailPictures_.clear();
  imageWidth_ = imageHeight_ = 0;
  keyboardEdit_ = nullptr;
  editors_.clear();
  formWidgets_.clear();
  choices_.clear();
  sliders_.clear();digitalClock_=nullptr;processing_=nullptr;homeDate_=nullptr;lockMask_=lockTitle_=wifiDot_=wifiCross_=nullptr;for(auto &ring:wifiRings_)ring=nullptr;analogHour_=analogMinute_=nullptr;
  masks_.clear();
  ids_.clear();documentTargets_.clear();
  all_ = false;
  anchor_ = -1;
  selected_.clear();
  int w = lv_display_get_horizontal_resolution(nullptr),
      h = lv_display_get_vertical_resolution(nullptr);
  auto color = [](const std::string &s) {
    return lv_color_hex(strtoul(s.c_str(), nullptr, 16));
  };
  auto bg = color(v.background), fg = color(v.foreground),
       panel = color(v.fieldColor);
  lv_obj_remove_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_text_font(screen_, font_, 0);
  lv_obj_set_style_bg_color(screen_, bg, 0);
  lv_obj_set_style_text_color(screen_, fg, 0);
  lv_image_cache_drop(&wallpaperDescriptor_);
  wallpaperPicture_=v.wallpaper;
  if(wallpaperPicture_) {
    wallpaperDescriptor_={};wallpaperDescriptor_.header.magic=LV_IMAGE_HEADER_MAGIC;
    wallpaperDescriptor_.header.cf=LV_COLOR_FORMAT_RGB565;
    wallpaperDescriptor_.header.w=wallpaperPicture_->width;wallpaperDescriptor_.header.h=wallpaperPicture_->height;
    wallpaperDescriptor_.header.stride=wallpaperPicture_->width*2;
    wallpaperDescriptor_.data_size=wallpaperPicture_->pixels.size();
    wallpaperDescriptor_.data=reinterpret_cast<const uint8_t *>(wallpaperPicture_->pixels.data());
    auto *image=lv_image_create(screen_);lv_image_set_src(image,&wallpaperDescriptor_);
    lv_image_set_pivot(image,0,0);lv_image_set_scale(image,w*256/wallpaperPicture_->width);
    lv_obj_remove_flag(image,LV_OBJ_FLAG_CLICKABLE);
    auto *shade=lv_obj_create(screen_);lv_obj_remove_style_all(shade);lv_obj_set_size(shade,w,h);lv_obj_set_style_bg_color(shade,lv_color_hex(0x080b10),0);lv_obj_set_style_bg_opa(shade,LV_OPA_50,0);lv_obj_remove_flag(shade,LV_OBJ_FLAG_CLICKABLE);
  }
  const int sidebarInset=v.sidebar.empty()?0:76;
  auto label = [&](lv_obj_t *p, const std::string &s, int x, int y, int width,
                   int height) {
    if(sidebarInset && p==screen_ && y>=160 && y<h-164 && x<80){x+=sidebarInset;width=std::max(1,width-sidebarInset);}
    auto *o = lv_label_create(p);
    lv_label_set_text(o, s.c_str());
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, width, height);
    lv_label_set_long_mode(o, LV_LABEL_LONG_DOT);
    return o;
  };
  auto pane = [&](int x, int y, int width, int height) {
    if(sidebarInset && y>=160 && y<h-164 && x<80){x+=sidebarInset;width=std::max(1,width-sidebarInset);}
    auto *o = lv_obj_create(screen_);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, width, height);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_bg_color(o, panel, 0);
    if(v.wallpaper) lv_obj_set_style_bg_opa(o,LV_OPA_80,0);
    lv_obj_set_style_text_color(o, fg, 0);
    lv_obj_set_scroll_dir(o, LV_DIR_VER);
    return o;
  };
  auto tool = [&](lv_obj_t *parent, const Button &b, int x, int yy,
                  int width = 80, int height = 72) {
    auto *o = button(parent, b.id, b.label, x, yy, width, height);
    lv_obj_set_style_pad_all(o, 0, 0);
    if (!b.icon.empty()) {
      auto *caption = lv_obj_get_child(o, 0);
      lv_obj_set_style_text_font(caption, compactFont_ ? compactFont_ : font_,
                                 0);
      lv_obj_set_height(caption, 26);
      lv_obj_align(caption, LV_ALIGN_BOTTOM_MID, 0, 0);
      int size = b.id == "capture" ? 58 : 40;
      if(height<=64 && width>=110) {
        lv_obj_set_align(caption,LV_ALIGN_TOP_LEFT);lv_obj_set_pos(caption,40,(height-30)/2);lv_obj_set_size(caption,width-46,32);
        lv_obj_set_style_text_align(caption,LV_TEXT_ALIGN_LEFT,0);
        icon(o,b.icon,8,(height-26)/2,26);return o;
      }
      if (height <= 48)
        lv_label_set_text(caption, "");
      icon(o, b.icon, (width - size) / 2, height <= 48 ? 1 : 0, size);
      if (b.id == "toggle")
        transportImage_ = lv_obj_get_child(o, lv_obj_get_child_count(o) - 1);
    }
    return o;
  };
  if(v.layout=="lock") {
    auto *panel= pane(w/2-225,100,450,520);
    lv_obj_remove_flag(panel,LV_OBJ_FLAG_SCROLLABLE);
    status_=label(panel,statusClock(runtime_),20,12,410,40);
    auto *masked=label(panel,v.body,20,64,410,72);lockMask_=masked;
    lv_obj_set_style_text_align(masked,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_font(masked,largeFont_,0);
    for(size_t i=0;i<v.buttons.size();++i) {
      const auto &b=v.buttons[i];
      button(panel,b.id,b.label,12+int(i%3)*142,136+int(i/3)*86,134,78);
    }
    lockTitle_=label(screen_,v.title,36,22,w-72,54);
    return;
  }
  if (v.layout == "camera" || v.layout == "video_view" || v.layout == "photo") {
    auto *viewport = pane(0, 0, w, h);
    lv_obj_set_style_bg_color(viewport, lv_color_hex(0x08090b), 0);
    lv_obj_remove_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
    imageWidth_ = v.layout == "photo" ? 0 : w;
    imageHeight_ = h;
    auto picture = v.layout == "photo" ? v.picture : runtime_.media().picture();
    if (!picture) {
      auto empty = std::make_shared<Picture>();
      empty->width = 16;
      empty->height = 9;
      empty->pixels.assign(16 * 9 * 2, 0);
      picture = empty;
    }
    unsigned magnification=v.layout=="photo" && v.result=="zoom4" ? 4 : v.layout=="photo" && v.result=="zoom2" ? 2 : 1;
    if(magnification>1) {
      const double fit=std::min(double(w)/picture->width,double(h)/picture->height);
      auto *area=lv_obj_create(viewport);lv_obj_remove_style_all(area);
      int wide=int(picture->width*fit*magnification),high=int(picture->height*fit*magnification);
      lv_obj_set_size(area,wide,high);lv_obj_remove_flag(area,LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_flag(viewport,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(viewport,LV_DIR_ALL);
      showPicture(picture,area,wide,high);
    } else showPicture(picture, viewport, w, h);
    tool(screen_,{"back","Voltar","","ui_back"},16,16,138,48);
    tool(screen_,{"home","Início","","ui_home"},168,16,118,48);
    int x = (w - int(v.tools.size()) * 104) / 2;
    for (const auto &b : v.tools) {
      tool(screen_, b, x, h - 100, 92, 84);
      x += 104;
    }
    message_ = label(screen_, v.body, 310, 22, w - 330, 34);
    if (v.layout != "photo")
      liveStatusLabel_ =
          label(screen_, runtime_.media().cameraStatus(), 24, 80, w - 48, 34);
    return;
  }
  auto *top = pane(0, 0, w, 112);
  lv_obj_remove_flag(top, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(top, lv_color_hex(0x202027), 0);
  lv_obj_set_style_bg_grad_color(top, lv_color_hex(0x09090c), 0);
  lv_obj_set_style_bg_grad_dir(top, LV_GRAD_DIR_VER, 0);
  icon(top,v.profile=="silent"?"ui_silent":v.profile=="offline"?"ui_offline":"ui_normal",22,6,26);
  label(top,!v.profileLabel.empty()?v.profileLabel:v.profile=="silent"?"Silencioso":v.profile=="offline"?"Offline":"Normal",58,5,290,32);
  processing_=lv_spinner_create(top);lv_obj_set_pos(processing_,w-580,7);lv_obj_set_size(processing_,28,28);lv_spinner_set_anim_params(processing_,900,220);lv_obj_set_style_arc_width(processing_,3,LV_PART_MAIN);lv_obj_set_style_arc_width(processing_,3,LV_PART_INDICATOR);lv_obj_add_flag(processing_,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(processing_,LV_OBJ_FLAG_CLICKABLE);lv_obj_set_ext_click_area(processing_,12);lv_obj_add_event_cb(processing_,[](lv_event_t *e){auto *ui=static_cast<UI *>(lv_event_get_user_data(e));if(ui->busy_)ui->runtime_.cancel();},LV_EVENT_CLICKED,this);
  batteryGauge_ = lv_bar_create(top);
  lv_obj_set_pos(batteryGauge_, w - 535, 12);
  lv_obj_set_size(batteryGauge_, 34, 18);
  lv_obj_set_style_border_width(batteryGauge_, 2, 0);
  lv_obj_set_style_border_color(batteryGauge_, lv_color_hex(0xeeeeee), 0);
  lv_obj_set_style_radius(batteryGauge_, 2, 0);
  lv_bar_set_range(batteryGauge_, 0, 100);
  auto *terminal = pane(w - 501, 17, 4, 8);
  lv_obj_set_parent(terminal, top);
  lv_obj_set_style_bg_color(terminal, lv_color_hex(0xeeeeee), 0);
  auto initialPower = runtime_.powerInfo();
  battery_ =
      label(top,
            initialPower.available ? std::to_string(initialPower.percent) +
                                         (initialPower.charging ? "%+" : "%")
                                   : "--%",
            w - 491, 5, 72, 32);
  lv_bar_set_value(batteryGauge_,
                   initialPower.available ? initialPower.percent : 0,
                   LV_ANIM_OFF);
  for(unsigned i=0;i<3;++i) {
    int radius=6+int(i)*6;auto *arc=lv_arc_create(top);wifiRings_[i]=arc;
    lv_obj_set_pos(arc,w-420+18-radius,29-radius);lv_obj_set_size(arc,radius*2,radius*2);
    lv_arc_set_bg_angles(arc,225,315);lv_obj_set_style_arc_width(arc,3,LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc,LV_OPA_TRANSP,LV_PART_INDICATOR);lv_obj_set_style_bg_opa(arc,LV_OPA_TRANSP,LV_PART_KNOB);
    lv_obj_remove_flag(arc,LV_OBJ_FLAG_CLICKABLE);lv_obj_remove_flag(arc,LV_OBJ_FLAG_CLICK_FOCUSABLE);
  }
  wifiDot_=lv_obj_create(top);lv_obj_remove_style_all(wifiDot_);lv_obj_set_pos(wifiDot_,w-404,29);lv_obj_set_size(wifiDot_,5,5);lv_obj_set_style_radius(wifiDot_,LV_RADIUS_CIRCLE,0);lv_obj_set_style_bg_opa(wifiDot_,LV_OPA_COVER,0);
  wifiCross_=label(top,"x",w-395,17,20,24);if(compactFont_)lv_obj_set_style_text_font(wifiCross_,compactFont_,0);updateWifi();
  status_ = label(top, statusClock(runtime_), w - 370, 5, 346, 32);
  lv_obj_set_style_text_align(status_, LV_TEXT_ALIGN_RIGHT, 0);
  icon(top, renderOwner_ == "shell" ? "menu" : renderOwner_=="energy"?"settings":renderOwner_, 20, 48, 48);
  label(top, v.title, 84, 52, w - 560, 44);
  tool(top,{"home",v.home,"","ui_home"},w-430,48,128,48);
  tool(top,{"menu",v.menu,"","ui_grid"},w-292,48,128,48);
  tool(top,{"switcher",v.switcher,"","ui_apps"},w-154,48,132,48);
  auto *hint = label(screen_, v.body, 24, 122, w - 48, 34);
  liveStatusLabel_ = v.mediaStatus ? hint : nullptr;
  lv_obj_set_style_text_color(hint, lv_color_hex(0xb7c4d1), 0);
  const bool browserDocument=renderOwner_=="browser" && (v.layout=="reader" || v.layout=="web_document");
  const bool webDocument=v.layout=="web_document";
  const bool calculator = v.layout == "calculator",
             contacts = v.layout == "contacts", files = v.layout == "files",
             imageView = v.layout == "image" || v.layout == "image_zoom" || webDocument;
  mediaFineTime_ = v.layout == "recorder";
  const bool player = v.layout == "player", recorder = v.layout == "recorder",
             gallery = v.layout == "gallery";
  const bool packages=v.layout=="packages";
  bool clock=v.layout=="clock",switcher=v.layout=="switcher" || v.layout=="packages";
  const bool grid = v.layout == "grid" || v.layout == "home";
  const bool calendar=v.layout=="calendar";
  const bool mailCompose=v.layout=="mail_compose";
  const bool form = v.layout == "form" || mailCompose;
  const bool meshChat=v.layout=="chat" && v.field && v.controls.size()==3;
  const bool meshNodes=v.layout=="mesh_nodes";
  int y = 168, contentH = browserDocument?h-258:webDocument?360:h - 280;
  auto editor = [&](const std::string &value, int x, int yy, int width,
                    int height, bool one) {
    if(sidebarInset && yy>=160 && yy<h-164){x+=sidebarInset;width=std::max(1,width-sidebarInset);}
    auto *o = lv_textarea_create(screen_);
    lv_obj_set_pos(o, x, yy);
    lv_obj_set_size(o, width, height);
    lv_obj_set_style_radius(o, 3, 0);
    lv_obj_set_style_bg_color(o, panel, 0);
    lv_obj_set_style_text_color(o, fg, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(0x697580), 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(0x41658c), LV_PART_SELECTED);
    lv_textarea_set_max_length(o, 16000);
    lv_textarea_set_one_line(o, one);
    setEditorText(o, value.c_str());
    lv_textarea_set_text_selection(o, true);
    lv_obj_add_event_cb(o, changed, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(o, changed, LV_EVENT_FOCUSED, this);
    lv_group_add_obj(group_, o);
    editors_.push_back(o);
    return o;
  };
  int actionX = 18, actionW = v.layout == "notebook" ? w * 3 / 5 - 36 : w - 36,
      actionY = y, actionH = contentH;
  if (v.field && !calendar && !meshChat) {
    if (form) {
      auto controls = v.controls;
      if (controls.empty())
        for (const auto &name : splitLines(v.fields))
          controls.push_back({name, "text", "", ""});
      auto values = splitLines(v.text);
      if(mailCompose){auto at=v.text.find('\n');if(at!=v.text.npos)at=v.text.find('\n',at+1);if(at!=v.text.npos && values.size()>=3){values[2]=v.text.substr(at+1);values.resize(3);}}
      int step = mailCompose?80:std::min(116, contentH / std::max(1, int(controls.size())));
      for (size_t n = 0; n < controls.size(); ++n) {
        const auto &c = controls[n];
        const auto value = n < values.size() ? values[n] : "";
        int labelH=controls.size()>4?26:32;
        auto *caption=label(screen_, c.label, 26, y + int(n) * step, mailCompose?w-52:w * 3 / 5 - 52, labelH);
        if(controls.size()>4 && compactFont_) lv_obj_set_style_text_font(caption,compactFont_,0);
        int fieldH=mailCompose?(n==2?contentH-188:46):std::min(58,step-labelH-3);
        lv_obj_t *widget;
        if(c.type=="slider") {
          widget=slider(screen_,c.keys,atoi(value.c_str()),c.options=="10"?10:0,36,y+labelH+int(n)*step,w*3/5-60);
        } else if (c.type == "select") {
          widget = lv_dropdown_create(screen_);
          lv_obj_set_pos(widget, 24, y + labelH + int(n) * step);
          lv_obj_set_size(widget, w * 3 / 5 - 48, fieldH);
          lv_dropdown_set_options(widget, c.options.c_str());
          lv_dropdown_set_symbol(widget, LV_SYMBOL_DOWN);
          lv_obj_set_style_text_font(widget, LV_FONT_DEFAULT,
                                     LV_PART_INDICATOR);
          auto keys = splitLines(c.keys.empty() ? c.options : c.keys);
          auto found = std::find(keys.begin(), keys.end(), value);
          lv_dropdown_set_selected(
              widget, found == keys.end() ? 0 : found - keys.begin());
          choices_[widget] = std::move(keys);
          lv_group_add_obj(group_, widget);
          lv_obj_set_style_bg_color(widget, panel, 0);
          lv_obj_set_style_text_color(widget, fg, 0);
          auto *list = lv_dropdown_get_list(widget);
          lv_obj_set_style_text_font(list, font_, 0);
          lv_obj_set_style_bg_color(list, panel, 0);
          lv_obj_set_style_text_color(list, fg, 0);
          lv_obj_set_style_max_height(list, 340, 0);
        } else {
          widget = editor(value, 24, y + labelH + int(n) * step, mailCompose?w-48:w * 3 / 5 - 48, fieldH,
                          c.type!="multiline");
          if(mailCompose && n==2)lv_textarea_set_max_length(widget,4096);
          if(c.type=="pin") { lv_textarea_set_accepted_chars(widget,"0123456789");lv_textarea_set_max_length(widget,4); }
          if (c.type == "password" || c.type=="pin")
            lv_textarea_set_password_mode(widget, true);
          if (!edit_)
            edit_ = widget;
          if (c.type == "date" || c.type == "time") {
            masks_[widget] = c.type == "date" ? "00/00/0000" : "00:00";
            lv_textarea_set_max_length(widget, c.type == "date" ? 10 : 5);
            lv_textarea_set_placeholder_text(
                widget, c.type == "date" ? "DD/MM/AAAA" : "HH:MM");
          }
        }
        lv_obj_set_style_pad_top(widget,4,0);lv_obj_set_style_pad_bottom(widget,4,0);
        formWidgets_.push_back(widget);
      }
      editHeight_ = 58;
      actionX = w * 3 / 5;
      actionW = w - actionX - 20;
      if(mailCompose){actionY=y+contentH+8;actionH=0;}
    } else if(v.layout=="reader") {
      edit_=editor(v.text,24,y,browserDocument?w-176:w-48,browserDocument?contentH-(v.documentLinks.empty()?0:116):contentH-54,false);editHeight_=contentH;
      actionY=y;actionH=0;
      if(browserDocument&&!v.documentLinks.empty()){auto *list=pane(24,y+contentH-108,w-176,108);unsigned i=0;for(const auto &entry:v.documentLinks){auto *link=lv_button_create(list);lv_obj_set_pos(link,0,int(i++)*38);lv_obj_set_size(link,w-184-sidebarInset,34);lv_obj_set_style_pad_all(link,0,0);auto *caption=lv_label_create(link);lv_label_set_text(caption,entry.label.c_str());lv_obj_set_width(caption,w-220-sidebarInset);lv_label_set_long_mode(caption,LV_LABEL_LONG_DOT);lv_obj_set_style_text_font(caption,compactFont_?compactFont_:font_,0);lv_obj_set_style_text_color(caption,lv_color_hex(0x56bbff),0);lv_obj_set_user_data(link,reinterpret_cast<void*>(documentTargets_.size()));documentTargets_.push_back(entry.url);lv_obj_add_event_cb(link,documentLink,LV_EVENT_CLICKED,this);lv_group_add_obj(group_,link);}}
    } else if (contacts || files) {
      edit_ = editor(v.text, 24, y, files ? w * 3 / 4 - 48 : w - 48, 62, true);
      editHeight_ = 62;
      actionY = y + 76;
      actionH = contentH - 76;
    } else if (calculator) {
      edit_ = editor(v.text, 24, y, w - 48, 90, true);
      editHeight_ = 90;
      lv_obj_set_style_text_align(edit_, LV_TEXT_ALIGN_RIGHT, 0);
      if (largeFont_)
        lv_obj_set_style_text_font(edit_, largeFont_, 0);
      actionY = y + 106;
      actionH = contentH - 106;
    } else {
      edit_ = editor(v.text, 24, y, browserDocument?w-176:w * 3 / 5 - 44, contentH, false);
      editHeight_ = contentH;
      actionX = w * 3 / 5;
      actionW = w - actionX - 20;
      auto pos = editorPositions_.find(renderOwner_);
      if (pos != editorPositions_.end()) {
        lv_textarea_set_cursor_pos(edit_, pos->second.first);
        lv_obj_scroll_to_y(edit_, pos->second.second, LV_ANIM_OFF);
      }
    }
    if (edit_) {
      lv_group_focus_obj(edit_);
      if (contacts)
        lv_textarea_set_placeholder_text(edit_, "Buscar contato");
      if (calculator)
        lv_textarea_set_placeholder_text(edit_, "0");
    }
  }
  if(v.layout=="mail_html" || v.layout=="html") {
    diagnosticPane_=pane(24,y,w-48,contentH);
    lv_obj_set_style_bg_color(diagnosticPane_,lv_color_white(),0);
    lv_obj_set_style_bg_opa(diagnosticPane_,LV_OPA_COVER,0);
    lv_obj_set_style_text_color(diagnosticPane_,lv_color_hex(0x202124),0);
    lv_obj_set_style_text_font(diagnosticPane_,compactFont_?compactFont_:font_,0);
    lv_obj_set_style_pad_all(diagnosticPane_,16,0);
    lv_obj_set_style_pad_row(diagnosticPane_,12,0);
    lv_obj_set_flex_flow(diagnosticPane_,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(diagnosticPane_,LV_DIR_VER);
    thumbnailDescriptors_.reserve(v.document.size());
    for(const auto &block:v.document){
      if(!block.source.empty()){
        if(block.picture){
          thumbnailPictures_.push_back(block.picture);thumbnailDescriptors_.emplace_back();auto &d=thumbnailDescriptors_.back();
          d.header.magic=LV_IMAGE_HEADER_MAGIC;d.header.cf=LV_COLOR_FORMAT_RGB565;
          d.header.w=block.picture->width;d.header.h=block.picture->height;d.header.stride=block.picture->width*2;
          d.data_size=block.picture->pixels.size();d.data=(const uint8_t*)block.picture->pixels.data();
          auto *image=lv_image_create(diagnosticPane_);lv_image_set_src(image,&d);
        }else {
          auto message=block.alt.empty()?std::string(translated("[Imagem não carregada]","[Image not loaded]","[Imagen sin cargar]")):(std::string(translated("[Imagem: ","[Image: ","[Imagen: "))+block.alt+"]");
          auto *missing=lv_label_create(diagnosticPane_);lv_obj_set_width(missing,LV_PCT(100));lv_label_set_text(missing,message.c_str());
        }
      }else if(block.runs.size()==1 && !block.heading && !block.quote && !block.runs[0].strong && !block.runs[0].emphasis && !block.runs[0].underline && block.runs[0].href.empty()){
        auto *text=lv_label_create(diagnosticPane_);lv_obj_set_width(text,LV_PCT(100));
        lv_label_set_long_mode(text,LV_LABEL_LONG_WRAP);lv_label_set_text(text,block.runs[0].text.c_str());
        lv_obj_set_style_text_align(text,block.align=="center"?LV_TEXT_ALIGN_CENTER:block.align=="right"?LV_TEXT_ALIGN_RIGHT:LV_TEXT_ALIGN_LEFT,0);
      }else if(!block.runs.empty()){
        auto *paragraph=lv_spangroup_create(diagnosticPane_);lv_obj_set_width(paragraph,LV_PCT(100));
        lv_spangroup_set_mode(paragraph,LV_SPAN_MODE_BREAK);
        lv_spangroup_set_align(paragraph,block.align=="center"?LV_TEXT_ALIGN_CENTER:block.align=="right"?LV_TEXT_ALIGN_RIGHT:LV_TEXT_ALIGN_LEFT);
        if(block.quote){lv_obj_set_style_pad_left(paragraph,16,0);lv_obj_set_style_border_side(paragraph,LV_BORDER_SIDE_LEFT,0);lv_obj_set_style_border_width(paragraph,3,0);}
        for(const auto &run:block.runs){
          auto *span=lv_spangroup_add_span(paragraph);lv_span_set_text(span,run.text.c_str());auto *style=lv_span_get_style(span);
          lv_style_set_text_font(style,block.heading&&block.heading<=2?font_:(compactFont_?compactFont_:font_));
          lv_style_set_text_color(style,lv_color_hex(run.href.empty()?(run.emphasis?0x505050:0x202124):0x1565c0));
          if(run.underline||!run.href.empty())lv_style_set_text_decor(style,LV_TEXT_DECOR_UNDERLINE);
          if(run.strong||block.heading){lv_style_set_text_outline_stroke_width(style,1);lv_style_set_text_outline_stroke_color(style,lv_color_hex(0x202124));lv_style_set_text_outline_stroke_opa(style,LV_OPA_COVER);}
        }
        lv_spangroup_refresh(paragraph);
      }
    }
    lv_group_add_obj(group_,diagnosticPane_);lv_group_focus_obj(diagnosticPane_);
    actionH=0;
  }
  if(v.layout=="diagnostic") {
    diagnosticPane_=pane(24,y,w-48,contentH);lv_obj_set_scroll_dir(diagnosticPane_,LV_DIR_VER);
    lv_obj_set_style_bg_opa(diagnosticPane_,LV_OPA_COVER,0);
    diagnosticLabel_=label(diagnosticPane_,v.text,14,12,w-104,LV_SIZE_CONTENT);
    lv_label_set_long_mode(diagnosticLabel_,LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(diagnosticLabel_,compactFont_?compactFont_:font_,0);
    lv_obj_set_height(diagnosticLabel_,LV_SIZE_CONTENT);
    lv_group_add_obj(group_,diagnosticPane_);lv_group_focus_obj(diagnosticPane_);
    actionY=y+contentH+8;actionH=0;
  }
  if(!v.statusFields.empty() && v.layout!="home" && !meshNodes) {
    int bw=(w-60)/2;
    for(size_t i=0;i<v.statusFields.size();++i) {
      auto *card=pane(24+int(i%2)*(bw+12),y+int(i/2)*88,bw,80);
      lv_obj_set_style_radius(card,8,0);
      auto *name=label(card,v.statusFields[i].label,12,5,bw-24,26);
      if(compactFont_) lv_obj_set_style_text_font(name,compactFont_,0);
      auto *value=label(card,v.statusFields[i].value,12,32,bw-24,42);
      lv_label_set_long_mode(value,LV_LABEL_LONG_WRAP);
      lv_obj_t *bar=nullptr;
      if(v.statusFields[i].percent>=0) {bar=lv_bar_create(card);lv_obj_set_pos(bar,12,72);lv_obj_set_size(bar,bw-24,5);lv_bar_set_value(bar,v.statusFields[i].percent,LV_ANIM_OFF);}
      statusWidgets_.push_back({value,bar,v.statusFields[i].value});
    }
    int rows=(int(v.statusFields.size())+1)/2;actionY=y+rows*88+10;actionH=std::max(0,contentH-rows*88-10);
  }
  if(clock) {
    auto *card=pane(24,y,370,350);lv_obj_set_style_radius(card,12,0);
    icon(card,"clock",140,14,80);
    auto now=runtime_.clockText();digitalClock_=label(card,now.size()>20?now.substr(12,8):"--:--:--",20,120,330,78);
    if(largeFont_) lv_obj_set_style_text_font(digitalClock_,largeFont_,0);
    lv_obj_set_style_text_align(digitalClock_,LV_TEXT_ALIGN_CENTER,0);
    label(card,now.substr(0,10),90,210,230,40);label(card,v.text,24,266,322,66);
    actionX=420;actionW=w-444;
  }
  if (imageView) {
    displayedPicture_ = v.picture;
    if (displayedPicture_) {
      lv_image_cache_drop(&pictureDescriptor_);
      pictureDescriptor_ = {};
      pictureDescriptor_.header.magic = LV_IMAGE_HEADER_MAGIC;
      pictureDescriptor_.header.cf = LV_COLOR_FORMAT_RGB565;
      pictureDescriptor_.header.w = displayedPicture_->width;
      pictureDescriptor_.header.h = displayedPicture_->height;
      pictureDescriptor_.header.stride = displayedPicture_->width * 2;
      pictureDescriptor_.data_size = displayedPicture_->pixels.size();
      pictureDescriptor_.data =
          reinterpret_cast<const uint8_t *>(displayedPicture_->pixels.data());
      auto *viewport = pane(18, y, browserDocument?w-160:webDocument?w-36:w * 3 / 4 - 36, contentH);
      lv_obj_set_scroll_dir(viewport, LV_DIR_ALL);
      auto *photo = lv_image_create(viewport);
      lv_image_set_src(photo, &pictureDescriptor_);
      lv_obj_set_pos(photo, 0, 0);
      lv_image_set_pivot(photo, 0, 0);
      double scale = std::min(double(browserDocument?w-176-sidebarInset:webDocument?w-48-sidebarInset:w * 3 / 4 - 48) / displayedPicture_->width,
                              double(contentH) / displayedPicture_->height);
      lv_image_set_scale(photo, v.layout == "image_zoom"
                                    ? 256
                                    : std::max(1, int(scale * 256)));
      if(browserDocument){double actual=std::max(1,int(scale*256))/256.0;for(const auto &hit:v.htmlHotspots){
        auto *link=lv_button_create(viewport);lv_obj_remove_style_all(link);lv_obj_set_pos(link,int(hit.x*actual),int(hit.y*actual));lv_obj_set_size(link,std::max(2,int(hit.width*actual)),std::max(2,int(hit.height*actual)));lv_obj_set_style_bg_opa(link,LV_OPA_TRANSP,0);lv_obj_set_style_outline_width(link,2,LV_STATE_FOCUSED);lv_obj_set_style_outline_color(link,lv_color_hex(0x0088ff),LV_STATE_FOCUSED);lv_obj_set_user_data(link,reinterpret_cast<void*>(documentTargets_.size()));documentTargets_.push_back(hit.url);lv_obj_add_event_cb(link,documentLink,LV_EVENT_CLICKED,this);lv_group_add_obj(group_,link);
      }}
    }
    actionX = w * 3 / 4;
    actionW = w - actionX - 20;
    if(webDocument)actionH=0;
  }
  if (player || recorder) {
    icon(screen_, recorder ? "recorder" : "music", 100, 196, 140);
    mediaTime_ = label(screen_, "00:00", 60, 350, 330, 74);
    lv_obj_set_style_text_font(mediaTime_, largeFont_, 0);
    lv_obj_set_style_text_align(mediaTime_, LV_TEXT_ALIGN_CENTER, 0);
    mediaLevel_ = lv_bar_create(screen_);
    lv_obj_set_pos(mediaLevel_, 60, 438);
    lv_obj_set_size(mediaLevel_, 310, 12);
    lv_bar_set_range(mediaLevel_, 0, 100);
    mediaProgress_ = lv_bar_create(screen_);
    lv_obj_set_pos(mediaProgress_, 60, 468);
    lv_obj_set_size(mediaProgress_, 310, 6);
    lv_bar_set_range(mediaProgress_, 0, 100);
    slider(screen_,"player_volume",runtime_.media().volume(),0,60,495,320);
    int index = 0;
    for (const auto &b : v.tools) {
      tool(screen_, b, 34 + (index % 8) * 90, 552 + (index / 8) * 76, 80, 72);
      ++index;
    }
    actionX = 440;
    actionW = w - actionX - 24;
    actionH = 330;
  }
  if(v.layout=="options") {actionX=(w-600)/2;actionW=600;actionH=240;}
  if(v.layout=="home") {
    auto now=runtime_.clockText();auto *clockCard=pane(24,158,250,194);lv_obj_set_style_radius(clockCard,8,0);
    auto *face=lv_obj_create(clockCard);lv_obj_set_pos(face,71,5);lv_obj_set_size(face,108,108);lv_obj_set_style_radius(face,LV_RADIUS_CIRCLE,0);lv_obj_set_style_bg_color(face,lv_color_hex(0x131820),0);lv_obj_set_style_border_color(face,lv_color_hex(0x6b7784),0);lv_obj_set_style_pad_all(face,0,0);lv_obj_remove_flag(face,LV_OBJ_FLAG_SCROLLABLE);
    label(face,"12",40,0,32,26);label(face,"6",47,80,22,26);label(face,"9",5,40,22,26);label(face,"3",85,40,22,26);
    analogHour_=lv_line_create(face);analogMinute_=lv_line_create(face);lv_obj_set_style_line_width(analogHour_,5,0);lv_obj_set_style_line_width(analogMinute_,3,0);lv_obj_set_style_line_color(analogHour_,lv_color_hex(0xf4f7fa),0);lv_obj_set_style_line_color(analogMinute_,lv_color_hex(0x36d9e9),0);updateAnalog();
    digitalClock_=label(clockCard,now.size()>17?now.substr(12,5):"--:--",20,124,210,60);lv_obj_set_style_text_font(digitalClock_,largeFont_,0);lv_obj_set_style_text_align(digitalClock_,LV_TEXT_ALIGN_CENTER,0);
    auto *dateCard=pane(286,158,280,194);lv_obj_set_style_radius(dateCard,8,0);label(dateCard,translated("HOJE","TODAY","HOY"),20,14,240,28);homeDate_=label(dateCard,now.substr(0,10),20,62,240,44);icon(dateCard,"profiles",20,128,42);label(dateCard,!v.profileLabel.empty()?v.profileLabel:v.profile=="silent"?"Silencioso":v.profile=="offline"?"Offline":"Normal",76,132,190,40);
    auto *agendaCard=button(screen_,"open:agenda","",578,158,w-602,194);lv_obj_set_style_pad_all(agendaCard,0,0);icon(agendaCard,"agenda",16,12,44);label(agendaCard,translated("Próximos compromissos","Upcoming appointments","Próximas citas"),76,18,w-700,34);auto *agenda=label(agendaCard,v.statusFields.empty()?v.text:v.statusFields[0].value,20,76,w-646,108);lv_label_set_long_mode(agenda,LV_LABEL_LONG_WRAP);
    auto *notices=button(screen_,"notices","",24,366,w-48,104);lv_obj_set_style_pad_all(notices,0,0);icon(notices,"mesh_ble",16,28,44);label(notices,translated("Notificações","Notifications","Notificaciones"),76,32,190,40);auto *notice=label(notices,v.statusFields.size()>1?v.statusFields[1].value:"Você está em dia.",284,16,w-360,84);lv_label_set_long_mode(notice,LV_LABEL_LONG_WRAP);
    actionX=24;actionY=494;actionW=w-48;actionH=140;
  }
  if(meshChat) {
    auto values=splitLines(v.text);
    for(size_t i=0;i<2;++i) {
      const auto &c=v.controls[i];int x=24+int(i)*(w/2-12),width=w/2-36;
      auto *caption=label(screen_,c.label,x,y,width,26);if(compactFont_)lv_obj_set_style_text_font(caption,compactFont_,0);
      auto *choice=lv_dropdown_create(screen_);lv_obj_set_pos(choice,x,y+28);lv_obj_set_size(choice,width,46);
      lv_dropdown_set_options(choice,c.options.c_str());lv_dropdown_set_symbol(choice,LV_SYMBOL_DOWN);
      lv_obj_set_style_text_font(choice,compactFont_?compactFont_:font_,0);lv_obj_set_style_text_font(choice,LV_FONT_DEFAULT,LV_PART_INDICATOR);
      auto keys=splitLines(c.keys);auto found=std::find(keys.begin(),keys.end(),i<values.size()?values[i]:"");
      lv_dropdown_set_selected(choice,found==keys.end()?0:found-keys.begin());choices_[choice]=std::move(keys);formWidgets_.push_back(choice);lv_group_add_obj(group_,choice);
      lv_obj_set_style_bg_color(choice,panel,0);lv_obj_set_style_text_color(choice,fg,0);
      auto *list=lv_dropdown_get_list(choice);lv_obj_set_style_text_font(list,compactFont_?compactFont_:font_,0);lv_obj_set_style_bg_color(list,panel,0);lv_obj_set_style_text_color(list,fg,0);lv_obj_set_style_max_height(list,300,0);
    }
    edit_=editor(values.size()>2?values[2]:"",24,h-174,w-220,58,true);editHeight_=58;lv_textarea_set_max_length(edit_,228);lv_textarea_set_placeholder_text(edit_,v.language=="en"?"Write a message...":v.language=="es"?"Escribe un mensaje...":"Escreva uma mensagem...");formWidgets_.push_back(edit_);lv_group_focus_obj(edit_);
    chatSend_=tool(screen_,{"send","Enviar","","ui_export"},w-180,h-174,156,58);
    actionY=y+84;
  }
  if(meshNodes) {
    actionW=w*3/5-36;
    auto *detail=pane(w*3/5,y,w*2/5-24,h-y-90);lv_obj_set_style_radius(detail,8,0);
    for(size_t i=0;i<v.statusFields.size();++i) {
      int yy=12+int(i)*66;
      auto *caption=label(detail,v.statusFields[i].label,14,yy,w*2/5-56,24);if(compactFont_)lv_obj_set_style_text_font(caption,compactFont_,0);
      auto *value=label(detail,v.statusFields[i].value,14,yy+26,w*2/5-56,34);if(compactFont_)lv_obj_set_style_text_font(value,compactFont_,0);lv_label_set_long_mode(value,LV_LABEL_LONG_WRAP);
    }
  }
  const bool extraPagerRow=v.centerTools && (v.footer.size()+(!v.optionAction.empty()?1:0)+(edit_&&!v.readonly?1:0)>2);
  if(v.centerTools) actionH=h-actionY-(extraPagerRow?182:86);
  if(browserDocument){actionX=w-144;actionW=120;actionH=h-actionY-90;}
  if(meshChat) actionH=h-190-actionY;
  if(packages){actionW=w-242;actionH=h-actionY-118;}
  if(!v.tools.empty() && !player && !recorder && !v.centerTools && !packages) actionH=std::max(0,actionH-54);
  auto *actions = pane(actionX, actionY, actionW, actionH);
  if(v.buttons.empty() || v.centerTools || v.layout=="home" || v.layout=="chat") lv_obj_set_style_bg_opa(actions,LV_OPA_TRANSP,0);
  lv_obj_set_style_bg_color(actions,lv_color_hex(0x23d4e9),LV_PART_SCROLLBAR);lv_obj_set_style_width(actions,6,LV_PART_SCROLLBAR);
  if(packages){packagePane_=actions;packageKey_=renderOwner_+":"+v.scrollKey;lv_obj_set_scrollbar_mode(actions,LV_SCROLLBAR_MODE_ON);lv_obj_add_flag(actions,LV_OBJ_FLAG_SCROLL_MOMENTUM);lv_obj_remove_flag(actions,lv_obj_flag_t(LV_OBJ_FLAG_SCROLL_ELASTIC|LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER));lv_obj_set_style_pad_right(actions,12,0);}
  lv_obj_t *firstAction = nullptr;
  if(calendar) {
    const char *weekdaysPt[]={"Dom","Seg","Ter","Qua","Qui","Sex","Sáb"};
    const char *weekdaysEn[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    const char *weekdaysEs[]={"Dom","Lun","Mar","Mié","Jue","Vie","Sáb"};
    auto weekdays=v.language=="en"?weekdaysEn:v.language=="es"?weekdaysEs:weekdaysPt;
    int offset=std::max(0,std::min(6,atoi(v.result.c_str()))),bw=actionW/7;
    for(int i=0;i<7;++i) label(actions,weekdays[i],i*bw+8,0,bw-12,30);
    for(size_t i=0;i<v.buttons.size();++i) {
      const auto &b=v.buttons[i];int at=offset+int(i);
      auto *day=button(actions,b.id,b.label,(at%7)*bw,36+(at/7)*56,bw-8,50);
      if(!firstAction) firstAction=day;
    }
  } else if (gallery) {
    thumbnailDescriptors_.resize(v.buttons.size());
    thumbnailPictures_.resize(v.buttons.size());
    int bw = actionW / 4, bh = 190;
    for (size_t i = 0; i < v.buttons.size(); ++i) {
      const auto &b = v.buttons[i];
      auto *tile = button(actions, b.id, b.label, (i % 4) * bw, (i / 4) * bh,
                          bw - 12, bh - 12);
      lv_obj_set_style_pad_all(tile,0,0);
      auto *caption = lv_obj_get_child(tile, 0);
      lv_obj_set_height(caption,30);
      lv_obj_set_style_text_font(caption, compactFont_, 0);
      lv_obj_align(caption, LV_ALIGN_BOTTOM_MID, 0, 0);
      if (b.thumbnail) {
        auto &image = thumbnailDescriptors_[i];
        thumbnailPictures_[i] = b.thumbnail;
        image = {};
        image.header.magic = LV_IMAGE_HEADER_MAGIC;
        image.header.cf = LV_COLOR_FORMAT_RGB565;
        image.header.w = b.thumbnail->width;
        image.header.h = b.thumbnail->height;
        image.header.stride = b.thumbnail->width * 2;
        image.data_size = b.thumbnail->pixels.size();
        image.data =
            reinterpret_cast<const uint8_t *>(b.thumbnail->pixels.data());
        auto *photo = lv_image_create(tile);
        lv_image_set_src(photo, &image);
        lv_image_set_pivot(photo, 0, 0);
        double scale =
            std::min(double(bw - 32) / image.header.w, 140.0 / image.header.h);
        lv_image_set_scale(photo, int(scale * 256));
        lv_obj_set_pos(photo, ((bw - 12) - image.header.w * scale) / 2, 0);
      } else
        icon(tile, b.icon.empty() ? "gallery" : b.icon, (bw - 100) / 2, 24, 96);
    }
  } else if(v.layout=="options") {
    for(size_t i=0;i<v.buttons.size();++i) {
      const auto &b=v.buttons[i];auto *row=button(actions,b.id,b.label,0,int(i)*62,actionW-8,56);
      lv_obj_set_style_pad_all(row,0,0);icon(row,b.icon,12,8,40);
      auto *caption=lv_obj_get_child(row,0);lv_obj_set_pos(caption,66,10);lv_obj_set_size(caption,actionW-140,36);lv_obj_set_style_text_align(caption,LV_TEXT_ALIGN_LEFT,0);
      if(b.subtitle=="selected") {icon(row,"ui_ok",actionW-70,13,28);lv_obj_set_style_border_color(row,lv_color_hex(0x90dbac),0);}
      if(!firstAction) firstAction=row;
    }
  } else if(v.layout=="status" || v.layout=="actions" || v.layout=="choices") {
    int cols=v.layout=="choices"?3:3,bw=actionW/cols;
    for(size_t i=0;i<v.buttons.size();++i) {
      const auto &b=v.buttons[i];auto *tile=tool(actions,b,int(i%cols)*bw,int(i/cols)*100,bw-14,90);
      if(b.subtitle=="selected") lv_obj_set_style_border_color(tile,lv_color_hex(0x9bdba7),0);
      if(!firstAction) firstAction=tile;
    }
  } else if(switcher) {
    for(size_t i=0;i<v.buttons.size();++i) {
      const auto &b=v.buttons[i];int yy=int(i)*84;
      auto *row=button(actions,b.id,b.label,0,yy,actionW-82,76);lv_obj_set_style_pad_all(row,0,0);
      auto *caption=lv_obj_get_child(row,0);lv_obj_set_align(caption,LV_ALIGN_TOP_LEFT);lv_obj_set_pos(caption,62,4);lv_obj_set_size(caption,actionW-180,32);lv_obj_set_style_text_align(caption,LV_TEXT_ALIGN_LEFT,0);
      icon(row,b.icon,10,18,40);auto *usage=label(row,b.subtitle,62,42,actionW-180,28);if(compactFont_) lv_obj_set_style_text_font(usage,compactFont_,0);
      if(!b.close.empty()) {auto *close=tool(actions,{b.close,"","","ui_close"},actionW-72,yy+10,60,56);lv_obj_set_style_bg_color(close,lv_color_hex(0x6b3943),0);}
      if(packages){packageRows_.push_back(row);lv_obj_add_flag(row,LV_OBJ_FLAG_SCROLL_ON_FOCUS);lv_obj_remove_flag(row,LV_OBJ_FLAG_SCROLLABLE);}
      if(!firstAction || (packages&&packageFocus_[packageKey_]==b.id)) firstAction=row;
    }
  } else if(v.layout=="chat") {
    int yy=8,bw=actionW*3/5;
    for(const auto &b:v.buttons) {
      lv_point_t extent{};lv_text_get_size(&extent,b.label.c_str(),compactFont_?compactFont_:font_,0,0,bw-32,LV_TEXT_FLAG_NONE);
      int height=std::max(90,std::min(360,int(extent.y)+56));
      auto *bubble=button(actions,b.id,b.label,b.outgoing?actionW-bw-12:12,yy,bw,height);lv_obj_set_style_pad_all(bubble,0,0);lv_obj_set_style_radius(bubble,10,0);
      lv_obj_set_style_bg_color(bubble,lv_color_hex(b.outgoing?0x304e40:0x2b2e37),0);lv_obj_set_style_bg_grad_dir(bubble,LV_GRAD_DIR_NONE,0);
      auto *body=lv_obj_get_child(bubble,0);lv_obj_set_align(body,LV_ALIGN_TOP_LEFT);lv_obj_set_pos(body,16,10);lv_obj_set_size(body,bw-32,height-46);lv_label_set_long_mode(body,LV_LABEL_LONG_WRAP);if(compactFont_)lv_obj_set_style_text_font(body,compactFont_,0);lv_obj_set_style_text_align(body,b.outgoing?LV_TEXT_ALIGN_RIGHT:LV_TEXT_ALIGN_LEFT,0);
      auto *stamp=label(bubble,b.subtitle,16,height-30,bw-32,26);if(compactFont_)lv_obj_set_style_text_font(stamp,compactFont_,0);lv_obj_set_style_text_align(stamp,b.outgoing?LV_TEXT_ALIGN_RIGHT:LV_TEXT_ALIGN_LEFT,0);
      if(!firstAction)firstAction=bubble;
      yy+=height+14;
    }
    if(meshChat) {lv_obj_update_layout(actions);lv_obj_scroll_to_y(actions,LV_COORD_MAX,LV_ANIM_OFF);}
  } else if (grid) {
    int cols = v.layout=="home"?6:v.columns?int(v.columns):3, bw = (actionW - 24) / cols;
    int rowH = v.layout=="home"?134:v.centerTools?148:v.tools.empty()?146:124;
    for (size_t i = 0; i < v.buttons.size(); ++i) {
      const auto &b = v.buttons[i];
      int x = int(i % cols) * bw + 8, yy = int(i / cols) * rowH;
      auto *tile = button(actions, b.id, b.label, x, yy, bw - 14, rowH - 10);
      if (!firstAction)
        firstAction = tile;
      lv_obj_set_style_bg_opa(tile, LV_OPA_TRANSP, 0);
      lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, LV_STATE_FOCUSED);
      lv_obj_set_style_border_opa(tile, LV_OPA_TRANSP, 0);
      lv_obj_set_style_pad_all(tile,0,0);
      auto *caption = lv_obj_get_child(tile, 0);
      if(compactFont_) lv_obj_set_style_text_font(caption,compactFont_,0);
      lv_obj_set_height(caption,30);
      lv_obj_align(caption, LV_ALIGN_BOTTOM_MID, 0, -2);
      icon(tile, b.icon.empty() ? b.id.substr(b.id.find(':') + 1) : b.icon,
           (bw - 14 - 88) / 2, 8, 88);
    }
  } else if(v.layout=="list" && v.columns>1) {
    int cols=int(v.columns),bw=(actionW-16)/cols;
    for(size_t i=0;i<v.buttons.size();++i) {
      const auto &b=v.buttons[i];auto *row=button(actions,b.id,b.label,int(i%cols)*bw,int(i/cols)*88,bw-12,80);lv_obj_set_style_pad_all(row,0,0);
      auto *name=lv_obj_get_child(row,0);lv_obj_set_align(name,LV_ALIGN_TOP_LEFT);lv_obj_set_pos(name,70,b.subtitle.empty()?22:5);lv_obj_set_size(name,bw-90,32);lv_obj_set_style_text_align(name,LV_TEXT_ALIGN_LEFT,0);
      lv_obj_set_style_bg_opa(row,LV_OPA_TRANSP,0);lv_obj_set_style_bg_opa(row,LV_OPA_COVER,LV_STATE_FOCUSED);
      icon(row,b.icon,12,16,48);if(!b.subtitle.empty()){auto *sub=label(row,b.subtitle,70,45,bw-90,28);if(compactFont_)lv_obj_set_style_text_font(sub,compactFont_,0);}
      if(!firstAction)firstAction=row;
    }
  } else if (calculator) {
    int bw = (actionW - 12) / 4, bh = (actionH - 8) / 5;
    for (size_t i = 0; i < v.buttons.size(); ++i) {
      const auto &b = v.buttons[i];
      auto *key = button(actions, b.id, b.label, int(i % 4) * bw,
                         int(i / 4) * bh, bw - 8, bh - 6);
      if (b.id == "calculate")
        lv_obj_set_style_bg_color(key, lv_color_hex(0x477e9b), 0);
    }
  } else if(browserDocument) {
    int row=0;
    for(const auto &b:v.buttons){int index=row++;auto *item=button(actions,b.id,b.label,(index%2)*60,(index/2)*56,54,50);lv_obj_set_style_min_width(item,0,0);lv_obj_set_style_min_height(item,0,0);lv_label_set_long_mode(lv_obj_get_child(item,0),LV_LABEL_LONG_CLIP);lv_label_set_text(lv_obj_get_child(item,0),b.label.c_str());lv_obj_add_flag(lv_obj_get_child(item,0),LV_OBJ_FLAG_HIDDEN);lv_obj_set_style_pad_all(item,0,0);icon(item,b.icon.empty()?"ui_info":b.icon,9,7,36);if(!firstAction)firstAction=item;}
  } else {
    int row = 0, toolIndex = 0;
    const int side = contacts || files ? actionW * 3 / 4 : actionW;
    lv_obj_t *tools = nullptr;
    if (contacts || files) {
      tools = pane(actionX + side + 12, actionY, actionW - side - 12, actionH);
      lv_obj_set_width(actions, side);
    }
    for (const auto &b : v.buttons) {
      bool contact = (contacts && b.id.rfind("edit:", 0) == 0) ||
                     (files && (b.id.rfind("dir:", 0) == 0 ||
                                b.id.rfind("file:", 0) == 0 ||
                                b.id.rfind("play:", 0) == 0));
      bool toolbar = (contacts || files) && !contact;
      int x = toolbar ? (toolIndex % 2) * ((actionW - side - 16) / 2) : 0;
      int yy = toolbar ? (toolIndex++ / 2) * (files?52:64)
                       : row++ * (player ? 54 : 76);
      int width = toolbar ? (actionW - side - 16) / 2 : side;
      auto *item=toolbar && !b.icon.empty()
          ? tool(tools,b,x,yy,width-4,files?46:58)
          : button(toolbar ? tools : actions,b.id,b.label,x,yy,width-4,toolbar?(files?46:58):(player?50:72));
      if (files && toolbar && compactFont_) {
        lv_obj_set_style_text_font(item, compactFont_, 0);
        lv_obj_set_height(lv_obj_get_child(item, 0),
                          lv_font_get_line_height(compactFont_));
      }
      if (!firstAction || (meshNodes && b.outgoing))
        firstAction = item;
      if(meshNodes && b.outgoing)lv_obj_set_style_bg_color(item,lv_color_hex(0x304958),0);
      if ((!contacts && !files) || contact) {
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_set_style_pad_all(item, 0, 0);
        lv_obj_set_style_border_side(item, LV_BORDER_SIDE_BOTTOM, 0);
        auto *caption = lv_obj_get_child(item, 0);
        lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_width(caption, width - (b.icon.empty() ? 38 : 100));
        lv_obj_align(caption, LV_ALIGN_TOP_LEFT, b.icon.empty() ? 14 : 84,
                     b.subtitle.empty() ? 18 : 2);
        if (!b.icon.empty())
          icon(item, b.icon, 12, player ? 4 : 8, player ? 40 : 54);
        if(meshNodes && compactFont_)lv_obj_set_style_text_font(caption,compactFont_,0);
        if (!b.subtitle.empty()) {
          auto *sub = label(item, b.subtitle, b.icon.empty() ? 14 : 84, 36,
                            width - 100, 30);
          lv_obj_set_style_text_color(sub, lv_color_hex(0xbac6d0), 0);
          if(meshNodes && compactFont_)lv_obj_set_style_text_font(sub,compactFont_,0);
        }
      }
    }
    if ((contacts || files) && row == 0)
      label(actions, files ? translated("Pasta vazia","Empty folder","Carpeta vacía") : translated("Nenhum contato. Toque em Novo.","No contacts. Tap New.","Sin contactos. Toca Nuevo."),
            20, 24, side - 40, 80);
  }
  if (!player && !recorder) {
    int index = 0;
    for (const auto &b : v.tools) {
      if(browserDocument){tool(screen_,b,24+index*168,h-70,160,44);}
      else if(packages){tool(screen_,b,w-204,y+index*96,180,84);}
      else if(v.centerTools) {int width=168,gap=8,total=int(v.tools.size())*(width+gap)-gap;tool(screen_,b,(w-total)/2+index*(width+gap),h-(extraPagerRow?160:64),width,54);}
      else if (grid || gallery || calendar || clock || v.layout=="list" || v.layout=="reader" || v.layout=="chat") {
        tool(screen_, b, 24 + index * 120, h-150, 112, 42);
      } else
        tool(screen_, b, w * 3 / 5 + 8 + (index % 3) * 104,
             168 + (index / 3) * 84, 96, 76);
      ++index;
    }
  }
  if (!v.result.empty() && !calendar) {
    auto *result = pane(w * 3 / 5 + 20, 350, w * 2 / 5 - 48, 172);
    lv_obj_set_style_bg_color(result, lv_color_hex(0x354857), 0);
    lv_obj_set_style_radius(result, 8, 0);
    label(result, translated("Resultado","Result","Resultado"), 18, 10, w * 2 / 5 - 80, 34);
    auto *number = label(result, v.result, 18, 56, w * 2 / 5 - 80, 104);
    lv_obj_set_style_text_font(number, largeFont_, 0);
    lv_label_set_long_mode(number, LV_LABEL_LONG_WRAP);
  }
  if(!v.sidebar.empty()){
    for(size_t i=0;i<v.sidebar.size();++i){const auto &item=v.sidebar[i];auto *nav=button(screen_,item.id,"",12,168+int(i)*62,64,56);lv_obj_set_style_pad_all(nav,0,0);icon(nav,item.icon,16,12,32);if(item.subtitle=="selected")lv_obj_set_style_bg_color(nav,lv_color_hex(0x235f88),0);}
  }
  if (!edit_ && firstAction)
    lv_group_focus_obj(firstAction);
  if(packagePane_&&packageScroll_.count(packageKey_)){lv_obj_update_layout(packagePane_);lv_obj_scroll_to_y(packagePane_,packageScroll_[packageKey_],LV_ANIM_OFF);}
  message_ = label(screen_, notice_, 24, h - 103, w - 48, 32);
  lv_obj_set_style_text_color(message_, lv_color_hex(0xd8bf82), 0);
  std::vector<Button> footer=v.footer;
  if(!v.optionAction.empty()) footer.push_back({v.optionAction,v.optionLabel.empty()?"Ajustes":v.optionLabel,"","settings"});
  if(edit_ && !v.readonly) footer.push_back({"keyboard",v.keyboard,"","ui_keyboard"});
  if(v.layout!="home" && renderOwner_!="shell") footer.push_back({"back",v.right,"","ui_back"});
  if(renderOwner_=="shell" && v.layout!="home" && v.layout!="grid" && !v.centerTools) footer.push_back({"back",v.right,"","ui_back"});
  if(footer.size()>6) footer.resize(6);
  for(size_t i=0;i<footer.size();++i) {
    int width=std::min(220,(w-24-4*(int(footer.size())-1))/int(footer.size()));
    int x=footer[i].id=="back" ? w-width-12 : 12+int(i)*(width+4);
    auto *control=tool(screen_,footer[i],x,h-64,width,62);
    if(!jobControl_ || footer[i].id=="cancel") jobControl_=control;
  }
  if (edit_ && !v.readonly) {
    keyboard_ = lv_buttonmatrix_create(screen_);
    lv_obj_remove_flag(keyboard_, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_buttonmatrix_set_map(keyboard_, keyMap_.data());
    lv_obj_set_pos(keyboard_, 16, h - 398);
    lv_obj_set_size(keyboard_, w - 32, 320);
    lv_obj_set_style_bg_color(keyboard_, panel, 0);
    lv_obj_set_style_bg_color(keyboard_, lv_color_hex(0x4d535c), LV_PART_ITEMS);
    lv_obj_set_style_text_color(keyboard_, fg, LV_PART_ITEMS);
    lv_obj_add_event_cb(keyboard_, virtualKey, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);
  }
}
void UI::updateAnalog() {
 if(!analogHour_ || !analogMinute_)return;
 time_t now=runtime_.now();tm time{};localtime_r(&now,&time);
 double minute=double(time.tm_min)*6.0*3.141592653589793/180.0,hour=(double(time.tm_hour%12)*30.0+double(time.tm_min)/2.0)*3.141592653589793/180.0;
 hourPoints_[0]={54,54};hourPoints_[1]={lv_value_precise_t(54+std::sin(hour)*25),lv_value_precise_t(54-std::cos(hour)*25)};
 minutePoints_[0]={54,54};minutePoints_[1]={lv_value_precise_t(54+std::sin(minute)*36),lv_value_precise_t(54-std::cos(minute)*36)};
 lv_line_set_points(analogHour_,hourPoints_,2);lv_line_set_points(analogMinute_,minutePoints_,2);
}
void UI::updateWifi() {
 if(!wifiDot_)return;
 auto network=runtime_.network().status();int level=wifiLevel(network.connected,network.signalValid,network.signal);
 auto on=lv_color_hex(0xe9f4ff),off=lv_color_hex(0x697583);
 for(unsigned i=0;i<3;++i)if(wifiRings_[i])lv_obj_set_style_arc_color(wifiRings_[i],level>=int(i)+2?on:off,LV_PART_MAIN);
 lv_obj_set_style_bg_color(wifiDot_,level>=1?on:off,0);
 if(wifiCross_) {if(level<0)lv_obj_remove_flag(wifiCross_,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(wifiCross_,LV_OBJ_FLAG_HIDDEN);}
}
void UI::wakeScreen() {
  lv_display_trigger_activity(nullptr);
  if(sleepCover_) { lv_obj_delete_async(sleepCover_); sleepCover_=nullptr; }
  brightness_=runtime_.brightness;
  if(runtime_.screenWriter) runtime_.screenWriter(brightness_);
  keys_.reset();
}
void UI::poll() {
  if(recoveryCleaning_) {
    if(completed_.load(std::memory_order_acquire)){worker_.join();busy_=false;recoveryCleaning_=false;}
    return;
  }
  if(processing_) {if(busy_ && !backgroundJob_ && lv_tick_get()-foregroundStarted_>=150 && (lv_tick_get()/400)%2==0) lv_obj_remove_flag(processing_,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(processing_,LV_OBJ_FLAG_HIDDEN);}
  if(!busy_ && refreshed_ && pending_.empty() && !pointerPressed()) render();
  auto idle=lv_display_get_inactive_time(nullptr);
  int desired=screenBrightness(runtime_.screenMinutes.load(),idle)*runtime_.brightness.load()/100;
  if(runtime_.lock.locked() && runtime_.ready() && renderOwner_!="lock" && !busy_ && pending_.empty())
    enqueue("show_lock");
  if(runtime_.screenOffRequested.exchange(false)) {
    desired=0;
  } else if(sleepCover_) desired=0;
  if(desired!=brightness_) {
    brightness_=desired;
    if(desired==0 && !sleepCover_) {
      runtime_.lock.engage();
      sleepCover_=lv_obj_create(lv_layer_top());
      lv_obj_remove_style_all(sleepCover_);
      lv_obj_set_size(sleepCover_,LV_PCT(100),LV_PCT(100));
      lv_obj_set_style_bg_color(sleepCover_,lv_color_black(),0);
      lv_obj_set_style_bg_opa(sleepCover_,LV_OPA_COVER,0);
      lv_obj_add_flag(sleepCover_,LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(sleepCover_,[](lv_event_t *e) {
        static_cast<UI *>(lv_event_get_user_data(e))->wakeScreen();
      },LV_EVENT_RELEASED,this);
    }
    if(runtime_.screenWriter) runtime_.screenWriter(desired);
  }

  if (screen_ && lv_tick_get() - lastMedia_ >= 50) {
    lastMedia_ = lv_tick_get();
    if (mediaTime_) {
      auto info = runtime_.media().info();
      int seconds = int(info.seconds);
      char time[32];
      if (mediaFineTime_)
        snprintf(time, sizeof(time), "%02d:%02d.%d", seconds / 60, seconds % 60,
                 int(info.seconds * 10) % 10);
      else
        snprintf(time, sizeof(time), "%02d:%02d", seconds / 60, seconds % 60);
      lv_label_set_text(mediaTime_, time);
      if (mediaLevel_)
        lv_bar_set_value(mediaLevel_, info.level, LV_ANIM_OFF);
      if (mediaProgress_)
        lv_bar_set_value(mediaProgress_, info.progress, LV_ANIM_OFF);
      if (transportImage_) {
        auto image =
            icons_.find(info.state == "playing" ? "ui_pause" : "ui_play");
        if (image != icons_.end())
          lv_image_set_src(transportImage_, &image->second);
        auto *parent = lv_obj_get_parent(transportImage_);
        lv_label_set_text(lv_obj_get_child(parent, 0),
                          info.state == "playing" ? "Pausar" : "Tocar");
      }
      if (liveStatusLabel_)
        lv_label_set_text(liveStatusLabel_, runtime_.media().status().c_str());
    }
    if (imageWidth_ > 0 && liveImage_ && !mediaTime_) {
      auto picture = runtime_.media().picture();
      if (picture && picture != displayedPicture_)
        showPicture(picture, lv_obj_get_parent(liveImage_), imageWidth_,
                    imageHeight_);
      if (liveStatusLabel_)
        lv_label_set_text(liveStatusLabel_,
                          runtime_.media().cameraStatus().c_str());
    }
  }

  if (status_ && lv_tick_get() - lastClock_ >= 1000) {
    lastClock_ = lv_tick_get();
    updateWifi();updateAnalog();
    lv_label_set_text(status_, statusClock(runtime_).c_str());
    if(homeDate_) lv_label_set_text(homeDate_,runtime_.clockText().substr(0,10).c_str());
    if(digitalClock_) {auto clock=runtime_.clockText();lv_label_set_text(digitalClock_,clock.size()>20?clock.substr(12,runtime_.view().layout=="home"?5:8).c_str():"--:--:--");}
    if (battery_) {
      auto p = runtime_.powerInfo();
      std::string text =
          p.available ? std::to_string(p.percent) + (p.charging ? "%+" : "%")
                      : "--%";
      lv_label_set_text(battery_, text.c_str());
      if (batteryGauge_)
        lv_bar_set_value(batteryGauge_, p.available ? p.percent : 0,
                         LV_ANIM_OFF);
      lv_obj_set_style_text_color(
          battery_,
          lv_color_hex(p.available && p.percent <= 15 ? 0xff7373 : 0xeeeeee),
          0);
    }
  }
  bool requestedSound = false, alarmSound = false;
  for (auto &alarm : runtime_.pollTimers()) {
    notice_ = alarm.text;
    if (message_)
      lv_label_set_text(message_, notice_.c_str());
    requestedSound = requestedSound || alarm.sound;
    alarmSound = alarmSound || (alarm.alarm && alarm.sound);
  }
  if (requestedSound && tones_.size() < 4)
    tones_.push_back(alarmSound);
  if (tone_.joinable() && toneDone_.load(std::memory_order_acquire)) {
    tone_.join();
    if (!toneError_.empty()) {
      notice_ = "Falha de áudio: " + toneError_;
      if (message_)
        lv_label_set_text(message_, notice_.c_str());
    }
  }
  if (!tone_.joinable() && !tones_.empty()) {
    bool alarm = tones_.front();
    tones_.pop_front();
    toneDone_ = false;
    if (!tone_.submit([this, alarm] {
          runtime_.emitSignal(alarm);
          toneError_.clear();
          if (runtime_.media().device.diagnostic) {
            auto result = runtime_.audioResult();
            if (result != "ok")
              toneError_ = result;
          }
          toneDone_.store(true, std::memory_order_release);
        })) {
      toneDone_ = true;
      notice_ = "Falha de áudio: executor indisponível";
      if (message_)
        lv_label_set_text(message_, notice_.c_str());
    }
  }

  if (busy_) {
    if (!completed_.load(std::memory_order_acquire))
      return;
    worker_.join();
    busy_ = false;
    if (cardRemoved_) {
      cardRemoved_ = false;
      runtime_.storage().invalidate();
      recovery("Cartão removido. Reinsira e pressione Enter.");
      return;
    }
    if (retryJob_) {
      retryJob_ = false;
      if (result_)
        start();
      else
        recovery(runtime_.error);
      return;
    }
    if (backgroundJob_) {
      backgroundJob_ = false;
      auto *focused = lv_group_get_focused(group_);
      while (!deferredInput_.empty()) {
        auto queued = deferredInput_.front();
        deferredInput_.pop_front();
        if (queued.focus && lv_obj_is_valid(queued.focus))
          lv_group_focus_obj(queued.focus);
        input(queued.event);
      }
      if (focused && lv_obj_is_valid(focused))
        lv_group_focus_obj(focused);

      if(refreshed_ && statusFieldsOnly_ && statusWidgets_.size()==runtime_.view().statusFields.size() && renderOwner_==runtime_.current()) {
        for(size_t i=0;i<statusWidgets_.size();++i){auto &widget=statusWidgets_[i];const auto &field=runtime_.view().statusFields[i];
          if(widget.last!=field.value){lv_label_set_text(widget.value,field.value.c_str());widget.last=field.value;}
          if(widget.bar)lv_bar_set_value(widget.bar,field.percent,LV_ANIM_OFF);
        }
        refreshed_=false;
      }
      statusFieldsOnly_=false;
      if(refreshed_ && readerTextOnly_ && diagnosticPane_ && diagnosticLabel_ &&
         runtime_.view().layout=="diagnostic" && renderOwner_==runtime_.current()) {
        int scroll=lv_obj_get_scroll_y(diagnosticPane_);
        lv_obj_invalidate(diagnosticPane_);
        lv_label_set_text(diagnosticLabel_,runtime_.view().text.c_str());
        lv_obj_update_layout(diagnosticPane_);lv_obj_scroll_to_y(diagnosticPane_,scroll,LV_ANIM_OFF);
        lv_obj_invalidate(diagnosticPane_);
        refreshed_=false;
      }
      if(refreshed_ && readerTextOnly_ && edit_ && runtime_.view().readonly &&
         runtime_.view().layout=="reader" && renderOwner_==runtime_.current()) {
        // Counters must not destroy/recreate a long diagnostic textarea.
        int scroll=lv_obj_get_scroll_y(edit_);int cursor=lv_textarea_get_cursor_pos(edit_);
        setEditorText(edit_,runtime_.view().text.c_str());
        lv_textarea_set_cursor_pos(edit_,cursor);
        lv_obj_update_layout(edit_);lv_obj_scroll_to_y(edit_,scroll,LV_ANIM_OFF);
        refreshed_=false;
      }
      readerTextOnly_=false;
      if(refreshed_ && pending_.empty() && !pointerPressed()) {
        // A received packet must not discard characters entered while the
        // services worker was running, nor close the touch keyboard.
        bool composer=runtime_.view().layout=="chat" && formWidgets_.size()==3 && edit_;
        auto live=composer?splitLines(text()):std::vector<std::string>{};
        int cursor=composer?lv_textarea_get_cursor_pos(edit_):0;
        bool keyboardShown=composer && keyboard_ && !lv_obj_has_flag(keyboard_,LV_OBJ_FLAG_HIDDEN);
        render();
        if(composer && formWidgets_.size()==3 && edit_ && live.size()>=3) {
          for(size_t i=0;i<2;++i) {auto *widget=formWidgets_[i];auto &keys=choices_[widget];auto found=std::find(keys.begin(),keys.end(),live[i]);if(found!=keys.end())lv_dropdown_set_selected(widget,found-keys.begin());}
          setEditorText(edit_,live[2].c_str());lv_textarea_set_cursor_pos(edit_,cursor);
          if(keyboardShown)showKeyboard();
        }
      }
      if (liveStatusLabel_ && mediaTime_)
        lv_label_set_text(liveStatusLabel_, mediaStatusText_.c_str());
      if (!result_) {
        if (message_)
          lv_label_set_text(message_, runtime_.error.c_str());
        return;
      }
      if (runtime_.notices().empty()) {
        notice_.clear();
        if (message_)
          lv_label_set_text(message_, "");
      }
      if (!runtime_.notices().empty()) {
        notice_ = runtime_.notices().back().text;
        if (message_)
          lv_label_set_text(message_, notice_.c_str());
      }
      return;
    }
    if (!result_) {
      if (!runtime_.storage().online()) {
        recovery(runtime_.storage().lastError());
      } else {
        render();
        if (message_)
          lv_label_set_text(message_, runtime_.error.c_str());
      }
      return;
    }
    render();
    return;
  }
  if (cardRemoved_) {
    cardRemoved_ = false;
    runtime_.storage().invalidate();
    recovery("Cartão removido. Reinsira e pressione Enter.");
    return;
  }
  for (auto &e : keys_.tick(lv_tick_get()))
    input(e);
  if (pending_.empty()) {
    if (runtime_.ready() && lv_tick_get() - lastTick_ >= 1000) {
      lastTick_ = lv_tick_get();
      backgroundJob_ = true;
      busy_ = true;
      runtime_.beginOperation();
      completed_ = false;
      bool pointerDown=pointerPressed();
      bool refresh=runtime_.view().refreshMs && !pointerDown && !sleepCover_ && lv_tick_get()-lastRefresh_>=runtime_.view().refreshMs;
      if(refresh) lastRefresh_=lv_tick_get();
      auto value=text();
      if (!worker_.submit([this,refresh,value] {
            result_ = runtime_.tick();
            if(result_ && refresh) {
              auto previous=runtime_.view();result_=runtime_.dispatch("refresh",value);
              refreshed_=result_ && !sameLiveView(previous,runtime_.view());
              readerTextOnly_=false;statusFieldsOnly_=false;
              if(refreshed_ && previous.layout=="status" && !previous.statusFields.empty() && previous.statusFields.size()==runtime_.view().statusFields.size()) {
                auto candidate=previous;
                for(size_t i=0;i<candidate.statusFields.size();++i){candidate.statusFields[i].value=runtime_.view().statusFields[i].value;candidate.statusFields[i].percent=runtime_.view().statusFields[i].percent;}
                statusFieldsOnly_=sameLiveView(candidate,runtime_.view());
              }
              if(refreshed_ && (previous.layout=="reader" || previous.layout=="diagnostic") && previous.readonly &&
                 runtime_.view().layout==previous.layout && runtime_.view().readonly) {
                previous.text=runtime_.view().text;
                readerTextOnly_=sameLiveView(previous,runtime_.view());
              }
            }
            if (liveStatusLabel_)
              mediaStatusText_ = runtime_.media().status();
            completed_.store(true, std::memory_order_release);
          })) {
        result_ = false;
        runtime_.error = "Memória insuficiente para executor";
        completed_ = true;
      }
    }
    return;
  }
  auto action = pending_.front();
  pending_.pop_front();
  if (action.key != "retry" && !action.owner.empty() &&
      action.owner != runtime_.current())
    return;
  if(runtime_.lock.locked() && action.key!="show_lock" && runtime_.current()!="lock") {
    action.key="show_lock";
  }
  if(runtime_.lock.locked() && runtime_.current()=="lock" &&
     (action.key=="home" || action.key=="menu" || action.key=="switcher" || action.key=="notifications"))
    action.key="refresh";
  if(!action.explicitValue) action.value = text();
  if(action.key=="back" || action.key=="cancel" || action.key=="home" || action.key=="menu" || action.key=="switcher") {
    for(auto &entry:sliders_) {
      if(entry.second.kind=="brightness") runtime_.brightness=entry.second.initial;
      if(entry.second.kind=="volume") runtime_.media().setVolume(entry.second.initial);
    }
  }
  if (action.key == "cancel_job") {
    runtime_.cancel();
    return;
  }
  if (action.key == "keyboard") {
    showKeyboard();
    return;
  }
  retryJob_ = action.key == "retry" || action.key == "restore_release";
  if (!retryJob_ && !runtime_.ready())
    return;
  backgroundJob_ = false;
  foregroundStarted_=lv_tick_get();
  runtime_.beginOperation();
  busy_ = true;
  completed_.store(false, std::memory_order_relaxed);
  if (!worker_.submit([this, action] {
        bool pinEntry=action.owner=="lock" && (action.key.rfind("digit:",0)==0 || action.key=="erase");
        if(!pinEntry)traceAction(runtime_, action.key, "begin");
        if (retryJob_) {
          if(action.key=="restore_release") Releases(runtime_.storage()).restore();
          result_ = runtime_.boot();
        }
        else if(action.key=="show_lock") result_=(runtime_.faulted() || runtime_.dispatch("suspend",action.value)) && runtime_.open("lock");
        else if (action.key == "menu" || action.key == "switcher" ||
                 action.key == "notifications")
          result_ = (runtime_.faulted() ||
                     runtime_.dispatch("suspend", action.value)) &&
                    runtime_.open("shell") &&
                    runtime_.dispatch(
                        action.key == "notifications" ? "notices" : action.key);
        else
          result_ = runtime_.dispatch(action.key, action.value);
        if (result_ && !retryJob_ && !pinEntry)
          runtime_.tick();
        if(!pinEntry)traceAction(runtime_, action.key,
                    result_ ? "lua-complete" : "lua-error");
        completed_.store(true, std::memory_order_release);
      })) {
    result_ = false;
    runtime_.error = "Memória insuficiente para executor";
    completed_ = true;
  }
}
void UI::input(const Input &e) {
  if(recoveryCleaning_)return;
  if(e.down && e.key!="card_removed" && e.key!="card_inserted" && e.key!="reset") {
    lv_display_trigger_activity(nullptr);
    if(sleepCover_) { wakeScreen(); return; }
  }

  if (e.key.rfind("matrix:", 0) == 0) {
    unsigned k = unsigned(strtoul(e.key.c_str() + 7, nullptr, 10));
    for (auto &decoded : keys_.matrix(k / 14, k % 14, e.down, lv_tick_get()))
      input(decoded);
    return;
  }
  if (e.key == "card_removed") {
    runtime_.cancel();
    cardRemoved_ = true;
    return;
  }
  if (e.key == "card_inserted" || e.key == "reset") {
    keys_.reset();
    all_ = false;
    return;
  }
  if(renderOwner_=="lock" && runtime_.ready() && !cardRemoved_) {
    if(!e.down || e.repeat)return;
    auto digits=!e.text.empty()?e.text:e.key;
    if(!digits.empty() && digits.size()<=4 && digits.find_first_not_of("0123456789")==std::string::npos)for(char digit:digits)enqueue(std::string("digit:")+digit);
    else if(e.key=="backspace" || e.key=="delete")enqueue("erase");
    else if(e.key=="enter")enqueue("unlock");
    return;
  }
  if (busy_ && backgroundJob_) {
    if (deferredInput_.size() < 96)
      deferredInput_.push_back(
          {e, group_ ? lv_group_get_focused(group_) : nullptr});
    return;
  }
  if (busy_) {
    if (e.down && e.key == "escape")
      runtime_.cancel();
    return;
  }
  if (cardRemoved_)
    return;
  if (!e.down)
    return;
  if (!runtime_.ready()) {
    if (e.key == "enter" || e.key == "escape")
      enqueue("retry");
    return;
  }
  if(runtime_.lock.locked()) {
    if(e.key.size()==1 && e.key[0]>='0' && e.key[0]<='9') enqueue("digit:"+e.key);
    else if(e.text.size()==1 && e.text[0]>='0' && e.text[0]<='9') enqueue("digit:"+e.text);
    else if(e.key=="backspace") enqueue("erase");
    else if(e.key=="enter") enqueue("unlock");
    return;
  }
  if (e.ctrl && e.key == "s") {
    enqueue("save");
    return;
  }
  if (e.ctrl && e.key == "escape") {
    enqueue("menu");
    return;
  }
  if (e.ctrl && e.key == "tab") {
    enqueue("switcher");
    return;
  }
  if (e.key == "escape") {
    enqueue("back");
    return;
  }
  if (e.key == "tab") {
    if (e.shift)
      lv_group_focus_prev(group_);
    else
      lv_group_focus_next(group_);
    return;
  }
  auto *focusedObj = lv_group_get_focused(group_);
  if(focusedObj && sliders_.count(focusedObj)) {
    if(e.key=="left" || e.key=="right" || e.key=="up" || e.key=="down") {
      int delta=e.key=="left"?-1:e.key=="right"?1:e.key=="up"?10:-10;
      lv_slider_set_value(focusedObj,lv_slider_get_value(focusedObj)+delta,LV_ANIM_OFF);lv_obj_send_event(focusedObj,LV_EVENT_VALUE_CHANGED,nullptr);return;
    }
    if(e.key=="enter") {lv_obj_send_event(focusedObj,LV_EVENT_RELEASED,nullptr);return;}
  }
  if (focusedObj && choices_.count(focusedObj)) {
    if (e.key == "up" || e.key == "down") {
      auto selected = lv_dropdown_get_selected(focusedObj);
      auto count = choices_[focusedObj].size();
      if (count)
        lv_dropdown_set_selected(
            focusedObj, e.key == "down" ? (selected + 1) % count
                                        : (selected + count - 1) % count);
      return;
    }
    if (e.key == "enter") {
      if (lv_dropdown_is_open(focusedObj))
        lv_dropdown_close(focusedObj);
      else
        lv_dropdown_open(focusedObj);
      return;
    }
  }
  if(diagnosticPane_) {
    if(e.key=="down" || e.key=="up" || e.key=="pagedown" || e.key=="pageup") {
      int step=(e.key=="pagedown" || e.key=="pageup")?240:40;
      lv_obj_scroll_by(diagnosticPane_,0,(e.key=="down" || e.key=="pagedown")?-step:step,LV_ANIM_OFF);return;
    }
  }
  if(packagePane_&&!packageRows_.empty()){
    auto found=std::find(packageRows_.begin(),packageRows_.end(),focusedObj);
    if(found!=packageRows_.end()&&(e.key=="up"||e.key=="down"||e.key=="pageup"||e.key=="pagedown"||e.key=="home"||e.key=="end")){
      int at=int(found-packageRows_.begin()),last=int(packageRows_.size())-1;
      int scroll=lv_obj_get_scroll_y(packagePane_);if(at*84+76<scroll||at*84>scroll+lv_obj_get_height(packagePane_))at=std::max(0,std::min(last,scroll/84));
      int step=(e.key=="pagedown"||e.key=="pageup")?std::max(1,int(lv_obj_get_height(packagePane_))/84):1;
      at=e.key=="home"?0:e.key=="end"?last:std::max(0,std::min(last,at+((e.key=="up"||e.key=="pageup")?-step:step)));
      lv_group_focus_obj(packageRows_[at]);lv_obj_scroll_to_view(packageRows_[at],LV_ANIM_OFF);return;
    }
  }
  bool focused = edit_ && lv_group_get_focused(group_) == edit_;
  if (focused && runtime_.view().readonly) {
    if (e.key == "down" || e.key == "up")
      lv_obj_scroll_by(edit_, 0, e.key == "down" ? -36 : 36, LV_ANIM_OFF);
    return;
  }
  if (focused && runtime_.view().layout == "calculator" && e.key == "enter") {
    enqueue("calculate");
    return;
  }
  if(focused && e.key=="enter" && !runtime_.view().submitAction.empty()) {enqueue(runtime_.view().submitAction);return;}
  if (focused && !formWidgets_.empty() && e.key == "enter") {
    lv_group_focus_next(group_);
    return;
  }
  if (focused) {
    if (e.ctrl) {
      if (e.key == "s") {
        enqueue("save");
        return;
      }
      if (e.key == "a") {
        all_ = true;
        anchor_ = 0;
        std::string s = lv_textarea_get_text(edit_);
        int count = 0;
        for (unsigned char c : s)
          if ((c & 0xc0) != 0x80)
            ++count;
        lv_textarea_set_cursor_pos(edit_, count);
        selection(0, count);
        return;
      }
      if (e.key == "c" || e.key == "x") {
        auto selected = selectedText();
        if (!selected.empty())
          clipboard_ = selected;
        if (e.key == "x")
          eraseSelection();
        return;
      }
      if (e.key == "v") {
        eraseSelection();
        lv_textarea_add_text(edit_, clipboard_.c_str());
        return;
      }
      return;
    }
    if (e.key == "left" || e.key == "right" || e.key == "up" ||
        e.key == "down") {
      int before = lv_textarea_get_cursor_pos(edit_);
      if (e.shift && anchor_ < 0)
        anchor_ = before;
      if (e.key == "left")
        lv_textarea_cursor_left(edit_);
      else if (e.key == "right")
        lv_textarea_cursor_right(edit_);
      else if (e.key == "up")
        lv_textarea_cursor_up(edit_);
      else
        lv_textarea_cursor_down(edit_);
      if (e.shift)
        selection(anchor_, lv_textarea_get_cursor_pos(edit_));
      else {
        anchor_ = -1;
        lv_textarea_clear_selection(edit_);
      }
      return;
    }
    bool hadSelection = !selectedText().empty();
    if (!e.text.empty() || e.key == "enter" || e.key == "backspace" ||
        e.key == "delete")
      eraseSelection();
    if (e.key == "backspace") {
      if (!hadSelection) {
        auto mask = masks_.find(edit_);
        int cursor = lv_textarea_get_cursor_pos(edit_);
        if (mask != masks_.end() && cursor > 0 &&
            size_t(cursor) <= mask->second.size() &&
            mask->second[cursor - 1] != '0')
          lv_textarea_set_cursor_pos(edit_, cursor - 1);
        lv_textarea_delete_char(edit_);
      }
      return;
    }
    if (e.key == "delete") {
      if (!hadSelection)
        lv_textarea_delete_char_forward(edit_);
      return;
    }
    if (e.key == "enter") {
      lv_textarea_add_text(edit_, "\n");
      return;
    }
    if (!e.text.empty())
      lv_textarea_add_text(edit_, e.text.c_str());
  } else {
    if (e.key == "down" || e.key == "right")
      lv_group_focus_next(group_);
    else if (e.key == "up" || e.key == "left")
      lv_group_focus_prev(group_);
    else if (e.key == "enter") {
      auto *o = lv_group_get_focused(group_);
      if (o) {
        selected_.clear();
        auto idx = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(o));
        if (idx < ids_.size())
          enqueue(ids_[idx]);
      }
    }
  }
}
} // namespace tab5
