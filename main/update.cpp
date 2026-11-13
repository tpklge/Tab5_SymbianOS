// SPDX-License-Identifier: MIT
#include "runtime.hpp"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "bsp/m5stack_tab5.h"
void tab5_setup_system(tab5::Runtime &runtime) {
 runtime.systemWriter=[&runtime](const std::string &op,std::string &message) {
    if(op=="reboot") {
      if(!runtime.checkpointApps()) { message="Falha ao salvar aplicativos. Reinício cancelado."; return false; }
      esp_restart(); return true;
    }
    if(op=="sleep:1" || op=="sleep:5" || op=="sleep:15") {
      if(!runtime.checkpointApps()) { message="Falha ao salvar aplicativos. Suspensão cancelada."; return false; }
      auto media=runtime.media().info();
      if(media.filming || media.state=="recording" || media.state=="recording_paused" || media.state=="stopping") { message="Salve a gravação antes de suspender."; return false; }
      const unsigned seconds=unsigned(atoi(op.c_str()+6))*60;
      if(esp_sleep_enable_timer_wakeup(uint64_t(seconds)*1000000)!=ESP_OK) {
        message="Não foi possível configurar o temporizador de retomada."; return false;
      }
      bool reconnect=runtime.network().status().connected;
      runtime.network().disconnect();
      if(runtime.radioCommand) {std::string ignored;runtime.radioCommand("disconnect","",ignored);}
      if(!runtime.media().prepareSleep()){message="Aguarde a finalização da mídia antes de suspender.";return false;}
      runtime.storage().write("/logs/sleep.txt","deep_sleep_seconds="+std::to_string(seconds)+"\n");
      // Light sleep retains this application: deep sleep rebooted into Launcher.
      runtime.suspended=true;
      runtime.storage().pause();
      bsp_display_backlight_off();
      auto result=esp_light_sleep_start();
      bool online=runtime.storage().mount();
      runtime.media().resumeSleep();
      runtime.suspended=false;
      if(reconnect && online) runtime.network().resumeAutomatic();
      runtime.lock.engage();
      runtime.screenOffRequested=true;
      if(result!=ESP_OK || !online) {
        message=std::string("Retomada: ")+esp_err_to_name(result)+". SD "+(online?"disponível":"indisponível");return false;
      }
      message="Retomado no SymbianOS. Toque para acender a tela.";return true;
    }

 message="Atualizações somente pelo launcher.";return false;
 };
}
// Called only after native UI initialization, SD essential self-test and a grace period.
void tab5_confirm_runtime(bool healthy) {
  static bool done=false;
  if(done) return;
  auto *running=esp_ota_get_running_partition(); esp_ota_img_states_t state;
  if(running && esp_ota_get_state_partition(running,&state)==ESP_OK && state==ESP_OTA_IMG_PENDING_VERIFY) {
    if(healthy) done=esp_ota_mark_app_valid_cancel_rollback()==ESP_OK;
    else { esp_ota_mark_app_invalid_rollback_and_reboot(); done=true; }
  } else done=true;
}
