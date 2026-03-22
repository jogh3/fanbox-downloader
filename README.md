# A pixiv and fanbox scraper
this is a multithreaded command line tool to download from pixiv and fanbox using the apis

## installation
``` bash
  git clone https://github.com/jogh3/fanbox-downloader.git
  g++ -o fboxbashdl_backend *.cpp -lsqlite3 -ljsoncpp -lcurl -lzip
```
## usage
``` bash
  fboxbashdl [options] 

  --link -l URL                         URL you want to download from
  --date -d DATE                        date you want to download from in format - yyyy-mm-dd (e.g 02/02/2024 or 2 feb 2024)
                                        downloads all images by when not set
  --get-cookies-from-browser PROFILE    gets both fanbox and pixiv cookies from firefox
  --enter-cookies SITE COOKIE           allows manual entry of cookies for either pixiv or fanbox
  --set-download-location DIR           allows you to change the download location of the scraper
  --help -h                             shows this page
```
it can only grab cookies from a firefox installation used by default package manager, not flatpak or chrome, however you can manually enter cookies as well
