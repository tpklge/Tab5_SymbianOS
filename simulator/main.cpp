#include "browser_web.hpp"
#include "browser_document.hpp"
// SPDX-License-Identifier: MIT
#include "ui.hpp"
#include "boot_screen.hpp"
#include <SDL.h>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <random>
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
    BootScreen startup;startup.show(5,"Preparando o sistema");
    if(smoke) {auto p=pixels[10*1280+10];if(p!=uint16_t(((0x24>>3)<<11)|((0x24>>2)<<5)|(0x28>>3))){std::cerr<<"Early boot background not dark gray\n";result=1;}}
    Storage storage(argv[1]);
    Runtime runtime(storage);
    runtime.entropy=[] {std::random_device random;std::string out;const char *hex="0123456789abcdef";for(unsigned i=0;i<16;++i){auto n=random();out+=hex[(n>>4)&15];out+=hex[n&15];}return out;};
    runtime.setSignal([&](bool) {
      ++signals;
      SDL_Delay(80);
    });
    runtime.bootProgress=[&](unsigned percent){startup.show(40+percent*40/100,"Verificando arquivos do cartao");};
    runtime.boot();runtime.bootProgress={};
    UI ui(runtime);
    ui.start();startup.release();
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
    if(preview && std::string(argv[3])=="shell:signal") {
      // Explicit desktop fixture only: no radio or network connection.
      runtime.network().backend.connect=[](const std::string &,const std::string &,std::string &ip,const std::atomic<bool> &){ip="192.0.2.1";return true;};
      runtime.network().backend.signal=[] {return -62;};runtime.network().connect("Prévia simulada","");
      for(unsigned i=0;i<100 && runtime.network().status().busy;++i)SDL_Delay(2);
      runtime.network().autoConnect(1000);
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
    if (preview && std::string(argv[3]) == "network:saved") {
      runtime.network().backend.connect =
          [](const std::string &, const std::string &, std::string &ip,
             const std::atomic<bool> &) {
            ip = "192.0.2.5";
            return true;
          };
      for (auto name : {"Casa exemplo", "Estúdio exemplo"}) {
        runtime.network().connect(name, "apenas-preview", true);
        while (runtime.network().status().busy)
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      runtime.network().setAutomatic("Estúdio exemplo", false);
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
    if (preview && std::string(argv[3]).rfind("mesh_rak",0)==0) {
      // Synthetic UI fixture only; does not open UART or claim hardware success.
      runtime.radioCommand=[](const std::string &op,const std::string &,std::string &out){if(op=="link")out="uart";else if(op=="state")out="Rádio simulado";return true;};
      storage.write("/data/mesh/status.tsv","T5DATA1\nuart\t1\tPrévia com dados simulados\t99\t0\t4\t1\t1\tTab5 Casa\n");
      storage.write("/data/mesh/nodes.tsv","T5DATA1\n99\tTab5 Casa\tCASA\t1794174000\t8.5\t0\t85\t4.10\t-15.598900\t-56.094900\t165\t\n42\tEstação Jardim\tJARD\t1794174000\t6.3\t2\t73\t3.95\t-15.601200\t-56.098100\t170\t\n");
      storage.write("/data/mesh/channels.tsv","T5DATA1\n0\tPrincipal\t1\t\t\n1\tAmigos\t2\t\t\n");
      storage.write("/data/mesh/history.tsv","T5DATA1\n10\t42\t1794174000\t0\tOlá! A estação está recebendo bem por aqui.\trecebida\t4294967295\n11\teu\t1794174060\t0\tÓtimo! Mensagem enviada pelo Tab5.\taceita pelo transporte\t4294967295\n");
    }
    if (preview && std::string(argv[3]).rfind("email",0)==0) {
      MailAccount account;account.address="tab5@example.test";account.name="Meu Tab5";
      account.inHost="imap.example.test";account.inUser=account.address;account.inPassword="senha-recebimento";
      account.outHost="smtp.example.test";account.outUser=account.address;account.outPassword="senha-envio";
      std::string error;runtime.mail().configure(account,error);
    }
    if(preview && argc>5 && (std::string(argv[5])=="en" || std::string(argv[5])=="es"))storage.write("/config/public/language",argv[5]);
    if (preview) {
      std::string request = argv[3];
      auto colon = request.find(':');
      if(request=="shell:switcher") {runtime.open("notes");runtime.open("music");}
      if(request=="boot") startup.show(65,"Verificando arquivos do cartao");
      if (request!="boot" && !runtime.open(request.substr(0, colon)))
        return 1;
      if (colon != std::string::npos)
        runtime.dispatch(request.substr(colon + 1), runtime.view().text);
      if(request=="feeds:html") {
        storage.write("/data/feeds/sources.tsv","T5DATA1\nrich\tHTML preview\thttps://example.test/feed\n");
        storage.write("/data/feeds/cache-rich.xml","<rss><channel><item><title>HTML visual</title><description><![CDATA[<h1>Leitura formatada</h1><p>Parágrafo com <b>destaque</b> e <a href='https://example.test'>link</a>.</p><ul><li>Primeiro</li><li>Segundo</li></ul><img src='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAAAwCAIAAAAuKetIAAAAaElEQVR4nNXOQREAIAzAsFKFCEMT2hCxB9coyNrnUiZxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEufvwNQD4i4B+qYcivsAAAAASUVORK5CYII='>]]></description></item></channel></rss>");
        runtime.dispatch("source:rich");runtime.dispatch("article:1");
      }
      if (request == "network:scan") {
        for (unsigned i = 0; i < 100 && runtime.network().status().busy; ++i)
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        runtime.dispatch("refresh");
      }
      if (runtime.current() == "shell" && colon==std::string::npos)
        runtime.dispatch("menu");
      ui.render();
      if(request=="boot")startup.show(65,"Verificando arquivos do cartao");
    }
    SDL_StartTextInput();
    bool quit = false;
    uint64_t last = SDL_GetTicks64();
    int testBrightness = 100;
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
      if(!(preview && std::string(argv[3])=="boot")) ui.poll();
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
            { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
          }
        };
          auto click = [&](const std::string &caption) {
            settle();
            auto *button = findButton(lv_screen_active(), caption);
            if (!button) {
              std::cerr << "Missing control: " << caption << "\n";
              { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
              return;
            }
            lv_obj_send_event(button, LV_EVENT_CLICKED, nullptr);
            ui.poll();
            settle();
          };
        const std::string expected = "Teste UTF-8: ação, café, coração.";
        if (frame == 2) {
          runtime.open("email");runtime.dispatch("new");ui.render();lv_obj_update_layout(lv_screen_active());
          std::vector<lv_obj_t *> mailEditors;widgets(lv_screen_active(), &lv_textarea_class, mailEditors);
          if(mailEditors.size()!=3 || lv_textarea_get_one_line(mailEditors.back())) {std::cerr<<"Email multiline editor failed\n";result=1;}
          else {
            lv_textarea_set_text(mailEditors[0],"destino@example.test");
            lv_textarea_set_text(mailEditors[1],"Mensagem de teste");
            lv_textarea_set_text(mailEditors[2],"Primeira linha\nSegunda linha");
            auto *save=findButton(lv_screen_active(),"Salvar rascunho");
            if(!save) {std::cerr<<"Email draft footer missing\n";result=1;}
            else { lv_area_t bodyArea,buttonArea;lv_obj_get_coords(mailEditors[2],&bodyArea);lv_obj_get_coords(save,&buttonArea);
              if(bodyArea.y2>=buttonArea.y1) {std::cerr<<"Email editor overlaps footer\n";result=1;}
              click("Salvar rascunho");
            }
          }
          runtime.dispatch("settings");runtime.dispatch("incoming");ui.render();lv_obj_update_layout(lv_screen_active());
          mailEditors.clear();widgets(lv_screen_active(),&lv_textarea_class,mailEditors);
          if(mailEditors.size()!=4 || !lv_textarea_get_password_mode(mailEditors.back())) {std::cerr<<"Email password masking failed\n";result=1;}
          storage.write("/data/feeds/sources.tsv","T5DATA1\nrich\tHTML preview\thttps://example.test/feed\n");
          storage.write("/data/feeds/cache-rich.xml","<rss><channel><item><title>HTML visual</title><description><![CDATA[<h1>Leitura formatada</h1><p>Parágrafo com <b>destaque</b> e <a href='https://example.test'>link</a>.</p><ul><li>Primeiro</li><li>Segundo</li></ul><img src='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAAAwCAIAAAAuKetIAAAAaElEQVR4nNXOQREAIAzAsFKFCEMT2hCxB9coyNrnUiZxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEidxEufvwNQD4i4B+qYcivsAAAAASUVORK5CYII='>]]></description></item></channel></rss>");
          runtime.open("feeds");runtime.dispatch("source:rich");runtime.dispatch("article:1");ui.render();lv_obj_update_layout(lv_screen_active());
          if(runtime.view().layout!="web_document" || !runtime.view().picture || !findButton(lv_screen_active(),"Carregar imagens")){std::cerr<<"HTML raster document failed\n";result=1;}
          auto *saveArticle=findButton(lv_screen_active(),"Salvar offline");
          if(!saveArticle || lv_obj_get_y(saveArticle)<650){std::cerr<<"HTML save action overlays document\n";result=1;}
          runtime.dispatch("toggle_html");ui.render();lv_obj_update_layout(lv_screen_active());
          if(runtime.view().layout!="reader"){std::cerr<<"HTML text fallback failed\n";result=1;}
          storage.write("/config/public/language","en");runtime.open("settings");runtime.dispatch("language");ui.render();lv_obj_update_layout(lv_screen_active());
          if(!findButton(lv_screen_active(),"Apply")||runtime.view().controls[0].keys!="pt\nen\nes"){std::cerr<<"English locale/keys failed\n";result=1;}
          storage.write("/config/public/language","es");runtime.open("contacts");ui.render();lv_obj_update_layout(lv_screen_active());
          if(!findButton(lv_screen_active(),"Nuevo")){std::cerr<<"Spanish locale failed\n";result=1;}
          storage.write("/config/public/language","pt");
          auto savedMemoryReader=runtime.memoryReader;unsigned memoryReads=0;
          runtime.memoryReader=[&]{MemoryInfo m;m.available=true;m.internalTotal=1048576;m.internalFree=900000-(++memoryReads)*100;m.internalLargest=64000;return m;};
          runtime.open("bench");ui.render();lv_obj_update_layout(lv_screen_active());
          if(!findButton(lv_screen_active(),"Verificar SD") || findButton(lv_screen_active(),"Revisar OTA")){std::cerr<<"Measurement toolbar failed: "<<runtime.error<<" title="<<runtime.view().title<<"\n";result=1;}
          auto *measurementScreen=lv_screen_active();auto *measurementTool=findButton(measurementScreen,"Verificar SD");
          if(!measurementTool || lv_obj_get_y(measurementTool)<540){std::cerr<<"Measurement tool overlaps status cards: found="<<(measurementTool!=nullptr)<<" y="<<(measurementTool?lv_obj_get_y(measurementTool):-1)<<" centered="<<runtime.view().centerTools<<"\n";result=1;}
          for(unsigned n=0;n<4;++n){lv_tick_inc(2100);ui.poll();settle();if(lv_screen_active()!=measurementScreen){std::cerr<<"Measurement refresh rebuilt its widget tree\n";result=1;}}
          runtime.memoryReader=savedMemoryReader;
          // Inline composition, directional text, and pagination are real LVGL widgets.
          runtime.radioCommand=[](const std::string &op,const std::string &,std::string &out){if(op=="link")out="uart";else if(op=="state")out="UART simulada";return true;};
          storage.write("/data/mesh/nodes.tsv","T5DATA1\n42\tNó GUI\tGUI\t0\t6.5\t1\t80\t4.0\t\t\t\t\n");
          storage.write("/data/mesh/history.tsv","T5DATA1\n1\t42\t1794174000\t0\tRecebida GUI\trecebida\t4294967295\n2\teu\t1794174001\t0\tEnviada GUI\tpendente\t4294967295\n");
          runtime.open("mesh_rak");runtime.dispatch("history");ui.render();lv_obj_update_layout(lv_screen_active());
          auto *incoming=findButton(lv_screen_active(),"Recebida GUI"),*outgoing=findButton(lv_screen_active(),"Enviada GUI");
          if(!incoming || !outgoing || lv_obj_get_style_text_align(lv_obj_get_child(incoming,0),LV_PART_MAIN)!=LV_TEXT_ALIGN_LEFT || lv_obj_get_style_text_align(lv_obj_get_child(outgoing,0),LV_PART_MAIN)!=LV_TEXT_ALIGN_RIGHT) {std::cerr<<"Chat text direction failed\n";result=1;}
          std::vector<lv_obj_t *> composer;widgets(lv_screen_active(),&lv_textarea_class,composer);
          if(composer.size()!=1 || !findButton(lv_screen_active(),"Enviar")) {std::cerr<<"Inline chat composer missing\n";result=1;}
          auto chatText=[&] {std::vector<lv_obj_t *> inputs;widgets(lv_screen_active(),&lv_textarea_class,inputs);return inputs.empty()?std::string():std::string(lv_textarea_get_text(inputs[0]));};
          Input chatDraft;chatDraft.text="rascunho GUI";ui.input(chatDraft);lv_tick_inc(1100);ui.poll();settle();
          if(chatText().find("rascunho GUI")==std::string::npos){std::cerr<<"Chat refresh lost draft\n";result=1;}
          click("Teclado");auto *sendWithKeyboard=findButton(lv_screen_active(),"Enviar");std::vector<lv_obj_t *> touchKeys;widgets(lv_screen_active(),&lv_buttonmatrix_class,touchKeys);lv_area_t sendArea{},keyboardArea{};
          if(sendWithKeyboard && !touchKeys.empty()){lv_obj_get_coords(sendWithKeyboard,&sendArea);lv_obj_get_coords(touchKeys[0],&keyboardArea);}
          if(!sendWithKeyboard || touchKeys.empty() || sendArea.y2>=keyboardArea.y1){std::cerr<<"Touch keyboard covers chat send button\n";result=1;}
          click("Enviar");if(runtime.view().layout!="chat" || chatText().find("rascunho GUI")!=std::string::npos){std::cerr<<"Inline send did not clear composer\n";result=1;}
          click("Voltar");runtime.dispatch("nodes");ui.render();lv_obj_update_layout(lv_screen_active());
          auto *node=findButton(lv_screen_active(),"Nó GUI"),*next=findButton(lv_screen_active(),"Próxima");lv_area_t nodeArea{},nextArea{};
          if(node && next){lv_obj_get_coords(node,&nodeArea);lv_obj_get_coords(next,&nextArea);}
          if(!node || !next || nextArea.y1<630 || nodeArea.y2>=nextArea.y1){std::cerr<<"Node pagination overlaps content\n";result=1;}
          unsigned diagCounter=0;
          runtime.radioCommand=[&](const std::string &op,const std::string &,std::string &out){if(op=="link")out="uart";else if(op=="state")out="UART simulada";else if(op=="connect_uart")std::this_thread::sleep_for(std::chrono::milliseconds(120));else if(op=="diagnostic")out="UART RX="+std::to_string(++diagCounter);return true;};
          runtime.dispatch("back");ui.render();
          auto *connectBusy=findButton(lv_screen_active(),"Conectar"),*queuedDiagnostic=findButton(lv_screen_active(),"Diagnóstico");
          if(!connectBusy || !queuedDiagnostic){std::cerr<<"Mesh navigation fixture missing\n";result=1;}
          else {lv_obj_send_event(connectBusy,LV_EVENT_CLICKED,nullptr);ui.poll();lv_obj_send_event(queuedDiagnostic,LV_EVENT_CLICKED,nullptr);settle();}
          if(runtime.view().layout!="diagnostic"){std::cerr<<"Diagnostic click during foreground work was discarded\n";result=1;}
          auto reportLabel=[&]() -> lv_obj_t * {std::vector<lv_obj_t *> labels;widgets(lv_screen_active(),&lv_label_class,labels);for(auto *l:labels)if(std::string(lv_label_get_text(l)).rfind("CÓDIGO:",0)==0)return l;return nullptr;};
          auto *diagnosticScreen=lv_screen_active();auto *diagnosticLabel=reportLabel();
          if(diagnosticLabel && lv_obj_get_style_bg_opa(lv_obj_get_parent(diagnosticLabel),LV_PART_MAIN)!=LV_OPA_COVER){std::cerr<<"Diagnostic background must be opaque to clear old text\n";result=1;}
          std::vector<lv_obj_t *> diagEditors;widgets(lv_screen_active(),&lv_textarea_class,diagEditors);if(!diagEditors.empty()){std::cerr<<"Diagnostic uses an editable control\n";result=1;}
          for(unsigned n=0;n<3;++n) {lv_tick_inc(1100);ui.poll();settle();if(!diagnosticLabel || reportLabel()!=diagnosticLabel || lv_screen_active()!=diagnosticScreen){std::cerr<<"Diagnostic refresh rebuilt its screen\n";result=1;}}
          press("down");lv_obj_update_layout(lv_screen_active());if(diagnosticLabel && lv_obj_get_scroll_y(lv_obj_get_parent(diagnosticLabel))<=0){std::cerr<<"Diagnostic keyboard scrolling failed\n";result=1;}
          click("Salvar diagnóstico");std::string diagnosticFile;if(!storage.read("/data/mesh/diagnostic.txt",diagnosticFile) || diagnosticFile.find("ORIGEM:")==std::string::npos){std::cerr<<"Diagnostic export failed\n";result=1;}
          runtime.radioCommand={};
          runtime.open("energy");runtime.dispatch("screen");ui.render();
          std::vector<lv_obj_t *> bars;
          widgets(lv_screen_active(),&lv_slider_class,bars);
          if(bars.size()!=1) {std::cerr<<"Brightness slider missing\n";{ std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result=1; }}
          else {lv_slider_set_value(bars[0],37,LV_ANIM_OFF);lv_obj_send_event(bars[0],LV_EVENT_VALUE_CHANGED,nullptr);}
          click("OK");
          std::string persisted;
          if(runtime.brightness!=37 || !storage.read("/config/public/brightness",persisted) || persisted!="37") {std::cerr<<"Brightness slider commit failed\n";{ std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result=1; }}
          bars.clear();widgets(lv_screen_active(),&lv_slider_class,bars);
          if(!bars.empty()) {lv_slider_set_value(bars[0],63,LV_ANIM_OFF);lv_obj_send_event(bars[0],LV_EVENT_VALUE_CHANGED,nullptr);}
          click("Voltar");if(runtime.brightness!=37) {std::cerr<<"Brightness cancel preview failed\n";{ std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result=1; }}
          runtime.dispatch("sound");ui.render();bars.clear();widgets(lv_screen_active(),&lv_slider_class,bars);
          if(bars.size()!=1) { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result=1; }else {lv_slider_set_value(bars[0],27,LV_ANIM_OFF);lv_obj_send_event(bars[0],LV_EVENT_VALUE_CHANGED,nullptr);}
          click("OK");if(runtime.media().volume()!=27) {std::cerr<<"Volume slider failed\n";{ std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result=1; }}
          runtime.brightness=100;runtime.media().setVolume(60);storage.write("/config/public/brightness","100");
          runtime.network().backend.scan=[](std::vector<Network::AccessPoint> &list,const std::atomic<bool> &){list={{"Auto GUI","WPA2",-50,6}};return true;};
          runtime.open("network");runtime.dispatch("scan");ui.render();
          for(unsigned wait=0;wait<200 && runtime.network().status().busy;++wait) SDL_Delay(2);
          lv_tick_inc(1100);ui.poll();settle();
          std::vector<lv_obj_t *> spinners;widgets(lv_screen_active(),&lv_spinner_class,spinners);for(auto *spinner:spinners)if(!lv_obj_has_flag(spinner,LV_OBJ_FLAG_HIDDEN)) {std::cerr<<"Background tick shows foreground processing indicator\n";result=1;}
          if(!findButton(lv_screen_active(),"Auto GUI")) {std::cerr<<"Automatic scan refresh failed\n";{ std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result=1; }}
          runtime.open("notes");runtime.open("shell");runtime.dispatch("switcher");ui.render();
          std::vector<lv_obj_t *> appButtons;widgets(lv_screen_active(),&lv_button_class,appButtons);
          lv_obj_t *close=nullptr;for(auto *b:appButtons) if(lv_obj_get_width(b)<=64 && lv_obj_get_height(b)<=60) {close=b;break;}
          for(auto *row:appButtons) if(lv_obj_get_height(row)==76 && lv_obj_get_child_count(row)>=3) {
            std::vector<lv_obj_t *> captions;widgets(row,&lv_label_class,captions);
            if(captions.size()>=2) {lv_obj_update_layout(row);lv_area_t a,b;lv_obj_get_coords(captions[0],&a);lv_obj_get_coords(captions[1],&b);if(a.y2+2>b.y1) {std::cerr<<"Task captions overlap\n";result=1;}}
          }
          auto beforeClose=runtime.view().buttons.size();
          if(!close) {std::cerr<<"Compact close X missing\n";{ std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result=1; }} else {lv_obj_send_event(close,LV_EVENT_CLICKED,nullptr);ui.poll();settle();if(runtime.view().buttons.size()>=beforeClose) {std::cerr<<"Task close X failed\n";{ std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result=1; }}}
          // Actual form widgets and input path: masks, separate fields, and
          // dropdown keys.
          runtime.open("clock");
          runtime.dispatch("adjust");
          ui.render();
          std::vector<lv_obj_t *> fields;
          widgets(lv_screen_active(), &lv_textarea_class, fields);
          if (fields.size() != 2)
            { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
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
              { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
            }
            press("backspace");
            Input digit;
            digit.text = "5";
            ui.input(digit);
            if (std::string(lv_textarea_get_text(fields[1])) != "14:35")
              { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
            click("Aplicar data/hora");
            if (runtime.clockText().find("08/11/2026  14:35") ==
                std::string::npos)
              { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
          }
          click("Voltar");
          runtime.open("converter");
          ui.render();
          fields.clear();
          widgets(lv_screen_active(), &lv_textarea_class, fields);
          std::vector<lv_obj_t *> dropdowns;
          widgets(lv_screen_active(), &lv_dropdown_class, dropdowns);
          if (fields.size() != 1 || dropdowns.size() != 2)
            { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
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
              { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
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
            { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
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
              { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
          }
          std::vector<lv_obj_t *> fileButtons;
          widgets(lv_screen_active(), &lv_button_class, fileButtons);
          bool compactFileAction=false;
          for(auto *candidate:fileButtons) {
            lv_obj_update_layout(candidate);
            lv_area_t bounds;lv_obj_get_coords(candidate,&bounds);
            if(bounds.x1>950 && bounds.y1>180 && bounds.y1<580 &&
               lv_obj_get_width(candidate)<=160 && lv_obj_get_height(candidate)<=50) {
              std::vector<lv_obj_t *> images;widgets(candidate,&lv_image_class,images);
              if(!images.empty()) compactFileAction=true;
            }
          }
          if(!compactFileAction) {std::cerr<<"Compact file action icon missing\n";result=1;}
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
            { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
          ui.render();
          lv_tick_inc(40);
          lv_timer_handler();
          if (pixels[200 * 1280 + 100] != 0x1234) {
            std::cerr << "Mock camera image rendering failed\n";
            { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
          }
          click("Voltar");
          for (int cycle = 0; cycle < 20; ++cycle) {
            if (!runtime.open("clock")) {
              { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
              break;
            }
            ui.render();
            click("Ajustes");
            if (runtime.view().layout != "form" || !ui.editing())
              { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
            click("Voltar");
            click("Abertos");
            click("Voltar");
            if (!runtime.open("converter")) {
              { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
              break;
            }
            ui.render();
            click("Converter");
            click("Abertos");
            click("Voltar");
            if (!runtime.open("contacts")) {
              { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
              break;
            }
            ui.render();
            click("Novo");
            click("Voltar");
            click("Voltar");
          }
          if (!runtime.open("calculator"))
            { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
          ui.render();
          click("C");
          click("2");
          click("+");
          click("3");
          click("=");
          if (runtime.view().body != "5" || runtime.faulted()) {
            std::cerr << "Touch calculator regression: " << runtime.error
                      << "\n";
            { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
          }
          click("Voltar");
        }
        if (frame == 4) {
          if (!runtime.ready() || !runtime.dispatch("open:notes")) {
            { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
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
              { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
            }
            press("escape", true);
            ui.poll();
            settle();
            if (!runtime.dispatch("open:notes"))
              { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
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
          auto *keyboardButton=findButton(lv_screen_active(),"Teclado");
          if(keyboardButton) {lv_obj_update_layout(keyboardButton);lv_area_t area;lv_obj_get_coords(keyboardButton,&area);mx=(area.x1+area.x2)/2;my=(area.y1+area.y2)/2;}
          else {std::cerr<<"Keyboard footer button missing\n";{ std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result=1; }}
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
            { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
          }
        }
        if (frame == 56) {
          settle();
          std::atomic<bool> entered{false};
          runtime.network().backend.fetch=[&](const auto &,const auto &,const auto &cancel,std::string &error){entered=true;while(!cancel)std::this_thread::sleep_for(std::chrono::milliseconds(1));std::this_thread::sleep_for(std::chrono::milliseconds(200));error="Cancelado";return false;};
          if(!runtime.network().download("https://example.test/stalled","/data/feeds/latest.xml",true)){std::cerr<<"Recovery HTTP fixture failed\n";result=1;}
          auto waitStart=SDL_GetTicks64();while(!entered&&SDL_GetTicks64()-waitStart<1000)SDL_Delay(1);
          Input removed;
          removed.key = "card_removed";
          auto recoveryStart=SDL_GetTicks64();ui.input(removed);ui.poll();
          if(SDL_GetTicks64()-recoveryStart>=100){std::cerr<<"Recovery blocked the UI waiting for HTTP\n";result=1;}
          settle();runtime.network().backend.fetch={};
          if (runtime.ready())
            { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
          press("enter");
          ui.poll();
          settle();
          if (!runtime.ready() || !runtime.open("notes"))
            { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
          else
            ui.render();
        }
        if(frame==58) {
          runtime.screenWriter=[&](int percent) { testBrightness=percent; };
          runtime.screenMinutes=1;
          lv_display_trigger_activity(nullptr);
          lv_tick_inc(45001); ui.poll();
          if(testBrightness!=20) { std::cerr<<"Idle dim failed\n";{ std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result=1; } }
          lv_tick_inc(15000); ui.poll();
          if(testBrightness!=0) { std::cerr<<"Idle off failed\n";{ std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result=1; } }
          auto before=runtime.view().text;
          ui.input({"z","z",true}); ui.poll();
          if(testBrightness!=100 || runtime.view().text!=before) { std::cerr<<"Wake key consumed failed\n";{ std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result=1; } }
          lv_timer_handler();
          runtime.screenOffRequested=true;ui.poll();
          if(testBrightness!=0 || !lv_obj_get_child_count(lv_layer_top())) { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result=1; }
          else lv_obj_send_event(lv_obj_get_child(lv_layer_top(),-1),LV_EVENT_RELEASED,nullptr);
          lv_timer_handler();
          if(testBrightness!=100) { std::cerr<<"Touch wake failed\n";{ std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result=1; } }
          runtime.screenMinutes=2;
        }
        if(frame==60) {
          settle();
          if(!runtime.open("browser")) {result=1;}
          else {
            ui.render();lv_timer_handler();
            std::string page="<h1>Browser keyboard test</h1><p>Enter opens a cached page.</p><a href='/clicked'>Abrir destino</a>";
            const std::string address="https://example.test/keyboard";
            storage.mkdir("/data/browser");storage.mkdir("/data/browser/cache");
            BrowserDocument document;document.feed(page.data(),page.size());
            if(!storage.write(Network::pageCachePath(address),BrowserDocument::record(address,[&]{auto data=document.finish();BrowserWeb web;web.feed(page.data(),page.size());data.markup=web.finish();return data;}()))) {std::cerr<<"Browser fixture write failed\n";result=1;}
            std::vector<lv_obj_t *> fields;widgets(lv_screen_active(),&lv_textarea_class,fields);
            if(fields.empty())result=1;
            else {lv_textarea_set_text(fields.front(),address.c_str());lv_group_focus_obj(fields.front());press("enter");settle();if(runtime.view().layout!="reader" || runtime.view().text.find("Browser keyboard test")==std::string::npos){std::cerr<<"Browser Enter submit failed: "<<runtime.view().layout<<" / "<<runtime.view().body<<" / "<<runtime.error<<" / submit="<<runtime.view().submitAction<<"\n";result=1;}}
            ui.render();lv_obj_update_layout(lv_screen_active());
            auto *addressControl=findButton(lv_screen_active(),"Endereço"),*pager=findButton(lv_screen_active(),"Anterior");
            std::vector<lv_obj_t*> browserFields;widgets(lv_screen_active(),&lv_textarea_class,browserFields);
            if(!addressControl||!pager||browserFields.empty()||lv_obj_get_width(addressControl)>54||lv_obj_get_height(addressControl)>50||lv_obj_get_height(pager)>44){std::cerr<<"Browser controls are not compact: address="<<(addressControl?lv_obj_get_width(addressControl):-1)<<"x"<<(addressControl?lv_obj_get_height(addressControl):-1)<<" pager="<<(pager?lv_obj_get_height(pager):-1)<<" fields="<<browserFields.size()<<"\n";result=1;}
            else{lv_area_t content,control;lv_obj_get_coords(browserFields.front(),&content);lv_obj_get_coords(addressControl,&control);if(content.x2>=control.x1){std::cerr<<"Browser controls overlap content\n";result=1;}}
            BrowserDocument target;std::string destination="<p>Destino aberto</p>";target.feed(destination.data(),destination.size());storage.write(Network::pageCachePath("https://example.test/clicked"),BrowserDocument::record("https://example.test/clicked",target.finish()));
            auto *nativeLink=findButton(lv_screen_active(),"Abrir destino");if(!nativeLink){std::cerr<<"Native clickable link missing\n";result=1;}else{lv_obj_send_event(nativeLink,LV_EVENT_CLICKED,nullptr);settle();if(runtime.view().text.find("Destino aberto")==std::string::npos){std::cerr<<"Native link did not navigate\n";result=1;}}
            runtime.dispatch("go",address);runtime.dispatch("toggle_render");ui.render();lv_obj_update_layout(lv_screen_active());std::vector<lv_obj_t*> clickTargets;widgets(lv_screen_active(),&lv_button_class,clickTargets);lv_obj_t *htmlLink=nullptr;for(auto *candidate:clickTargets)if(!lv_obj_get_child_count(candidate)){htmlLink=candidate;break;}
            if(!htmlLink||runtime.view().htmlHotspots.empty()){std::cerr<<"HTML clickable hit area missing\n";result=1;}else{lv_obj_send_event(htmlLink,LV_EVENT_CLICKED,nullptr);settle();if(runtime.view().text.find("Destino aberto")==std::string::npos){std::cerr<<"HTML link did not navigate\n";result=1;}}
            runtime.open("notes");ui.render();
          }
        }
        if(frame==61) {
          settle();
          for(unsigned n=0;n<45;++n) storage.write("/home/Transfer/smoke-"+std::to_string(n)+".sis","fixture");
          runtime.open("installer");ui.render();lv_obj_update_layout(lv_screen_active());
          auto *previous=findButton(lv_screen_active(),"Anterior"),*next=findButton(lv_screen_active(),"Próxima"),*theme=findButton(lv_screen_active(),"Tema padrão");
          auto *row=findButton(lv_screen_active(),"smoke-0.sis");
          if(!previous||!next||!theme||!row){std::cerr<<"Installer controls missing\n";result=1;}
          else {
            auto *list=lv_obj_get_parent(row);lv_area_t a{},b{};lv_obj_get_coords(list,&a);lv_obj_get_coords(previous,&b);
            if(a.x2>=b.x1||lv_obj_get_x(previous)!=lv_obj_get_x(next)||lv_obj_get_x(theme)!=lv_obj_get_x(previous)){std::cerr<<"Installer right column overlaps list\n";result=1;}
            lv_group_focus_obj(row);press("pagedown");lv_obj_update_layout(lv_screen_active());int offset=lv_obj_get_scroll_y(list);
            if(offset<=0){std::cerr<<"Installer Page Down did not scroll\n";result=1;}
            runtime.dispatch("refresh");ui.render();lv_obj_update_layout(lv_screen_active());row=findButton(lv_screen_active(),"smoke-0.sis");
            if(!row||lv_obj_get_scroll_y(lv_obj_get_parent(row))!=offset){std::cerr<<"Installer lost list position\n";result=1;}
          }
          runtime.open("notes");ui.render();
        }
        if(frame==62) {
          if(!runtime.lock.configure("","1234",std::string(32,'a'))) { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result=1; }
          settle();press("a",true);Input draft;draft.text="Rascunho preservado no bloqueio";ui.input(draft);
          runtime.lock.engage();ui.poll();settle();
          std::string lockedDraft;
          if(!storage.read("/data/notes/session.tsv",lockedDraft) || lockedDraft.find(draft.text)==std::string::npos) {std::cerr<<"Lock lost unsaved draft\n";{ std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result=1; }}
          if(runtime.current()!="lock" || !findButton(lv_screen_active(),"OK")) {std::cerr<<"PIN UI did not lock\n";{ std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result=1; }}
          // Physical matrix input while the first digit job is already running.
          for(unsigned key=1;key<=4;++key) {Input raw;raw.key="matrix:"+std::to_string(key);ui.input(raw);ui.poll();raw.down=false;ui.input(raw);}
          press("enter");settle();
          if(runtime.lock.locked() || runtime.current()=="lock" || runtime.view().text!=draft.text) {std::cerr<<"Rapid physical PIN input failed\n";result=1;}
          runtime.lock.engage();ui.poll();settle();
          auto *stableKey=findButton(lv_screen_active(),"1");click("1");
          if(findButton(lv_screen_active(),"1")!=stableKey) {std::cerr<<"PIN keypad rebuilt for digit\n";result=1;}
          for(auto digit:{"2","3","4","OK"}) {auto *key=findButton(lv_screen_active(),digit);if(!key){result=1;break;}lv_obj_send_event(key,LV_EVENT_CLICKED,nullptr);ui.poll();}
          settle();
          if(runtime.lock.locked() || runtime.current()=="lock" || runtime.view().text!=draft.text) {std::cerr<<"Touch PIN failed\n";{ std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result=1; }}
          if(!runtime.lock.configure("1234","",std::string(32,'a'))) { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result=1; }
        }
        if (frame > 65) {
          if (signals != 1) {
            std::cerr << "Background delivery/dedup smoke failed\n";
            { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
          }
          auto pixel = pixels[100 * 1280 + 1270];
          if (((pixel >> 11) & 31) + ((pixel >> 5) & 63) + (pixel & 31) > 115) {
            std::cerr << "Dark landscape pixel check failed\n";
            { std::cerr << "Smoke assertion line " << __LINE__ << "\n"; result = 1; }
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
        << "UI smoke passed: sliders, automatic Wi-Fi status, compact close X, numeric touch PIN, idle dim/off, touch/key wake, mock camera image, Brazilian masks, dropdowns, "
           "file manager "
           "copy/sidebar, 20 clock/converter/switcher/back cycles, "
           "calculator keypad, dark landscape, UTF-8, clipboard, deferred "
           "background input, alarm, touch keypad, SD save\n";
  return result;
}
