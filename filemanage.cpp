#include "filemanage.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <sqlite3.h> // needed for get_cookies
#include <zip.h>
#include "internet.hpp"

// if you want to use fs:: instead of std::filesystem::
namespace fs = std::filesystem;

// private helper
bool filehandler::contains(string haystack, string needle) {
    return haystack.find(needle) != string::npos;
}

bool filehandler::copy_ffdb(string source,string dest){
  std::ifstream source_file(source,std::ios::binary);
  if (!source_file.is_open()){
    std::cout << "could not open firefox_db" << std::endl;
    return false;
  }
  std::ofstream dest_file(dest,std::ios::binary);
  if (!dest_file.is_open()){
    std::cout << "could not create db copy" << std::endl;
    return false;
  }
  dest_file << source_file.rdbuf();

  if (!dest_file){
    std::cout << "writing failed" << std::endl;
  }
  source_file.close();
  dest_file.close();
  return true;
}

Json::Value filehandler::read_cookies(string file_path) {
  Json::Value root;
  std::ifstream file(file_path);
  if (!file.is_open()){
    std::cout << "could not open cookie file to read" << std::endl;
    return root;
  }
  file >> root;
  file.close();
  return root;
}

bool filehandler::write_cookies(string file_path,string raw_json){
  Json::Value root;
  std::stringstream ss(raw_json);
  ss >> root;
  if (ss.fail()){
    std::cout << "error failed to parse string" << std::endl;
    return false;
  }
  std::ofstream file(file_path);
  if (!file.is_open()){
    std::cout << "could not open cookie file for writing" << std::endl;
    return false;
  }
  file << root;
  file.close();
  return true;
}

string filehandler::get_cookies(string database, string dlloc){
  sqlite3* db;
  sqlite3_stmt* stmt;
  if (sqlite3_open(database.c_str(),&db) != SQLITE_OK){
    std::cout << "failed to open database " << sqlite3_errmsg(db) << std::endl;
    return "{}";
  }
  string query = "SELECT host, name, value FROM moz_cookies WHERE host LIKE '%pixiv.net' OR host LIKE '%fanbox.cc'";
  if (sqlite3_prepare_v2(db,query.c_str(),-1,&stmt,NULL) != SQLITE_OK){
    std::cout << "error failed to prepare query " << sqlite3_errmsg(db) << std::endl;
    sqlite3_close(db);
    return "{}";
  }
  Json::Value root;
  root["dlloc"] = dlloc;
  root["pixiv"] = "";
  root["fanbox"] = "";

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    string host = (const char*)sqlite3_column_text(stmt, 0);
    string name = (const char*)sqlite3_column_text(stmt, 1);
    string value = (const char*)sqlite3_column_text(stmt, 2);

    if (contains(host, "pixiv.net") && name == "PHPSESSID") {
      root["pixiv"] = value;
      // std::cout << root["pixiv"] << std::endl;
    }
    else if (contains(host,"fanbox.cc") && name == "FANBOXSESSID") {
      root["fanbox"]=value;
      // std::cout << root["fanbox"] << std::endl;
    }
  }
  if (root["pixiv"] == "" || root["fanbox"] == ""){
    std::cout << "could not find either pixiv or fanbox cookies:\npixiv: " << root["pixiv"] << "\nfanbox: " << root["fanbox"] << std::endl;
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return root.toStyledString();
}

string filehandler::get_folder_name(string url){
  if (url.find("pixiv.net") != string::npos){
    if(url.find("/users/") != string::npos){
      string after = url.substr(url.find("/users/")+7);
      size_t end = after.find_first_not_of("0123456789");
      if (end != string::npos){
        return after.substr(0,end);
      }
      return after;
    } else if (url.find("/tags/") != string::npos) {
      string after = url.substr(url.find("/tags/")+6);
      size_t end = after.find("/");
      if (end != string::npos) return after.substr(0,end);
      return after;
    }
    return "pixiv_other";
  } else if (url.find("fanbox.cc") != string::npos) {
      string after = url.substr(url.find("https://")+8);
      size_t end = after.find(".");
      string name = after.substr(0,end);
      return name;
  } else {
    return "unknown dump";
  }
}
string filehandler::clean_filename(string input) {
    string invalid_chars = "\\/:?\"<>|*";
    for (char &c : input) {
        // checks if the current char 'c' is in the invalid list
        if (invalid_chars.find(c) != string::npos) {
            c = '_'; // replace with underscore
        }
    }
    return input;
}
bool filehandler::create_folder(string path) {
    try {
        // create_directories works like "mkdir -p", creating parents if needed.
        // it returns false if folder exists, but does not throw error.
        if (std::filesystem::create_directories(path)) {
            if (dir_create == false){
            std::cout << "created directory: " << path << std::endl;
            dir_create = true;
            }
        }
        return true; 
    } catch (std::filesystem::filesystem_error& e) {
        std::cout << "filesystem error: " << e.what() << std::endl;
        return false;
    }
}
string filehandler::get_ext(string url) {
    if (url.find(".png") != string::npos) return ".png";
    if (url.find(".jpg") != string::npos) return ".jpg";
    if (url.find(".jpeg") != string::npos) return ".jpeg";
    if (url.find(".gif") != string::npos) return ".gif";
    return ".jpg"; // fallback
}
bool filehandler::write_out(string responce){
  std::ofstream output_log("output_log.txt");
  if (!output_log.is_open()){
    std::cout << "unable to open file" << std::endl;
    return false;
  }
  output_log << "output: \n" << responce;
  output_log.close();
  return true;
}
bool filehandler::download_img(string url,Json::Value* cookies, string referer,string dl_loc, string img_name, getwebpage& internet){
  memorystruct image = internet.fetch_image_to_memory(url, referer, cookies);
  if (image.memory == NULL) {
        std::cout << "download failed (empty response)" << std::endl;
        return false;
    }
  string img_path = dl_loc+"/"+img_name;
  FILE *fp = fopen(img_path.c_str(), "wb");
  if (!fp){
    std::cout << "unable to download image" << std::endl;
    free(image.memory);
    return false;
  }
  if (image.size > 0){
  fwrite(image.memory,1,image.size,fp);
  }
  fclose(fp);
  free(image.memory);
  return true;
}
bool filehandler::zip_n_dl(vector<img_details>& imgs_to_zip, string dl_name, string dl_path){
  int error=0;
  // ensure we don't end up with "//" if dl_path already has a slash
  string separator = (dl_path.back() == '/') ? "" : "/";
  string full_path = dl_path + separator + dl_name;
  zip_t *archive = zip_open(full_path.c_str(),ZIP_CREATE | ZIP_TRUNCATE,&error);
  if (archive == NULL){
    std::cout << "failed to create archive error code" << error << std::endl;
    return false;
  }
  for (size_t i = 0; i < imgs_to_zip.size(); i++){
    img_details &cur_img = imgs_to_zip[i];
    zip_source_t *source = zip_source_buffer(archive, cur_img.img_data.memory, cur_img.img_data.size, 0);
    if (source == NULL){
      std::cout << "failed to add image to zip" << std::endl;
      zip_close(archive);
      return false;
    }
    zip_int64_t result = zip_file_add(archive, cur_img.img_name.c_str(), source,0);
    if (result < 0){
      zip_source_free(source);
      zip_close(archive);
      std::cout << "failed to add filename" << std::endl;
      return false;
    }
  }
  if (zip_close(archive) < 0) {
        std::cout << "failed to write zip file to disk" << std::endl;
        return false;
    }
  return true;
}
bool filehandler::write_list_non_used(string file_path,string file_name,vector<string>& list){
  std::ofstream outfile(file_path+"/"+file_name);
  if (outfile.is_open()){
    std::cout << "could not open file for writing: " << file_path << std::endl;
    if (file_name == "missing posts.txt"){
      for (const auto post : list){
        std::cout << post << std::endl;
      }
    return false;
    }
  }
  for (size_t i = 0; i < list.size(); i++){
    outfile << list[i] << "\n";
  }
  outfile.close();
  return true;
}
bool filehandler::write_list_external(string file_path, string file_name, vector<string*>& list_to_write) {
    std::ofstream out_file(file_path);
    if (!out_file.is_open()) {
        std::cout << "could not open file for writing: " << file_path << std::endl;
        return false;
    }
    
    for (size_t i = 0; i < list_to_write.size(); i++) {
        // the asterisk here dereferences the pointer, grabbing the actual string
        out_file << *(list_to_write[i]) << "\n";
    }
    
    out_file.close();
    return true;
}
