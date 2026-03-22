#ifndef INTERNETHPP
#define INTERNETHPP

#include <string>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <json/json.h> 
#include <vector>

using std::string;
using std::vector;
struct memorystruct {
  char *memory;
  size_t size;
};
struct post_info {
  string post_id;
  string date;
};
struct post_data {
  string post_id;
  string date;
  vector<string> img_urls;
  bool is_external;
  string self_url;
  string post_title;
  bool is_ugoira;
  string ugoira_frames;

  post_data(){}
  post_data(string inp_post_id,string inp_date,vector<string> inp_img_urls, bool inp_is_external,string inp_self,string inp_post_title, bool inp_is_ug, string inp_ugoi){
    post_id = inp_post_id;
    date = inp_date;
    img_urls = inp_img_urls;
    is_external = inp_is_external;
    self_url = inp_self;
    post_title = inp_post_title;
    is_ugoira = inp_is_ug;
    ugoira_frames = inp_ugoi;
  }
};
struct img_details{
  memorystruct img_data;
  string img_name;
};
class getwebpage {
  public:
    // static functions for curl callbacks
    static size_t writememorycallback(void *contents, size_t size, size_t nmemb, void *userp);
    static size_t write_file_callback(void *ptr, size_t size, size_t nmemb, void *stream);
    
    // main functions
    string scrape(string url, Json::Value *cookies, string referer="");
    struct memorystruct fetch_image_to_memory(string url, string referer, Json::Value *cookies);
    vector<struct post_info> get_all_post_ids(string url, string id, string type, Json::Value *cookies,string referer);
    string url_encode(string value);
    string url_decode(string value);
    post_data get_post_details(post_info post, Json::Value *cookies, string type,string artist_id="");

    bool contains_ext(string *json_string); 
};

#endif
