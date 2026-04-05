#include <iostream>
#include <cstring>
#include <sqlite3.h>
#include <json/json.h>
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include <sstream>
#include <curl/curl.h>
#include <sys/stat.h> // for mkdir
#include <vector>
#include <zip.h>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <thread>
#include <atomic>
#include <mutex>
#include <future>
#include <ctime>
#include <cstdio>
#include "internet.hpp"
#include "filemanage.hpp"
using std::string;

namespace fs = std::filesystem;

void print_results(int amount_dld,int full_dl){
  std::cout << "\33[2K\r" << "downloading " << amount_dld << "/" << full_dl << "\r\n";
}
time_t parse_bash_date(string date_str){ // turns the bash date into a single number to be compared
  struct tm tm = {0};
  if (sscanf(date_str.c_str(),"%d-%d-%d",&tm.tm_year,&tm.tm_mon,&tm.tm_mday) != 3){
    return 0;
  }
  tm.tm_year -= 1900;
  tm.tm_mon -= 1;
  tm.tm_isdst = 0;
  // std::cout << "input_time" << timegm(&tm);
  return timegm(&tm);
}
time_t parse_api_date(string date_str){ // turns the api data into the same single number as bash date for comparison
    struct tm tm = {0};
    int off_sign = 1; // 1 for +, -1 for -
    int off_hour = 0;
    int off_min = 0;
    char sign_char = '+';
    // 1. parse the main date and time part; format: YYYY-MM-DDTHH:MM:SS; we also scan for the timezone sign (+ or -) immediately after
    if (sscanf(date_str.c_str(), "%d-%d-%dT%d:%d:%d%c%d:%d", 
               &tm.tm_year, &tm.tm_mon, &tm.tm_mday, 
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec,
               &sign_char, &off_hour, &off_min) < 6) {
        return 0; // parsing failed
    }
    // 2. adjust struct tm requirements
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    tm.tm_isdst = 0;
    // 3. get the initial timestamp (treating the time as if it were utc for now)
    time_t t = timegm(&tm);
    // 4. handle timezone offset; if the time was "05:00 +09:00", that means it is 05:00 in japan.; to get utc, we must SUBTRACT 9 hours.; if the sign is '-', we add.
    long offset_seconds = (off_hour * 3600) + (off_min * 60);
    if (sign_char == '+') {
        t -= offset_seconds;
    } else if (sign_char == '-') {
        t += offset_seconds;
    }
    // std::cout << "api_time" << t;
    return t;
}
// global variables and structs, mostly for multithreading purposes
std::mutex data_mtx; // synchronization primitive in the <mutex> header used to protect shared data from simultaneous access by multiple threads
std::atomic<int> current_id_index{0};
std::atomic<int> dl_num{0};
std::atomic<bool> stop_flag{false};
std::atomic<bool> ended_early{false};
vector<string> failed_img_dls;
vector<string> downloaded;
struct worker_config {
  string search_type;
  string user_id;
  time_t stop_date;
  Json::Value* cookies;
};
struct worker_dl_config{
  string search_type;
  string dlpath;
  Json::Value* cookies;
  string user_id;
};
size_t Ms_s(int stime){return stime*1000000;} // simple function to turn seconds into microseconds for usleep
// gets the important data of posts, to be downloaded,using multiple threads
void fetch_details_worker(const vector<post_info>& all_ids, vector<post_data>& results,vector<string*>& external_urls, getwebpage& internet, worker_config config){
  while (true){
  if (stop_flag) break;
  int i = current_id_index++; // doesnt need to be locked as current_id_index is atomic(cannot be race conditioned)
  if ( i >= all_ids.size()){
    break;
  }
  post_data pd = internet.get_post_details(all_ids[i],config.cookies,config.search_type,config.user_id);
  // std::cout << "DEBUG: pd.date = " << pd.date << std::endl;
  if (pd.post_id == "null") {stop_flag = true; ended_early = true; break;}
  data_mtx.lock();
  if (config.stop_date != 0 && parse_api_date(pd.date) <= config.stop_date){ // checks if stop date is reached, to avoid wasting time
    std::cout << "reached stop date" << std::endl;
    stop_flag = true;
  }else {
    if (!stop_flag){
      results.push_back(pd);
      if (pd.is_external) external_urls.push_back(&results.back().self_url);
      std::cout << "\33[2K\rGathering metadata: " << results.size() << " posts found..." << std::flush;
    }
  }
  data_mtx.unlock();
  usleep(Ms_s(5));
  }
}
// fetches all the images and downloads them, one thread per post
void fetch_image_worker(const vector<post_data>& all_data,getwebpage& internet, filehandler& files, worker_dl_config config, string& referer){
  while (true){
    if (stop_flag) break;
    int i = current_id_index++;
    if (i >= all_data.size()){
      break;
    }

    post_data current_dl = all_data[i];
    {
    std::lock_guard<std::mutex> lock(data_mtx);
    std::cout << "\33[2K\rDownloading: " << (i+1) << "/" << all_data.size() << " actual num. downloaded: " << (dl_num+1) << " | " << current_dl.post_title << std::flush;
    }
    if (current_dl.img_urls.empty()) continue; // skip if no images
    string safe_title = files.clean_filename(current_dl.post_title);
    string dl_item;
    bool post_success = false;
    if (current_dl.is_ugoira){
      dl_item = current_dl.post_id +"_" + safe_title;
      if(!files.create_folder(config.dlpath+"/"+dl_item)){ // check if folder is unable to be created, stop as could be greater issue
        std::cout << "unable to create post folder" << std::endl;
        stop_flag = true;
        break;
      }
      string zip_name = current_dl.post_id+"_ugoira_frames.zip";
      bool dl_suc = files.download_img(current_dl.img_urls[0],config.cookies,referer,config.dlpath+"/"+dl_item,zip_name,internet);
      current_dl.img_urls.erase(current_dl.img_urls.begin());
      if (dl_suc) { // checks if successful download
        string json_path = config.dlpath+"/"+dl_item+"/"+current_dl.post_id+"_frames.json";
        files.write_string_to_file(json_path, current_dl.ugoira_frames);
        dl_num++;
        post_success=true;
      } else {
        std::lock_guard<std::mutex> lock(data_mtx);
        failed_img_dls.push_back("failed ugoira at post id: " + current_dl.post_id);
      }
    } else if (current_dl.img_urls.size() < 4 && !current_dl.img_urls.empty()){
        int success_dls = 0; // number of images successfully downloaded, for insurance
        for (int j = 0; j < current_dl.img_urls.size(); j++){
          string img_name;
          dl_item = current_dl.post_id+"_"+safe_title;
          if (!files.create_folder(config.dlpath+"/"+dl_item)){std::cout << "unable to create post folder" << std::endl; stop_flag = true; break;}
          if (config.search_type == "use" || config.search_type == "tag" || config.search_type == "user_tag"){ // checks if pixiv or not
            img_name = current_dl.post_id+"_p"+std::to_string(j)+"_master_1200"+files.get_ext(current_dl.img_urls[j]);
          } else if (config.search_type == "fan") { // checks if fanbox
            img_name = current_dl.post_id+"_p"+std::to_string(j)+files.get_ext(current_dl.img_urls[j]);
          }
          bool dl_suc = files.download_img(current_dl.img_urls[j], config.cookies, "", config.dlpath+"/"+dl_item, img_name,internet);
          if (dl_suc) { // checks if download is successful
            success_dls++;
          } else {
            std::lock_guard<std::mutex> lock(data_mtx); // RAII is safer than manual lock/unlock
            failed_img_dls.push_back("failed img dl at post id: " + current_dl.post_id);
          }
          usleep(Ms_s(2));
          // download images
        }
      if (success_dls == current_dl.img_urls.size()) {dl_num++; post_success = true;} // final check to see if entire folder is downloaded properly
    } else if(current_dl.img_urls.size() >= 4) {
      dl_item = current_dl.post_id+"_"+safe_title+".zip";
      vector<img_details> imgs_to_zip; // image data that will be zipped
      for (int j = 0; j < current_dl.img_urls.size();j++){ // downloads the images to memory instead of disk
        string img_name;
        if (config.search_type == "use" || config.search_type == "tag" || config.search_type == "user_tag"){
          img_name = current_dl.post_id+"_p"+std::to_string(j)+"_master_1200"+files.get_ext(current_dl.img_urls[j]);
        } else if (config.search_type == "fan") {
            img_name = current_dl.post_id+"_p"+std::to_string(j)+files.get_ext(current_dl.img_urls[j]);
          }
          imgs_to_zip.push_back({internet.fetch_image_to_memory(current_dl.img_urls[j],referer,config.cookies),img_name});
          usleep(Ms_s(2));
      }
      if (!files.zip_n_dl(imgs_to_zip,dl_item, config.dlpath)){ // writes zip to disk
        std::cout << "zip failed" << std::endl;
        imgs_to_zip.clear();
        {
          std::lock_guard<std::mutex> lock(data_mtx);
          failed_img_dls.push_back("failed zip for post id: " + current_dl.post_id);
        }
        continue;
      }
      imgs_to_zip.clear();
      dl_num++;
      post_success = true;
      // get images to memory, then zip images
    }
    if (post_success){ // final success check for any failed downloads
    std::lock_guard<std::mutex> lock(data_mtx);
    if(config.search_type == "fan"){
    downloaded.push_back(config.user_id+".fanbox.cc/posts/"+current_dl.post_id);
    } else if (config.search_type == "use" || config.search_type == "tag" || config.search_type == "user_tag"){
        downloaded.push_back("www.pixiv.net/en/artwork/"+current_dl.post_id);
    }
    }
  }
}
// checks full unused posts, that either weren't external or failed/did not need to be downloaded
void get_missed_posts(const vector<post_data>& all_data, vector<string>& downloaded, vector<string*>& external, string search_type, string user_id, vector<string>& missed) {  
    // step 1: sort the downloaded vector to allow binary search
    std::sort(downloaded.begin(), downloaded.end());
    // step 2: dereference and sort external urls so we can binary search them too
    vector<string> ext_strings;
    for (size_t i = 0; i < external.size(); i++) {
        ext_strings.push_back(*(external[i]));
    }
    std::sort(ext_strings.begin(), ext_strings.end());
    // step 3: loop through all post data and search
    for (size_t i = 0; i < all_data.size(); i++) {
        string expected_str;
        // reconstruct the url exactly as it was saved in fetch_image_worker
        if (search_type == "fan") {
            expected_str = user_id + ".fanbox.cc/posts/" + all_data[i].post_id;
        } else {
            expected_str = "www.pixiv.net/en/artwork/" + all_data[i].post_id;
        }
        // binary_search returns a boolean (true if found, false if not)
        bool in_dl = std::binary_search(downloaded.begin(), downloaded.end(), expected_str);
        bool in_ext = std::binary_search(ext_strings.begin(), ext_strings.end(), expected_str);
        // if it is in neither, it was missed
        if (!in_dl && !in_ext) {
          string post_url;
          if(search_type == "fan"){
            post_url = user_id+".fanbox.cc/posts/"+all_data[i].post_id;
          } else if (search_type == "use" || search_type == "tag" || search_type == "user_tag"){
              post_url = "www.pixiv.net/en/artwork/"+all_data[i].post_id;
          }
          missed.push_back(post_url);
        }
    }
}

int main(int argc, char* argv[]){
    getwebpage internet; // initialize necessary objects
    filehandler files;
    curl_global_init(CURL_GLOBAL_ALL);
    std::cout << "starting download/cookie getting now" << std::endl;
    string target_url=argv[2]; // parsing the input from the bash frontend
    string stop_date=argv[4];
    string profile_name, pix_cookie = "",fan_cookie = "", dlloc = "";
    if (std::string(argv[12]) != "null"){
      profile_name=argv[12];
    }
    string use_browser_cookies=argv[6];
    if (std::string(argv[8]) != "null"){
      pix_cookie=argv[8];
    }
    if (std::string(argv[10]) != "null"){
      fan_cookie=argv[10];
    }
    string home=argv[13];
    // constant variables to be used throughout the script
    const string pixiv_api="https://www.pixiv.net/ajax";
    const string fanbox_api="https://api.fanbox.cc";
    const string user_agent="Mozilla/5.0 (X11; Arch Linux; Linux x86_64)";
    const string default_dlloc=home+"/Downloads/firefox stuff";
    const string config_dir=home+"/.config/fboxbashdl";
    files.configfolder = config_dir+"/";
    const string cookie_file=config_dir+"/cookies.json";
    const string firefox_db_lo=home+"/.mozilla/firefox/"+profile_name+"/cookies.sqlite";
    string user_id = files.get_folder_name(target_url); // gets id of user being scraped
    user_id = internet.url_decode(user_id);
    string api_url;
    string search_type;
    if (target_url.find("pixiv.net") != string::npos){ // this if statement gets the type of posts being scraped and unnecessarily makes the api url
      if(target_url.find("/users/") != string::npos){
        size_t artwork_pos = target_url.find("/artworks/");
        if (artwork_pos != string::npos && target_url.length() > artwork_pos+10){
          api_url = pixiv_api+"/user/"+user_id;
          search_type = "user_tag";
        } else {
            api_url = pixiv_api+"/user/"+user_id;
            search_type = "use";
          }
      } else if (target_url.find("/tags/") != string::npos) {
        api_url = pixiv_api+"/search/tags/"+user_id;
        search_type="tag";
      }
    } else if (target_url.find("fanbox.cc") != string::npos) {
        api_url = fanbox_api+user_id;
        search_type="fan";
    }
    if (std::string(argv[14]) == "null") { // checks if a new download location has been set or not
      dlloc = default_dlloc;
    } else {
      dlloc = argv[14];
    }
    if (use_browser_cookies == "true"){ // gets and sets/saves the cookies to be used by the scraper, either from browser or manually entered
      std::cout << "copying cookies.sqlite3 now" << std::endl;
      if(!files.copy_ffdb(firefox_db_lo,config_dir+"copydb.sqlite")){
        return -1;
      }
      Json::Value root = files.read_cookies(cookie_file);
      if (std::string(argv[14]) == "null" && root["dlloc"].asString() != "" ){
        dlloc = root["dlloc"].asString();
      }
      string cookies_sjson = files.get_cookies(config_dir+"copydb.sqlite",dlloc);
      if (!files.write_cookies(cookie_file, cookies_sjson)){
        return -1;
      }
    } else if (use_browser_cookies == "false" && (pix_cookie != "" || fan_cookie != "")) {
      Json::Value root = files.read_cookies(cookie_file);
      if (std::string(argv[14]) != "null"){
        root["dlloc"] = dlloc;       
      }
      if (root["dlloc"].asString() == ""){
        root["dlloc"] = dlloc;
      }
      if (pix_cookie != ""){
      root["pixiv"] = pix_cookie;
      }
      if (fan_cookie != ""){
      root["fanbox"] = fan_cookie;
      }
      string cookies_sjson = root.toStyledString();
      if (!files.write_cookies(cookie_file, cookies_sjson)){
        return -1;
      }
    }
    if (std::string(argv[14]) != "null"){ // writes it all to a save file
      Json::Value root = files.read_cookies(cookie_file);
      root["dlloc"] = dlloc;
      string cookies_sjson = root.toStyledString();
      if (!files.write_cookies(cookie_file, cookies_sjson)){
        return -1;
      }
    }
    string dlpath = files.read_cookies(cookie_file)["dlloc"].asString();
    string artist_name="";
    string folder_path="";
    string url_referer="";
    if (search_type == "use"){ // gets the actual name of artist, not just id
      url_referer="https://www.pixiv.net/member.php?id="+user_id;
      Json::Value th_cookies = files.read_cookies(cookie_file);
      string json_response = internet.scrape(api_url+"?full=1",&th_cookies,url_referer);
      Json::Value root;
      std::stringstream ss(json_response);
      ss >> root;
      if (!root["error"].asBool()){
        artist_name = root["body"]["name"].asString();
      } else {
        std::cout << "failed to get artist name: " << root["message"].asString() << std::endl;
        return -1;
      }
      folder_path=dlpath+"/"+artist_name+"("+user_id+")"; // sets the name used the download folder for this scrape in the set download folder
    } else if (search_type == "tag") {
      url_referer="https://www.pixiv.net/";
      folder_path = dlpath+"/"+user_id;
    } else if (search_type == "fan") {
      url_referer = "https://www.fanbox.cc/";
      folder_path = dlpath+"/"+user_id;
    } else if (search_type == "user_tag") {
      url_referer="https://www.pixiv.net/member.php?id="+user_id;
      Json::Value th_cookies = files.read_cookies(cookie_file);
      string json_response = internet.scrape(api_url+"?full=1",&th_cookies,url_referer);
      Json::Value root;
      std::stringstream ss(json_response);
      ss >> root;
      if (!root["error"].asBool()){
        artist_name = root["body"]["name"].asString();
      } else {
        std::cout << "failed to get artist name: " << root["message"].asString() << std::endl;
        return -1;
      }
      size_t tag_start = target_url.find("/artworks/")+10;
      size_t tag_end = target_url.find("?",tag_start);
      if (tag_end == string::npos) tag_end = target_url.length();
      string use_tag = internet.url_decode(target_url.substr(tag_start,tag_end-tag_start));
      use_tag = files.clean_filename(use_tag);
      folder_path = dlpath+"/"+artist_name+"("+user_id+")"+"("+use_tag+")";
    }
    if(!files.create_folder(folder_path)){
      std::cout << "failed to create download folder";
      return -1;
    }
    Json::Value all_cookies = files.read_cookies(cookie_file); // opens the save file for cookies and dl location in memory to be used everywhere
    vector<post_info> all_post_ids = internet.get_all_post_ids(target_url, user_id, search_type, &all_cookies,url_referer); // gets all the posts
    std::cout << "Found " << all_post_ids.size() << " posts" << std::endl;
    // if (search_type == "use"){
    //   // reverse(all_post_ids.begin(),all_post_ids.end());
    // }
    if (search_type == "use" || search_type == "tag" || search_type =="user_tag") { // sorts the posts by ids if scraping pixiv
      std::sort(all_post_ids.begin(), all_post_ids.end(), [](const post_info& a, const post_info& b) {
        // Sort Descending (Newest First)
        // We must compare lengths first to handle "99" vs "100" correctly
        if (a.post_id.length() != b.post_id.length()) {
            return a.post_id.length() > b.post_id.length();
        }
        return a.post_id > b.post_id;
      });
    } 
    time_t real_stop = 0;
    if (stop_date != "1970-01-01"){
      real_stop = parse_bash_date(stop_date);
    }
    vector<post_data> all_post_data;
    all_post_data.reserve(all_post_ids.size()); // reserves space for all the data,, even if not used due to stop data
    vector<string*> external_posts; // vector of pointers is why space is reserved so data isn't reallocated somewhere else, making pointers invalid
    worker_config config = {search_type,user_id,real_stop,&all_cookies}; // sets config for all the workers
    unsigned int num_threads = std::thread::hardware_concurrency(); // gets max num of threads
    if (num_threads == 0){std::cout << "no threads found set default" << std::endl; num_threads = 1;}; // if unable to get info use one thread(every cpu should have at least 1, right?)
    if (num_threads > 2) num_threads = 2; // set it to 2 threads regardless, as this is the max to not get ip banned, from my testing at least
    vector<std::thread> workers;
    for (int i = 0; i < num_threads; i++){ // creates the jobs for the threads
      // we pass 'std::ref' for references so they aren't copied
      workers.push_back(std::thread(
        fetch_details_worker,
        std::ref(all_post_ids),
        std::ref(all_post_data),
        std::ref(external_posts),
        std::ref(internet),
        config
    ));
    }
    for (auto& t : workers){ // buffer to ensure all threads finish what they're doing before moving onto the next part of the program
      if (t.joinable()) t.join();
    }
    current_id_index = 0; stop_flag = false; // reset the variables for next part, downloading
    workers.clear();
    all_post_ids.clear(); // clear all ids as we are done with them, all necessary data is in the all_post_data variable
    if (ended_early == true) return -1;
    if (all_post_data.empty()){ // exit of not posts were collected
      std::cout << "\nno posts available in time frame" << std::endl;
      return -1;
    }
    std::cout << "\nnumber of post data gathered: " << all_post_data.size() << std::endl;
    if (search_type == "fan"){
      std::cout << "all posts with external links -------------------" << std::endl;
      for (const auto* url : external_posts){
        std::cout << *url << std::endl;
      }
      std::cout << "-------------------------------------------------\n" << std::endl;
    }
    worker_dl_config dl_config = {search_type, folder_path,&all_cookies,user_id}; // sets the downloader config, to allow for all threads to have necessary info
    for (int i = 0; i < num_threads; i++){
      workers.push_back(std::thread(
        fetch_image_worker,
        std::ref(all_post_data),
        std::ref(internet),
        std::ref(files),
        dl_config,
        std::ref(url_referer)
      ));
    }
    for (auto& t : workers){
      if(t.joinable()) t.join();
    }
    vector<string> non_used;
    get_missed_posts(all_post_data,downloaded,external_posts,search_type,user_id,non_used); // get all posts that weren't processed in any way
    if (!external_posts.empty()){ // if any external_posts write them to a txt file to be kept just in case
      files.write_list_external(folder_path,"external posts.txt",external_posts);
    }
    if(!non_used.empty()){ // write all un processed posts to a file just in case
      files.write_list_non_used(folder_path,"missing posts.txt",non_used);
    }
    if (failed_img_dls.size() != 0){  // output and print any failed posts
      std::cout << "\nfailed img dls ------------------" << std::endl;
      for (int i = 0; i < failed_img_dls.size(); i++){
        std::cout << failed_img_dls[i] << std::endl;
      }
      std::cout << "---------------------------------" << std::endl;
      files.write_list_non_used(folder_path,"failed images.txt",failed_img_dls);
    }
    std::cout << "\ncompleted download" << std::endl;
    curl_global_cleanup();
    return 1;
}
