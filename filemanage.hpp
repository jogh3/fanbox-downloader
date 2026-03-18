#ifndef FILESHPP
#define FILESHPP

#include <string>
#include <vector>
#include <json/json.h>
#include "internet.hpp" // needed because you use 'getwebpage' in the function args

using std::string;
using std::vector;
class getwebpage;
class filehandler {
  public:
    static string configfolder;
  private:
    // helper function used internally by get_cookies
    bool contains(string haystack, string needle);
    bool dir_create = false;

  public:
    // file system operations
    bool create_folder(string path);
    string clean_filename(string input);
    string get_folder_name(string url);

    // json/file io operations
    bool copy_ffdb(string source, string dest);
    Json::Value read_cookies(string file_path);
    bool write_cookies(string file_path, string raw_json);

    // sqlite operation
    string get_cookies(string database, string dlloc);
    
    string get_ext(string url);
    void processpost(string post_id, string folder_path, vector<string> image_urls, Json::Value cookie_string, getwebpage& internet);
    string url_encode(string value);
    static bool write_out(string response);

    bool download_img(string url,Json::Value* cookies, string referer,string dl_loc, string img_name, getwebpage& internet);
    bool zip_n_dl(vector<img_details>& imgs_to_zip,string dl_name,string dl_path);
    bool write_list_non_used(string file_path, string file_name, vector<string>& list);
    bool write_list_external(string file_path, string file_name, vector<string*>& list_to_write); 
};

#endif
