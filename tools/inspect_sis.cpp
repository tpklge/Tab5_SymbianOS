// SPDX-License-Identifier: MIT
// Host inspector uses the native parser independently of the Python generator.
#include "installer.hpp"
#include <filesystem>
#include <iostream>
#include <unistd.h>
int main(int argc,char **argv){
 if(argc!=2){std::cerr<<"Usage: sis_inspect file.sis\n";return 2;}
 namespace fs=std::filesystem;auto pattern=(fs::temp_directory_path()/"tab5-sis-inspect-XXXXXX").string();
 auto *created=mkdtemp(pattern.data());if(!created){std::cerr<<"Cannot create temporary inspection directory\n";return 1;}fs::path root(created);
 try {
  fs::create_directories(root);fs::copy_file(argv[1],root/"input.sis");
  tab5::Storage disk(root.string());if(!disk.mount())throw std::runtime_error(disk.lastError());tab5::Installer installer(disk);tab5::Package p;
  if(!installer.inspect("/input.sis",p)||!p.uid)throw std::runtime_error(installer.error.empty()?"not a Tab5 SIS":installer.error);
  std::cout<<"SIS v9 / Tab5 profile "<<p.sisProfile<<" / UNSIGNED\nID: "<<p.id<<"\nUID: 0x"<<std::hex<<p.uid<<std::dec<<"\nTitle: "<<p.title<<"\nAuthor (unverified): "<<p.author<<"\nVersion: "<<p.version<<"\nPayload bytes: "<<p.bytes<<"\nInspection ms: "<<p.inspectionMs<<"\nSHA256: "<<p.archiveHash<<'\n';
  std::cout<<"Schema: "<<p.schema<<"\nPermissions: "<<p.permissions<<"\n";
  for(const auto &dep:p.dependencies)std::cout<<"Dependency: "<<dep.uid<<" "<<dep.minimum<<".."<<dep.maximum<<"\n";
  for(const auto &f:p.hashes)std::cout<<f.second<<"  "<<f.first<<'\n';fs::remove_all(root);return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';fs::remove_all(root);return 1;}
}
