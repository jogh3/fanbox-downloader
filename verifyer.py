import requests
import os
import re

# ==============================================================================
# CONFIGURATION - FILL THESE IN
# ==============================================================================
# 1. Paste the link to the artist's Pixiv page
pixiv_url = "https://www.pixiv.net/en/users/45072297" 

# 2. Paste your PHPSESSID cookie value here (just the value, no "PHPSESSID=")
pixiv_cookie = "63594709_w24uApeZo71cRLgZOA5IBKPVD9DAAbSm" 

# 3. Paste the exact path to where the artist's folder is on your system
download_directory = "/home/joe/Downloads/firefox stuff/FeetArtist(45072297)/" 
# ==============================================================================

def get_api_post_ids(artist_id):
    url = f"https://www.pixiv.net/ajax/user/{artist_id}/profile/all"
    headers = {
        "User-Agent": "Mozilla/5.0 (X11; Arch Linux; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/115.0",
        "Referer": f"https://www.pixiv.net/en/users/{artist_id}/artworks",
        "Cookie": f"PHPSESSID={pixiv_cookie}"
    }
    
    print(f"fetching post list from pixiv api for user {artist_id}...")
    response = requests.get(url, headers=headers)
    
    if response.status_code != 200:
        print(f"api request failed. status code: {response.status_code}")
        return set()
        
    data = response.json()
    if data.get("error"):
        print(f"pixiv returned an error: {data.get('message')}")
        return set()
        
    # pixiv returns the ids as keys in the 'illusts' and 'manga' dictionaries
    illusts = data["body"].get("illusts", {})
    manga = data["body"].get("manga", {})
    
    illust_ids = list(illusts.keys()) if isinstance(illusts, dict) else []
    manga_ids = list(manga.keys()) if isinstance(manga, dict) else []
    
    all_api_ids = set(illust_ids + manga_ids)
    print(f"found {len(all_api_ids)} total posts on the api.")
    return all_api_ids

def get_local_post_ids(directory):
    print(f"scanning local directory: {directory}")
    if not os.path.exists(directory):
        print("directory does not exist. check your path.")
        return set()
        
    local_ids = set()
    # regex to catch your naming convention: title_id or title_id.zip
    # looks for an underscore, followed by digits, optionally followed by .zip, at the end of the string
    id_pattern = re.compile(r'_(\d+)(?:\.zip)?$')
    
    for filename in os.listdir(directory):
        match = id_pattern.search(filename)
        if match:
            local_ids.add(match.group(1))
            
    print(f"found {len(local_ids)} downloaded posts in the folder.")
    return local_ids

def main():
    # extract artist id from the url
    match = re.search(r'/users/(\d+)', pixiv_url)
    if not match:
        print("could not extract artist id from the url. check the format.")
        return
        
    artist_id = match.group(1)
    
    api_ids = get_api_post_ids(artist_id)
    if not api_ids:
        return
        
    local_ids = get_local_post_ids(download_directory)
    
    missing_ids = api_ids - local_ids
    
    print("\n----------------------------------------")
    if not missing_ids:
        print("verification passed. all posts are accounted for.")
    else:
        print(f"missing {len(missing_ids)} posts. here are the ids:")
        # sort descending (newest first) to match how pixiv displays them
        for post_id in sorted(list(missing_ids), key=int, reverse=True):
            print(f"- {post_id} (https://www.pixiv.net/en/artworks/{post_id})")
    print("----------------------------------------\n")

if __name__ == "__main__":
    main()
