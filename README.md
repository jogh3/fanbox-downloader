# A pixiv and fanbox scraper
this is a multithreaded command line tool to download from pixiv and fanbox using the apis

## features
* multithreaded downloading for fast archival
* automatically extracts direct api links and zip files (including ugoira metadata)
* parses and handles rate limits/ip bans gracefully
* grabs session cookies directly from your local firefox database

## prerequisites
before building, you need to install the required development libraries for your system:
g++ make libsqlite3-dev libjsoncpp-dev libcurl4-openssl-dev libzip-dev

## installation
``` bash
  git clone https://github.com/jogh3/fanbox-downloader.git
  cd fanbox-downloader
  make
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
it can only grab cookies from a firefox installation used by default package manager, not flatpak or chrome as the location of cookies differs on them, however you can manually enter cookies, by tagging with "pixiv" or "fanbox" as well, to do both at once, pass the --enter-cookies argument twice

