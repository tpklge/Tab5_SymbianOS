// SPDX-License-Identifier: MIT
#include "ui.hpp"
#include <SDL.h>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <vector>
using namespace tab5;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static std::vector<uint16_t> pixels(1280 * 720);
static bool down = false;
static int mx = 0, my = 0;
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *data) {
  for (int y = a->y1; y <= a->y2; ++y)
    memcpy(&pixels[y * 1280 + a->x1],
           data + (y - a->y1) * (a->x2 - a->x1 + 1) * 2,
           (a->x2 - a->x1 + 1) * 2);
  SDL_UpdateTexture(texture, nullptr, pixels.data(), 1280 * 2);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, nullptr, nullptr);
  SDL_RenderPresent(renderer);
  lv_display_flush_ready(d);
}
static void pointer(lv_indev_t *, lv_indev_data_t *d) {
  d->point.x = mx;
  d->point.y = my;
  d->state = down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
static lv_obj_t *findButton(lv_obj_t *root, const std::string &caption) {
  if (lv_obj_check_type(root, &lv_label_class) &&
      caption == lv_label_get_text(root)) {
    auto *parent = lv_obj_get_parent(root);
    if (parent && lv_obj_check_type(parent, &lv_button_class))
      return parent;
  }
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
    if (auto *found = findButton(lv_obj_get_child(root, i), caption))
      return found;
  return nullptr;
}
static void widgets(lv_obj_t *root, const lv_obj_class_t *type,
                    std::vector<lv_obj_t *> &out) {
  if (lv_obj_check_type(root, type))
    out.push_back(root);
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
    widgets(lv_obj_get_child(root, i), type, out);
}
static std::string key(SDL_Keycode k) {
  switch (k) {
  case SDLK_RETURN:
    return "enter";
  case SDLK_ESCAPE:
    return "escape";
  case SDLK_TAB:
    return "tab";
  case SDLK_BACKSPACE:
    return "backspace";
  case SDLK_DELETE:
    return "delete";
  case SDLK_LEFT:
    return "left";
  case SDLK_RIGHT:
    return "right";
  case SDLK_UP:
    return "up";
  case SDLK_DOWN:
    return "down";
  default:
    if (k >= 32 && k < 127)
      return std::string(1, char(k));
    return "";
  }
}
int main(int argc, char **argv) {
  setenv("TZ", "<-04>4", 1);
  tzset();
  if (argc < 2) {
    std::cerr
        << "Usage: tab5sim <path/to/SymbianOS> [--smoke screenshot.bmp]\n";
    return 2;
  }
  bool smoke = argc > 2 && std::string(argv[2]) == "--smoke";
  bool preview = argc > 4 && std::string(argv[2]) == "--preview";
  if (smoke || preview)
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 0);
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    std::cerr << SDL_GetError();
    return 2;
  }
  auto *window =
      SDL_CreateWindow("Tab5Symbian — simulador", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1024, 576, SDL_WINDOW_RESIZABLE);
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  if (!window || !renderer) {
    std::cerr << SDL_GetError();
    return 2;
  }
  SDL_RenderSetLogicalSize(renderer, 1280, 720);
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
                              SDL_TEXTUREACCESS_STREAMING, 1280, 720);
  lv_init();
  auto *display = lv_display_create(1280, 720);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  static std::vector<uint8_t> buf(1280 * 40 * 2);
  lv_display_set_buffers(display, buf.data(), nullptr, buf.size(),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flush);
  auto *mouse = lv_indev_create();
  lv_indev_set_type(mouse, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(mouse, pointer);
  int result = 0;
  int signals = 0;
  {
    Storage storage(argv[1]);
    Runtime runtime(storage);
    runtime.setSignal([&](bool) {
      ++signals;
      SDL_Delay(80);
    });
    runtime.boot();
    UI ui(runtime);
    ui.start();
    if (preview) {
      PowerInfo power;
      power.available = true;
      power.percent = 73;
      power.millivolts = 7940;
      power.milliamps = -260;
      power.charging = true;
      power.enabled = true;
      runtime.setPowerInfo(power);
    }
    if (preview && std::string(argv[3]).rfind("camera", 0) == 0) {
      // Original synthetic chart, used only by the desktop preview.
      runtime.media().device.capture = [](Picture &picture) {
        picture.width = 640;
        picture.height = 360;
        picture.pixels.resize(640 * 360 * 2);
        for (unsigned y = 0; y < 360; ++y)
          for (unsigned x = 0; x < 640; ++x) {
            uint16_t pixel = y > 240 - (x % 160) / 2
                                 ? 0x3ce7
                                 : uint16_t(0x4a00 + (x / 40) * 16 + (y / 40));
            size_t at = (y * 640 + x) * 2;
            picture.pixels[at] = pixel & 255;
            picture.pixels[at + 1] = pixel >> 8;
          }
        return true;
      };
    }
    if (preview && std::string(argv[3]) == "network:scan") {
      runtime.network().backend.scan =
          [](std::vector<Network::AccessPoint> &found,
             const std::atomic<bool> &) {
            found = {{"Casa exemplo", "WPA2/WPA3", -38, 6},
                     {"Estúdio exemplo", "WPA3", -59, 11},
                     {"Convidados exemplo", "WPA2", -72, 1}};
            return true;
          };
    }
    if (preview) {
      std::string request = argv[3];
      auto colon = request.find(':');
      if (!runtime.open(request.substr(0, colon)))
        return 1;
      if (colon != std::string::npos)
        runtime.dispatch(request.substr(colon + 1), runtime.view().text);
      if (request == "network:scan") {
        for (unsigned i = 0; i < 100 && runtime.network().status().busy; ++i)
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        runtime.dispatch("refresh");
      }
      if (runtime.current() == "shell")
        runtime.dispatch("menu");
      ui.render();
    }
    SDL_StartTextInput();
    bool quit = false;
    uint64_t last = SDL_GetTicks64();
    unsigned frame = 0;
    while (!quit) {
      SDL_Event e;
      while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT)
          quit = true;
        if (e.type == SDL_MOUSEMOTION) {
          mx = int(e.motion.x);
          my = int(e.motion.y);
        }
        if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
          down = e.type == SDL_MOUSEBUTTONDOWN;
          mx = e.button.x;
          my = e.button.y;
        }
        if (e.type == SDL_TEXTINPUT) {
          Input in;
          in.text = e.text.text;
          ui.input(in);
        }
        if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
          Input in;
          in.key = key(e.key.keysym.sym);
          in.down = e.type == SDL_KEYDOWN;
          in.ctrl = (e.key.keysym.mod & KMOD_CTRL) != 0;
          in.shift = (e.key.keysym.mod & KMOD_SHIFT) != 0;
          in.alt = (e.key.keysym.mod & KMOD_ALT) != 0;
          in.repeat = e.key.repeat;
          ui.input(in);
        }
        if (e.type == SDL_WINDOWEVENT &&
            e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
          Input in;
          in.key = "reset";
          ui.input(in);
        }
      }
      uint64_t now = SDL_GetTicks64();
      lv_tick_inc(uint32_t(now - last));
      last = now;
      ui.poll();
      lv_timer_handler();
      if (smoke) {
        ++frame;
        auto press = [&](const std::string &key, bool ctrl = false,
                         bool shift = false) {
          Input in;
          in.key = key;
          in.ctrl = ctrl;
          in.shift = shift;
          ui.input(in);
        };
        auto settle = [&] {
          uint64_t begin = SDL_GetTicks64();
          while (!ui.idle() && SDL_GetTicks64() - begin < 3000) {
            ui.poll();
            lv_tick_inc(2);
            lv_timer_handler();
            SDL_Delay(2);
          }
          if (!ui.idle()) {
            std::cerr << "UI worker timeout\n";
            result = 1;
          }
        };
        const std::string expected = "Teste UTF-8: ação, café, coração.";
        if (frame == 2) {
          auto click = [&](const std::string &caption) {
            settle();
            auto *button = findButton(lv_screen_active(), caption);
            if (!button) {
              std::cerr << "Missing control: " << caption << "\n";
              result = 1;
              return;
            }
            lv_obj_send_event(button, LV_EVENT_CLICKED, nullptr);
            ui.poll();
            settle();
          };
          // Actual form widgets and input path: masks, separate fields, and
          // dropdown keys.
          runtime.open("clock");
          runtime.dispatch("adjust");
          ui.render();
          std::vector<lv_obj_t *> fields;
          widgets(lv_screen_active(), &lv_textarea_class, fields);
          if (fields.size() != 2)
            result = 1;
          else {
            auto fill = [&](lv_obj_t *field, const std::string &value) {
              lv_group_focus_obj(field);
              press("a", true);
              Input text;
              text.text = value;
              ui.input(text);
            };
            fill(fields[0], "08112026");
            fill(fields[1], "1430");
            if (std::string(lv_textarea_get_text(fields[0])) != "08/11/2026" ||
                std::string(lv_textarea_get_text(fields[1])) != "14:30") {
              std::cerr << "Date/time mask input failed\n";
              result = 1;
            }
            press("backspace");
            Input digit;
            digit.text = "5";
            ui.input(digit);
            if (std::string(lv_textarea_get_text(fields[1])) != "14:35")
              result = 1;
            click("Aplicar data/hora");
            if (runtime.clockText().find("08/11/2026  14:35") ==
                std::string::npos)
              result = 1;
          }
          click("Voltar");
          runtime.open("converter");
          ui.render();
          fields.clear();
          widgets(lv_screen_active(), &lv_textarea_class, fields);
          std::vector<lv_obj_t *> dropdowns;
          widgets(lv_screen_active(), &lv_dropdown_class, dropdowns);
          if (fields.size() != 1 || dropdowns.size() != 2)
            result = 1;
          else {
            lv_group_focus_obj(fields[0]);
            press("a", true);
            Input value;
            value.text = "1,5";
            ui.input(value);
            lv_dropdown_set_selected(dropdowns[0], 0);
            lv_group_focus_obj(dropdowns[0]);
            press("down");
            lv_dropdown_set_selected(dropdowns[1], 0);
            click("Converter");
            if (runtime.view().body.find("1500 m") == std::string::npos) {
              std::cerr << "Dropdown conversion failed\n";
              result = 1;
            }
          }
          storage.write("/home/Documents/exemplo.txt", "Arquivo de teste");
          runtime.open("files");
          ui.render();
          click("exemplo.txt");
          click("Copiar");
          fields.clear();
          widgets(lv_screen_active(), &lv_textarea_class, fields);
          if (fields.size() != 2)
            result = 1;
          else {
            lv_group_focus_obj(fields[1]);
            press("a", true);
            Input dest;
            dest.text = "/home/Documents/copia.txt";
            ui.input(dest);
            click("Confirmar");
            std::string copied;
            if (!storage.read("/home/Documents/copia.txt", copied) ||
                copied != "Arquivo de teste")
              result = 1;
          }
          if (auto *navigation =
                  findButton(lv_screen_active(), "Pasta acima")) {
            lv_obj_update_layout(navigation);
            if (lv_obj_get_width(navigation) > 160 ||
                lv_obj_get_height(navigation) > 50)
              result = 1;
          } else
            result = 1;
          runtime.media().device.capture = [](Picture &picture) {
            picture.width = 640;
            picture.height = 360;
            picture.pixels.resize(640 * 360 * 2);
            for (size_t i = 0; i < picture.pixels.size(); i += 2) {
              picture.pixels[i] = 0x34;
              picture.pixels[i + 1] = 0x12;
            }
            return true;
          };
          if (!runtime.open("camera") || !runtime.dispatch("preview"))
            result = 1;
          ui.render();
          lv_tick_inc(40);
          lv_timer_handler();
          if (pixels[200 * 1280 + 100] != 0x1234) {
            std::cerr << "Mock camera image rendering failed\n";
            result = 1;
          }
          click("Voltar");
          for (int cycle = 0; cycle < 20; ++cycle) {
            if (!runtime.open("clock")) {
              result = 1;
              break;
            }
            ui.render();
            click("Ajustar relógio / fuso");
            if (runtime.view().layout != "form" || !ui.editing())
              result = 1;
            click("Voltar");
            click("Abertos");
            click("Voltar");
            if (!runtime.open("converter")) {
              result = 1;
              break;
            }
            ui.render();
            click("Converter");
            click("Abertos");
            click("Voltar");
            if (!runtime.open("contacts")) {
              result = 1;
              break;
            }
            ui.render();
            click("Novo");
            click("Voltar");
            click("Voltar");
          }
          if (!runtime.open("calculator"))
            result = 1;
          ui.render();
          click("C");
          click("2");
          click("+");
          click("3");
          click("=");
          if (runtime.view().body != "5" || runtime.faulted()) {
            std::cerr << "Touch calculator regression: " << runtime.error
                      << "\n";
            result = 1;
          }
          click("Voltar");
        }
        if (frame == 4) {
          if (!runtime.ready() || !runtime.dispatch("open:notes")) {
            result = 1;
          } else {
            ui.render();
            press("a", true);
            Input text;
            text.text = expected;
            ui.input(text);
            press("left", false, true);
            press("x", true);
            press("v", true);
            press("a", true);
            press("c", true);
            press("x", true);
            press("v", true);
            press("s", true);
            ui.poll();
            settle();
            std::string saved;
            if (!storage.read("/data/notes/document.txt", saved) ||
                saved != expected) {
              std::cerr << "Editing/clipboard smoke failed: " << saved << "\n";
              result = 1;
            }
            press("escape", true);
            ui.poll();
            settle();
            if (!runtime.dispatch("open:notes"))
              result = 1;
            ui.render();
          }
        }
        // Exercise the actual LVGL pointer route to open the external keypad,
        // type q with the touch button, then save through the physical-key API.
        if (frame == 6) {
          storage.write("/data/organizer/alarms.tsv",
                        "T5DATA1\nui\tAlarme de teste UI\t" +
                            std::to_string(runtime.now() - 1) + "\t0\t1\n");
          lv_tick_inc(1000);
        }
        if (frame == 8) {
          Input typed;
          typed.text = "z";
          ui.input(typed);
        }
        if (frame == 12) {
          mx = 470;
          my = 680;
          down = true;
        }
        if (frame == 22)
          down = false;
        if (frame == 32) {
          mx = 65;
          my = 350;
          down = true;
        }
        if (frame == 42)
          down = false;
        if (frame == 52) {
          press("s", true);
          ui.poll();
          settle();
          std::string saved;
          if (!storage.read("/data/notes/document.txt", saved) ||
              saved != expected + "zq") {
            std::cerr << "Touch keyboard smoke failed: " << saved << "\n";
            result = 1;
          }
        }
        if (frame == 56) {
          Input removed;
          removed.key = "card_removed";
          ui.input(removed);
          ui.poll();
          if (runtime.ready())
            result = 1;
          press("enter");
          ui.poll();
          settle();
          if (!runtime.ready() || !runtime.open("notes"))
            result = 1;
          else
            ui.render();
        }
        if (frame > 65) {
          if (signals != 1) {
            std::cerr << "Background delivery/dedup smoke failed\n";
            result = 1;
          }
          auto pixel = pixels[100 * 1280 + 1270];
          if (((pixel >> 11) & 31) + ((pixel >> 5) & 63) + (pixel & 31) > 65) {
            std::cerr << "Dark landscape pixel check failed\n";
            result = 1;
          }

          if (argc > 3) {
            auto *s = SDL_CreateRGBSurfaceWithFormatFrom(
                pixels.data(), 1280, 720, 16, 2560, SDL_PIXELFORMAT_RGB565);
            SDL_SaveBMP(s, argv[3]);
            SDL_FreeSurface(s);
          }
          quit = true;
        }
      }
      if (preview &&
          ++frame > (std::string(argv[3]).rfind("camera", 0) == 0 ? 65 : 8)) {
        auto *surface = SDL_CreateRGBSurfaceWithFormatFrom(
            pixels.data(), 1280, 720, 16, 2560, SDL_PIXELFORMAT_RGB565);
        SDL_SaveBMP(surface, argv[4]);
        SDL_FreeSurface(surface);
        quit = true;
      }
      SDL_Delay(5);
    }
    SDL_StopTextInput();
  }
  lv_deinit();
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  if (smoke && result == 0)
    std::cout
        << "UI smoke passed: mock camera image, Brazilian masks, dropdowns, "
           "file manager "
           "copy/sidebar, 20 clock/converter/switcher/back cycles, "
           "calculator keypad, dark landscape, UTF-8, clipboard, deferred "
           "background input, alarm, touch keypad, SD save\n";
  return result;
}
