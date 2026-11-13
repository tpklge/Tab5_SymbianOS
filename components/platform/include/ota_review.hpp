// SPDX-License-Identifier: MIT
#pragma once
#include "digest.hpp"
#include "storage.hpp"
#include <algorithm>
#include <cctype>
#include <functional>
namespace tab5 {
// Shared by the firmware and host regression with the actual ESP32-P4 BIN.
// Review never writes flash. esp_ota_end performs the final IDF image check.
struct OtaReview {
  size_t bytes=0, capacity=0;
  std::string stage="checksum",expected,actual,version;
};
inline bool reviewOta(Storage &disk,size_t capacity,const std::function<bool()> &cancel,
                      OtaReview &r,std::string &error,const std::function<void()> &yield={}) {
  r={};r.capacity=capacity;std::string checksum,block;
  auto fail=[&](const std::string &why){error=why;return false;};
  if(!disk.read("/update/runtime.sha256",checksum,256))return fail("Falta /SymbianOS/update/runtime.sha256; extraia o kit OTA completo.");
  if(checksum.compare(0,3,"\xef\xbb\xbf")==0)checksum.erase(0,3);
  while(!checksum.empty()&&isspace(static_cast<unsigned char>(checksum.back())))checksum.pop_back();
  if(checksum.size()!=64)return fail("runtime.sha256 deve conter somente os 64 caracteres do hash (mais quebra de linha opcional).");
  for(auto &c:checksum){c=tolower(static_cast<unsigned char>(c));if(!isxdigit(static_cast<unsigned char>(c)))return fail("runtime.sha256 contém caractere inválido.");}
  r.expected=checksum;r.stage="slot";
  if(!capacity)return fail("Nenhum slot OTA inativo disponível nesta tabela de partições.");
  Digest sha;r.stage="read";
  for(;;){
    if(cancel&&cancel())return fail("Revisão OTA cancelada; flash preservada.");
    if(!disk.readRange("/update/runtime.bin",r.bytes,4096,block))return fail("Erro ao ler runtime.bin no offset "+std::to_string(r.bytes)+"; confira SD e arquivo.");
    if(block.empty())break;
    if(!r.bytes){
      r.stage="header";
      if(block.size()<288)return fail("runtime.bin truncado: faltam cabeçalho e descritor ESP.");
      const auto *p=reinterpret_cast<const unsigned char *>(block.data());
      if(p[0]!=0xe9)return fail("runtime.bin não é imagem de aplicativo ESP (ZIP/bootloader não são aceitos).");
      if((unsigned(p[12])|(unsigned(p[13])<<8))!=0x12)return fail("runtime.bin não foi compilado para ESP32-P4.");
      if(p[1]<1||p[1]>16||p[32]!=0x32||p[33]!=0x54||p[34]!=0xcd||p[35]!=0xab)return fail("Descritor ESP inválido; use somente o BIN da aplicação.");
      if(block.compare(80,20,"tab5symbian_runtime\0",20)!=0)return fail("Imagem pertence a outro projeto; esperado tab5symbian_runtime.");
      r.version=block.substr(48,32);r.version.resize(r.version.find('\0')==r.version.npos?r.version.size():r.version.find('\0'));
    }
    if(block.size()>capacity-std::min(capacity,r.bytes)){r.stage="slot";return fail("Imagem excede slot OTA: lidos "+std::to_string(r.bytes+block.size())+", capacidade "+std::to_string(capacity)+" bytes. Use layout compatível pelo launcher.");}
    r.stage="hash";sha.update(block.data(),block.size());r.bytes+=block.size();
    if(yield && (r.bytes%65536)==0)yield();
  }
  if(r.bytes<288)return fail("runtime.bin vazio ou truncado.");
  r.actual=sha.finish();r.stage="compare";
  if(r.actual!=r.expected)return fail("SHA-256 diferente: runtime.bin e runtime.sha256 não correspondem. Reenvie o par da mesma versão. Detalhes em logs/update.txt.");
  r.stage="verified";error.clear();return true;
}
}
