// SPDX-License-Identifier: MIT
#include "ui.hpp"
#include "src/misc/cache/instance/lv_image_cache.h"
#include "src/misc/cache/instance/lv_image_header_cache.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sstream>
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
      "runtime=0.6.0\naction=" + action + "\nstage=" + stage +
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
      } else
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
    lv_textarea_set_text(edit_, s.c_str());
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
void UI::clicked(lv_event_t *e) {
  auto *self = static_cast<UI *>(lv_event_get_user_data(e));
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
        (self->ids_[index] == "options" || self->ids_[index] == "cancel_job"))
      self->runtime_.cancel();
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
    lv_textarea_set_text(obj, formatted.c_str());
    lv_textarea_set_cursor_pos(obj, restored);
    self->formatting_ = false;
  }
}
void UI::restoreKeyboardField() {
  if (keyboardEdit_ && lv_obj_is_valid(keyboardEdit_)) {
    lv_obj_set_y(keyboardEdit_, keyboardEditY_);
    lv_obj_set_height(keyboardEdit_, keyboardEditH_);
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
  lv_obj_set_style_bg_grad_color(b, lv_color_hex(0x252c34), 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(0x5289b0), LV_STATE_FOCUSED);
  lv_obj_set_style_outline_width(b, 1, LV_STATE_FOCUSED);
  lv_obj_set_style_outline_color(b, lv_color_hex(0x8cc4e8), LV_STATE_FOCUSED);
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
  std::istringstream lines(layout);
  std::string line;
  while (std::getline(lines, line))
    keyLabels_.push_back(line == "ROW" ? "\n" : line);
  for (auto &label : keyLabels_)
    keyMap_.push_back(label.c_str());
  keyMap_.push_back("");
  font_ = lv_tiny_ttf_create_data_ex(fontData_.data(), fontData_.size(), 26,
                                     LV_FONT_KERNING_NONE, 96);
  if (!font_) {
    recovery("Falha ao carregar fonte externa");
    return false;
  }
  largeFont_ = lv_tiny_ttf_create_data_ex(fontData_.data(), fontData_.size(),
                                          48, LV_FONT_KERNING_NONE, 48);
  compactFont_ = lv_tiny_ttf_create_data_ex(fontData_.data(), fontData_.size(),
                                            20, LV_FONT_KERNING_NONE, 48);
  icons_.clear();
  if (runtime_.asset("icons.bin", iconData_) && iconData_.size() >= 8 &&
      iconData_.compare(0, 4, "T5IC") == 0) {
    auto byte = [this](size_t i) {
      return static_cast<unsigned char>(iconData_[i]);
    };
    unsigned side = byte(4) | (byte(5) << 8), count = byte(6) | (byte(7) << 8);
    if (side == 96 && count <= 64 &&
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
  runtime_.shutdown();
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
  edit_ = keyboard_ = message_ = jobControl_ = nullptr;
  battery_ = batteryGauge_ = nullptr;
  status_ = liveStatusLabel_ = liveImage_ = mediaTime_ = mediaLevel_ =
      mediaProgress_ = transportImage_ = nullptr;
  thumbnailDescriptors_.clear();
  thumbnailPictures_.clear();
  imageWidth_ = imageHeight_ = 0;
  ids_.clear();
  keyboardEdit_ = nullptr;
  editors_.clear();
  formWidgets_.clear();
  choices_.clear();
  masks_.clear();
  pending_.clear();
  if (group_)
    lv_group_delete(group_);
  group_ = lv_group_create();
  auto *l = lv_label_create(screen_);
  lv_obj_set_width(l, 650);
  lv_obj_set_pos(l, 30, 60);
  std::string msg =
      "Tab5Symbian runtime 0.6.0 / API 5\nRecuperacao\n\n" + why +
      "\n\nPrepare /SymbianOS no microSD FAT32.\nRestaure os arquivos e "
      "reinicie.\nNenhum cartao sera formatado.\nEnter: tentar novamente";
  lv_label_set_text(l, msg.c_str());
  lv_obj_set_style_bg_color(screen_, lv_color_hex(0x292b2f), 0);
  lv_obj_set_style_text_color(screen_, lv_color_hex(0xeeeeee), 0);
  button(screen_, "retry", "Tentar novamente", 30, 420, 300, 70);
  lv_screen_load(screen_);
  if (old)
    lv_obj_delete(old);
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
  lv_image_set_scale(image, size * 256 / 96);
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
void UI::render() {
  keys_.reset();
  if (!runtime_.ready()) {
    recovery(runtime_.error);
    return;
  }
  const auto &v = runtime_.view();
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
  edit_ = keyboard_ = message_ = jobControl_ = nullptr;
  battery_ = batteryGauge_ = nullptr;
  status_ = liveStatusLabel_ = liveImage_ = mediaTime_ = mediaLevel_ =
      mediaProgress_ = transportImage_ = nullptr;
  thumbnailDescriptors_.clear();
  thumbnailPictures_.clear();
  imageWidth_ = imageHeight_ = 0;
  keyboardEdit_ = nullptr;
  editors_.clear();
  formWidgets_.clear();
  choices_.clear();
  masks_.clear();
  ids_.clear();
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
  auto label = [&](lv_obj_t *p, const std::string &s, int x, int y, int width,
                   int height) {
    auto *o = lv_label_create(p);
    lv_label_set_text(o, s.c_str());
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, width, height);
    lv_label_set_long_mode(o, LV_LABEL_LONG_DOT);
    return o;
  };
  auto pane = [&](int x, int y, int width, int height) {
    auto *o = lv_obj_create(screen_);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, width, height);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_bg_color(o, bg, 0);
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
      if (height <= 48)
        lv_label_set_text(caption, "");
      icon(o, b.icon, (width - size) / 2, height <= 48 ? 1 : 0, size);
      if (b.id == "toggle")
        transportImage_ = lv_obj_get_child(o, lv_obj_get_child_count(o) - 1);
    }
    return o;
  };
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
    showPicture(picture, viewport, w, h);
    button(screen_, "back", "Voltar", 16, 16, 138, 48);
    button(screen_, "home", "Início", 168, 16, 118, 48);
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
  lv_obj_set_style_bg_color(top, lv_color_hex(0x505b68), 0);
  lv_obj_set_style_bg_grad_color(top, lv_color_hex(0x232b35), 0);
  lv_obj_set_style_bg_grad_dir(top, LV_GRAD_DIR_VER, 0);
  label(top, "SD • Pessoal", 22, 5, 350, 32);
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
            w - 491, 5, 100, 32);
  lv_bar_set_value(batteryGauge_,
                   initialPower.available ? initialPower.percent : 0,
                   LV_ANIM_OFF);
  status_ = label(top, statusClock(runtime_), w - 390, 5, 366, 32);
  lv_obj_set_style_text_align(status_, LV_TEXT_ALIGN_RIGHT, 0);
  icon(top, renderOwner_ == "shell" ? "menu" : renderOwner_, 20, 48, 48);
  label(top, v.title, 84, 52, w - 560, 44);
  button(top, "home", v.home, w - 430, 48, 128, 48);
  button(top, "menu", v.menu, w - 292, 48, 128, 48);
  button(top, "switcher", v.switcher, w - 154, 48, 132, 48);
  auto *hint = label(screen_, v.body, 24, 122, w - 48, 34);
  liveStatusLabel_ = v.mediaStatus ? hint : nullptr;
  lv_obj_set_style_text_color(hint, lv_color_hex(0xb7c4d1), 0);
  const bool calculator = v.layout == "calculator",
             contacts = v.layout == "contacts", files = v.layout == "files",
             imageView = v.layout == "image" || v.layout == "image_zoom";
  mediaFineTime_ = v.layout == "recorder";
  const bool player = v.layout == "player", recorder = v.layout == "recorder",
             gallery = v.layout == "gallery";
  const bool grid = v.layout == "grid" || v.layout == "home";
  const bool form = v.layout == "form";
  int y = 168, contentH = h - 280;
  auto editor = [&](const std::string &value, int x, int yy, int width,
                    int height, bool one) {
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
    lv_textarea_set_text(o, value.c_str());
    lv_textarea_set_text_selection(o, true);
    lv_obj_add_event_cb(o, changed, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(o, changed, LV_EVENT_FOCUSED, this);
    lv_group_add_obj(group_, o);
    editors_.push_back(o);
    return o;
  };
  int actionX = 18, actionW = v.layout == "notebook" ? w * 3 / 5 - 36 : w - 36,
      actionY = y, actionH = contentH;
  if (v.field) {
    if (form) {
      auto controls = v.controls;
      if (controls.empty())
        for (const auto &name : splitLines(v.fields))
          controls.push_back({name, "text", "", ""});
      auto values = splitLines(v.text);
      int step = std::min(116, contentH / std::max(1, int(controls.size())));
      for (size_t n = 0; n < controls.size(); ++n) {
        const auto &c = controls[n];
        const auto value = n < values.size() ? values[n] : "";
        label(screen_, c.label, 26, y + int(n) * step, w * 3 / 5 - 52, 32);
        lv_obj_t *widget;
        if (c.type == "select") {
          widget = lv_dropdown_create(screen_);
          lv_obj_set_pos(widget, 24, y + 32 + int(n) * step);
          lv_obj_set_size(widget, w * 3 / 5 - 48, 58);
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
          widget = editor(value, 24, y + 32 + int(n) * step, w * 3 / 5 - 48, 58,
                          true);
          if (c.type == "password")
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
        formWidgets_.push_back(widget);
      }
      editHeight_ = 58;
      actionX = w * 3 / 5;
      actionW = w - actionX - 20;
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
      edit_ = editor(v.text, 24, y, w * 3 / 5 - 44, contentH, false);
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
      auto *viewport = pane(18, y, w * 3 / 4 - 36, contentH);
      lv_obj_set_scroll_dir(viewport, LV_DIR_ALL);
      auto *photo = lv_image_create(viewport);
      lv_image_set_src(photo, &pictureDescriptor_);
      lv_obj_set_pos(photo, 0, 0);
      lv_image_set_pivot(photo, 0, 0);
      double scale = std::min(double(w * 3 / 4 - 48) / displayedPicture_->width,
                              double(contentH) / displayedPicture_->height);
      lv_image_set_scale(photo, v.layout == "image_zoom"
                                    ? 256
                                    : std::max(1, int(scale * 256)));
    }
    actionX = w * 3 / 4;
    actionW = w - actionX - 20;
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
    int index = 0;
    for (const auto &b : v.tools) {
      tool(screen_, b, 34 + (index % 8) * 90, 552 + (index / 8) * 76, 80, 72);
      ++index;
    }
    actionX = 440;
    actionW = w - actionX - 24;
    actionH = 330;
  }
  auto *actions = pane(actionX, actionY, actionW, actionH);
  lv_obj_t *firstAction = nullptr;
  if (gallery) {
    thumbnailDescriptors_.resize(v.buttons.size());
    thumbnailPictures_.resize(v.buttons.size());
    int bw = actionW / 4, bh = 206;
    for (size_t i = 0; i < v.buttons.size(); ++i) {
      const auto &b = v.buttons[i];
      auto *tile = button(actions, b.id, b.label, (i % 4) * bw, (i / 4) * bh,
                          bw - 12, bh - 12);
      auto *caption = lv_obj_get_child(tile, 0);
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
  } else if (grid) {
    int cols = v.layout == "home" ? 4 : 3, bw = (actionW - 24) / cols;
    int rowH = v.layout == "home" ? 214 : 146;
    for (size_t i = 0; i < v.buttons.size(); ++i) {
      const auto &b = v.buttons[i];
      int x = int(i % cols) * bw + 8, yy = int(i / cols) * rowH;
      auto *tile = button(actions, b.id, b.label, x, yy, bw - 14, rowH - 10);
      if (!firstAction)
        firstAction = tile;
      lv_obj_set_style_bg_opa(tile, LV_OPA_TRANSP, 0);
      lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, LV_STATE_FOCUSED);
      lv_obj_set_style_border_opa(tile, LV_OPA_TRANSP, 0);
      auto *caption = lv_obj_get_child(tile, 0);
      lv_obj_align(caption, LV_ALIGN_BOTTOM_MID, 0, -4);
      icon(tile, b.icon.empty() ? b.id.substr(b.id.find(':') + 1) : b.icon,
           (bw - 14 - 80) / 2, 8, 80);
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
  } else {
    int row = 0, tool = 0;
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
      int x = files && toolbar ? (tool % 2) * ((actionW - side - 16) / 2) : 0;
      int yy = toolbar ? (files ? (tool++ / 2) * 52 : tool++ * 60)
                       : row++ * (player ? 54 : 76);
      int width = toolbar ? (actionW - side - 16) / (files ? 2 : 1) : side;
      auto *item =
          button(toolbar ? tools : actions, b.id, b.label, x, yy, width - 4,
                 toolbar ? (files ? 46 : 58) : (player ? 50 : 72));
      if (files && toolbar && compactFont_) {
        lv_obj_set_style_text_font(item, compactFont_, 0);
        lv_obj_set_height(lv_obj_get_child(item, 0),
                          lv_font_get_line_height(compactFont_));
      }
      if (!firstAction)
        firstAction = item;
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
        if (!b.subtitle.empty()) {
          auto *sub = label(item, b.subtitle, b.icon.empty() ? 14 : 84, 36,
                            width - 100, 30);
          lv_obj_set_style_text_color(sub, lv_color_hex(0xbac6d0), 0);
        }
      }
    }
    if ((contacts || files) && row == 0)
      label(actions, files ? "Pasta vazia" : "Nenhum contato. Toque em Novo.",
            20, 24, side - 40, 80);
  }
  if (!player && !recorder) {
    int index = 0;
    for (const auto &b : v.tools) {
      if (grid || gallery) {
        tool(screen_, b, w - 220 + index * 102, 116, 92, 42);
      } else
        tool(screen_, b, w * 3 / 5 + 8 + (index % 3) * 104,
             168 + (index / 3) * 84, 96, 76);
      ++index;
    }
  }
  if (!v.result.empty()) {
    auto *result = pane(w * 3 / 5 + 20, 350, w * 2 / 5 - 48, 172);
    lv_obj_set_style_bg_color(result, lv_color_hex(0x354857), 0);
    lv_obj_set_style_radius(result, 8, 0);
    label(result, "Resultado", 18, 10, w * 2 / 5 - 80, 34);
    auto *number = label(result, v.result, 18, 56, w * 2 / 5 - 80, 104);
    lv_obj_set_style_text_font(number, largeFont_, 0);
    lv_label_set_long_mode(number, LV_LABEL_LONG_WRAP);
  }
  if (!edit_ && firstAction)
    lv_group_focus_obj(firstAction);
  message_ = label(screen_, notice_, 24, h - 103, w - 48, 32);
  lv_obj_set_style_text_color(message_, lv_color_hex(0xd8bf82), 0);
  jobControl_ = button(screen_, "options", v.left, 0, h - 64, w / 3, 64);
  button(screen_, edit_ && !v.readonly ? "keyboard" : "notifications",
         edit_ && !v.readonly ? v.keyboard : "Avisos", w / 3, h - 64, w / 3,
         64);
  button(screen_, "back", v.right, 2 * w / 3, h - 64, w - 2 * w / 3, 64);
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
void UI::poll() {
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
    lv_label_set_text(status_, statusClock(runtime_).c_str());
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
      runtime_.shutdown();
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
        runtime_.shutdown();
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
    runtime_.shutdown();
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
      if (!worker_.submit([this] {
            result_ = runtime_.tick();
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
  action.value = text();
  if (action.key == "cancel_job") {
    runtime_.cancel();
    return;
  }
  if (action.key == "keyboard") {
    showKeyboard();
    return;
  }
  retryJob_ = action.key == "retry";
  if (!retryJob_ && !runtime_.ready())
    return;
  backgroundJob_ = false;
  runtime_.beginOperation();
  busy_ = true;
  completed_.store(false, std::memory_order_relaxed);
  if (message_)
    lv_label_set_text(message_,
                      "… Esc ou Cancelar interrompe antes do commit.");
  if (jobControl_)
    lv_label_set_text(lv_obj_get_child(jobControl_, 0), "Cancelar");
  if (!worker_.submit([this, action] {
        traceAction(runtime_, action.key, "begin");
        if (retryJob_)
          result_ = runtime_.boot();
        else if (action.key == "menu" || action.key == "switcher" ||
                 action.key == "notifications")
          result_ = (runtime_.faulted() ||
                     runtime_.dispatch("suspend", action.value)) &&
                    runtime_.open("shell") &&
                    runtime_.dispatch(
                        action.key == "notifications" ? "notices" : action.key);
        else
          result_ = runtime_.dispatch(action.key, action.value);
        if (result_ && !retryJob_)
          runtime_.tick();
        traceAction(runtime_, action.key,
                    result_ ? "lua-complete" : "lua-error");
        completed_.store(true, std::memory_order_release);
      })) {
    result_ = false;
    runtime_.error = "Memória insuficiente para executor";
    completed_ = true;
  }
}
void UI::input(const Input &e) {
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
