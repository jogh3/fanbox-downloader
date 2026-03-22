#include "internet.hpp"
#include <curl/curl.h>
#include <iostream> 
#include <vector>
#include <algorithm>
#include "filemanage.hpp"
// implementations

size_t getwebpage::writememorycallback(void *contents, size_t size, size_t nmemb, void *userp){
  size_t realsize = size * nmemb;
  struct memorystruct *mem = (struct memorystruct *)userp;

  char *ptr = (char *)realloc(mem->memory, mem->size + realsize + 1);
  if (!ptr) {
    std::cout << "not enough memory (realloc failed)" << std::endl;
    return 0;
  }
  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]),contents,realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

size_t getwebpage::write_file_callback(void *ptr, size_t size, size_t nmemb, void *stream) {
    size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
    return written;
}

string getwebpage::scrape(string url, Json::Value *cookies, string referer){
  CURL *curl_handle;
  CURLcode res;
  struct memorystruct chunk;
  struct curl_slist *headers = NULL;

  // allocate initial memory
  chunk.memory = (char *)malloc(1);
  chunk.size = 0;
  

  // note: strictly speaking, curl_global_init should be in main(), 
  // but it will work here if not called concurrently.
  curl_handle = curl_easy_init();

  string responce = "{}";
  string pixiv_cookies = (*cookies)["pixiv"].asString();
  string fanbox_cookies = (*cookies)["fanbox"].asString();

  if (pixiv_cookies == "" && url.find("pixiv") != string::npos){
    std::cout << "pixiv cookies not found,should work,but some posts may not be available" << std::endl;
  } else if (fanbox_cookies == "" && url.find("fanbox") != string::npos) {
    std::cout << "fanbox cookies not found" << std::endl;
    return "";
  }

  string cookie_header = "";
  if(pixiv_cookies !="") cookie_header += "PHPSESSID=" + pixiv_cookies + "; ";
  if(fanbox_cookies !="") cookie_header += "FANBOXSESSID=" + fanbox_cookies + "; ";   

  if (curl_handle){
    curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/115.0");
    if(!cookie_header.empty()){
      curl_easy_setopt(curl_handle, CURLOPT_COOKIE, cookie_header.c_str());
    }
    if (url.find("fanbox.cc") != string::npos){
        // std::cout << "adding headers to fanbox call" << std::endl;
        headers = curl_slist_append(headers, "Origin: https://www.fanbox.cc");
        headers = curl_slist_append(headers, "Accept: application/json, text/plain, */*");
        headers = curl_slist_append(headers, "x-requested-with: XMLHttpRequest");
        headers = curl_slist_append(headers, "Sec-Fetch-Dest: empty");
        headers = curl_slist_append(headers, "Sec-Fetch-Mode: cors");
        headers = curl_slist_append(headers, "Sec-Fetch-Site: same-site");
        headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.9");
        headers = curl_slist_append(headers, "Connection: keep-alive");
        headers = curl_slist_append(headers, "Host: api.fanbox.cc");
        curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
    }
    if (!referer.empty()) {
      // If we explicitly passed a referrer, use it.
      curl_easy_setopt(curl_handle, CURLOPT_REFERER, referer.c_str());
    } else if (url.find("fanbox.cc") != string::npos) {
      // Default for Fanbox if nothing is passed
      curl_easy_setopt(curl_handle, CURLOPT_REFERER, "https://www.fanbox.cc/");
    } else {
      // Default for Pixiv if nothing is passed
      curl_easy_setopt(curl_handle, CURLOPT_REFERER, "https://www.pixiv.net/");
    }
    curl_easy_setopt(curl_handle, CURLOPT_ACCEPT_ENCODING, ""); 
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, writememorycallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    // std::cout << "url used:" << url << std::endl;
    res = curl_easy_perform(curl_handle);
    
    long http_code = 0;
    curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code == 403 || http_code == 429 || http_code == 503) {
      std::cout << "\n[!] server block detected (http " << http_code << "). you are likely ip banned, rate limited, or your cookies are dead." << std::endl;
      filehandler::write_out(responce);
      return "failed";
    }
    if (res != CURLE_OK){
      std::cout << "curl_easy_perform failed: " << curl_easy_strerror(res) << std::endl;
    } else {
      responce = string(chunk.memory);
    }
    if(headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl_handle);
    free(chunk.memory);
    }
    if (responce.empty() || responce == "{}"){
      std::cout << "\nempty responce from api, prob banned, check config folder for output" << std::endl;
      filehandler::write_out(responce);
      return "failed";
    }
  return responce;
}

struct memorystruct getwebpage::fetch_image_to_memory(string url, string referer, Json::Value *cookies){
  CURL *curl_handle;
  CURLcode res;
  struct curl_slist *headers = NULL;

  struct memorystruct chunk;
  chunk.memory = (char *)malloc(1);
  chunk.size = 0;
  
  string pixiv_cookies = (*cookies)["pixiv"].asString();
  string fanbox_cookies = (*cookies)["fanbox"].asString();

  if (pixiv_cookies == "" && url.find("pixiv") != string::npos){
    std::cout << "pixiv cookies not found..." << std::endl;
  } else if (fanbox_cookies == "" && url.find("fanbox") != string::npos) {
    std::cout << "fanbox cookies not found..." << std::endl;
    return chunk;
  }

  string cookie_string = "";
  if(pixiv_cookies !="") cookie_string += "PHPSESSID=" + pixiv_cookies + "; ";
  if(fanbox_cookies !="") cookie_string += "FANBOXSESSID=" + fanbox_cookies + "; ";

  curl_handle = curl_easy_init();
  if (curl_handle){
    if (url.find("fanbox.cc") != string::npos){
        // std::cout << "adding headers to fanbox call" << std::endl;
        headers = curl_slist_append(headers, "Origin: https://www.fanbox.cc");
        headers = curl_slist_append(headers, "Accept: application/json, text/plain, */*");
        headers = curl_slist_append(headers, "x-requested-with: XMLHttpRequest");
        headers = curl_slist_append(headers, "Sec-Fetch-Dest: empty");
        headers = curl_slist_append(headers, "Sec-Fetch-Mode: cors");
        headers = curl_slist_append(headers, "Sec-Fetch-Site: same-site");
        headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.9");
        headers = curl_slist_append(headers, "Connection: keep-alive");
        // headers = curl_slist_append(headers, "Host: api.fanbox.cc");
        curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
    }
    curl_easy_setopt(curl_handle,CURLOPT_URL,url.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/115.0");
    if (!referer.empty()) {
      // If we explicitly passed a referrer, use it.
      curl_easy_setopt(curl_handle, CURLOPT_REFERER, referer.c_str());
    } else if (url.find("fanbox.cc") != string::npos) {
      // Default for Fanbox if nothing is passed
      curl_easy_setopt(curl_handle, CURLOPT_REFERER, "https://www.fanbox.cc/");
    } else {
      // Default for Pixiv if nothing is passed
      curl_easy_setopt(curl_handle, CURLOPT_REFERER, "https://www.pixiv.net/");
    }
    if (!cookie_string.empty()){
      curl_easy_setopt(curl_handle, CURLOPT_COOKIE, cookie_string.c_str());
    }
    curl_easy_setopt(curl_handle,CURLOPT_WRITEFUNCTION, writememorycallback);
    curl_easy_setopt(curl_handle,CURLOPT_WRITEDATA, (void *)&chunk);
    res = curl_easy_perform(curl_handle);
    if (res != CURLE_OK){
      std::cout << "Download failed: " << url << " Error: " << curl_easy_strerror(res) << std::endl;
            // On failure, free memory and return empty
            free(chunk.memory);
            chunk.memory = NULL;
            chunk.size = 0;
    }
    curl_easy_cleanup(curl_handle);
  }
  if (chunk.size < 100){
    std::cout << "error downloading " << url << std::endl;
    std::cout << "response " << chunk.memory << std::endl;
    chunk.memory = NULL;
    chunk.size = 0;
  }
  if (headers) {
    curl_slist_free_all(headers);
  }
  return chunk;
}

vector<post_info> getwebpage::get_all_post_ids(string url, string id,string type, Json::Value *cookies,string referer){
    vector<post_info> post_ids;
    if (type == "use") {
        // --- EXISTING PIXIV USER LOGIC ---
        // URL: https://www.pixiv.net/ajax/user/{ID}/profile/all
        string api_url = "https://www.pixiv.net/ajax/user/" + id + "/profile/all";
        string json = scrape(api_url, cookies, referer);
        if (json == "failed"){
          post_ids.push_back({"null",""});
          return post_ids;
        } 
        Json::Value root;
        std::stringstream ss(json);
        ss >> root;
        
        if (!root["error"].asBool()) {
            if (root["body"]["illusts"].isObject()) {
                for (auto const& pid : root["body"]["illusts"].getMemberNames()) 
                    post_ids.push_back({pid,"1970-01-01"});
            }
            if (root["body"]["manga"].isObject()) {
                for (auto const& pid : root["body"]["manga"].getMemberNames()) 
                    post_ids.push_back({pid,"1970-01-01"});
            }
        }
    } 
    else if (type == "tag") {
        int page = 1;
        bool keep_going = true;
        string encoded_tag = url_encode(id); 
        while(keep_going) {
            string api_url = "https://www.pixiv.net/ajax/search/artworks/" + encoded_tag +"?word=" + encoded_tag + "&p=" + std::to_string(page) + "&mode=all";
            
            string json = scrape(api_url, cookies, referer);
            if (json == "failed"){
              post_ids.push_back({"null",""});
              return post_ids;
            }
            Json::Value root;
            std::stringstream ss(json);
            ss >> root;

            if (!root["error"].asBool() && root["body"]["illustManga"]["data"].size() > 0) {
                for (const auto& item : root["body"]["illustManga"]["data"]) {
                    post_ids.push_back({item["id"].asString(),item["createDate"].asString()});
                }
                page++;
                // Safety break for testing (remove later)
                if (page > 10) keep_going = false; 
            } else {
                keep_going = false;
            }
       }
    } 
    else if (type == "fan") {
        // --- fanbox logic (updated to use paginateCreator) ---
        // step 1: get the list of all page urls
        string list_url = "https://api.fanbox.cc/post.paginateCreator?creatorId=" + id;
        // std::cout << "fetching pagination list: " << list_url << std::endl;
        
        string list_json = scrape(list_url, cookies, "https://www.fanbox.cc/@"+id+"/posts");
        Json::Value list_root;
        std::stringstream list_ss(list_json);
        list_ss >> list_root;

        // the body should be an array of strings (urls)
        const Json::Value& page_list = list_root["body"];
        
        if (page_list.isArray() && page_list.size() > 0) {
            // step 2: loop through each url provided by the api
            for (const auto& page_url_val : page_list) {
                string page_url = page_url_val.asString();
                // std::cout << "scraping page: " << page_url << std::endl;

                string json = scrape(page_url, cookies, "https://www.fanbox.cc/@"+id+"/posts");
                if (json == "failed"){
                  post_ids.push_back({"null",""});
                  return post_ids;
                }
                Json::Value root;
                std::stringstream ss(json);
                ss >> root;

                const Json::Value& body = root["body"];
                
                // the list urls usually return a direct array of items
                if (body.isArray()) {
                    for (const auto& item : body) {
                         post_ids.push_back({item["id"].asString(), item["publishedDatetime"].asString()});
                    }
                } 
                // handle occasional object wrapper if necessary
                else if (body.isObject() && body["items"].isArray()) {
                    for (const auto& item : body["items"]) {
                        post_ids.push_back({item["id"].asString(), item["publishedDatetime"].asString()});
                    }
                }
            }
        } else {
            std::cout << "no pages found or error in pagination list" << std::endl;
        }
    }

    return post_ids;
}
string getwebpage::url_encode(string value){
  CURL *curl = curl_easy_init();
    if (curl) {
        char *output = curl_easy_escape(curl, value.c_str(), value.length());
        if (output) {
            string result(output);
            curl_free(output);
            curl_easy_cleanup(curl);
            return result;
        }
        curl_easy_cleanup(curl);
    }
    return value;
}
string getwebpage::url_decode(string value){
  CURL *curl = curl_easy_init();
  if (curl) {
    int outlength;
    char *output = curl_easy_unescape(curl, value.c_str(), value.length(), &outlength);
    if (output) {
      string result(output);
      curl_free(output);
      curl_easy_cleanup(curl);
      return result;
    }
    curl_easy_cleanup(curl);
  }
  return value;
}
post_data getwebpage::get_post_details(post_info post, Json::Value *cookies, string type, string artist_id){
  post_data full_data = post_data("","",{""},false,"","",false,"");
  if(type == "use" || type == "tag"){
    string url,referer,json_out,json_img_out; 
    url = "https://www.pixiv.net/ajax/illust/"+post.post_id;
    referer="https://www.pixiv.net/artworks/"+post.post_id;
    json_out=scrape(url,cookies,referer);
    json_img_out=scrape(url+"/pages",cookies,referer);
    if (json_out == "{}"){
      full_data.post_id="null";
      return full_data;
    }
    Json::Value scrape_out, scrape_img_out;
    std::stringstream ss(json_out);
    ss >> scrape_out;
    std::stringstream img_ss(json_img_out);
    img_ss >> scrape_img_out;
    if (scrape_out["error"].asBool()){
      std::cout << "failed to get post data for " << post.post_id << std::endl;
      full_data.post_id = "null";
      return full_data;
    }
    string date = "",post_title = "", ug_frames = "";
    bool is_ug = false;
    vector<string> img_urls;
    post_title= scrape_out["body"]["title"].asString();
    date = post.date;
    if (scrape_out["body"]["illustType"].asInt() == 2) {
      is_ug = true;
      string meta_url = "https://www.pixiv.net/ajax/illust/" + post.post_id + "/ugoira_meta";
      string meta_json = scrape(meta_url, cookies, referer);
      if (meta_json != "{}" && meta_json != "failed") {
        Json::Value meta_root;
        std::stringstream meta_ss(meta_json);
        meta_ss >> meta_root;    
        if (!meta_root["error"].asBool()) {
          // originalSrc is the direct link to the server-side zip file
          img_urls.push_back(meta_root["body"]["originalSrc"].asString());
          // convert the frames array back into a formatted raw string
          ug_frames = meta_root["body"]["frames"].toStyledString();
        }
      }
    } else {
        // standard image extraction for illust/manga
        for (int j = 0; j < scrape_img_out["body"].size(); j++){
          img_urls.push_back(scrape_img_out["body"][j]["urls"]["regular"].asString());
          }
      }
    if (type == "use"){
      // std::cout << "DEBUG: getting uploadDate of:" << scrape_out["body"]["uploadDate"].asString() << "for post" << post.post_id << std::endl;
      date = scrape_out["body"]["uploadDate"].asString();
      // std::cout << "DEBUG: date is now: " << date << std::endl;
    }
    for (int j = 0; j < scrape_img_out["body"].size(); j++){
      img_urls.push_back(scrape_img_out["body"][j]["urls"]["regular"].asString());
    }
    full_data = post_data(post.post_id,date,img_urls,false,"",post_title,is_ug,ug_frames);
    return full_data;

  }else if (type == "fan") {
    string url,referer,json_out;
    url = "https://api.fanbox.cc/post.info?postId="+post.post_id;
    referer = "https://www.fanbox.cc/@"+artist_id+"/posts/"+post.post_id;
    json_out = scrape(url,cookies,referer);
    if (json_out == "{}"){
      full_data.post_id="null";
      return full_data;
    }
    //std::cout << json_out;
    Json::Value scrape_out;
    std::stringstream ss(json_out);
    ss >> scrape_out;
    if (scrape_out["error"].asBool()){
      std::cout << "failed to get post data for " << post.post_id << std::endl;
      return full_data;
    }
    string date="",post_title="",self_url="";
    vector<string> img_urls;
    bool is_external = false;
    post_title = scrape_out["body"]["title"].asString();
    date = scrape_out["body"]["publishedDatetime"].asString();
    if (scrape_out["body"]["body"].isMember("images")){
      for (int j = 0; j < scrape_out["body"]["body"]["images"].size();j++){
        img_urls.push_back(scrape_out["body"]["body"]["images"][j]["originalUrl"].asString());
      }
    }
    string post_text = "";
    if (scrape_out["body"]["body"].isMember("text")) {
        post_text = scrape_out["body"]["body"]["text"].asString();
    } 
    if (scrape_out["body"]["body"].isMember("files") || contains_ext(&post_text)){
      is_external = true;
      self_url = artist_id+".fanbox.cc/posts/"+post.post_id;
    }
    full_data = post_data(post.post_id,date,img_urls,is_external,self_url,post_title,false,"");
    return full_data;
  }
  return full_data;
}
bool getwebpage::contains_ext(string *json_string){
  vector<string> search_terms = {
        ".cc", 
        ".net", 
        ".com", 
        "/posts/",
        "fanbox.cc",
        "pixiv.net"
  };
  for (const string &term : search_terms){
    if (json_string->find(term) != string::npos){
      return true;
    }
  }
  return false;
}
