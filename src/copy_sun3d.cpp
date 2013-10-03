/*
 * copy_sun3d.cpp
 *
 *  Created on: Oct 2, 2013
 *      Author: alan
 */

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <curl/curl.h>

using namespace std;

void system_command(string str) {
  if (system(str.c_str()))
    return;
}

size_t write_string(char *buf, size_t size, size_t nmemb, string *name) {
  (*name) += string(buf, size * nmemb);
  return size * nmemb;
}

void get_file_name(CURL *curl, string *url_name, string *name) {
  curl_easy_setopt(curl, CURLOPT_URL,           url_name->c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_string);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA,     name);

  curl_easy_perform(curl);
}

void parse_names(const string &names,
                 string ext,
                 const string &sun3d_dir,
                 const string &local_dir,
                 vector<string> *file_i,
                 vector<string> *file_o) {
  ext += "\"";

  unsigned int pos = names.find(ext);
  while( pos < names.size()) {
    unsigned p = names.rfind("\"", pos);
    p++;

    string n = names.substr(p, pos - p + 4);
    cout << "-";
    file_i->push_back(sun3d_dir + n);
    file_o->push_back(local_dir + n);

    pos = names.find(ext, pos + ext.size());
  }
  cout << endl;
}

size_t write_file(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  size_t written = fwrite(ptr, size, nmemb, stream);
  cout << "-";
  return written;
}

void write_names(CURL *curl, vector<string> *file_i, vector<string> *file_o) {
  for (size_t i = 0; i < file_i->size(); ++i) {
    FILE *fp = fopen((*file_o)[i].c_str(), "wb");

    curl_easy_setopt(curl, CURLOPT_URL,           (*file_i)[i].c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     fp);
    curl_easy_perform(curl);

    fclose(fp);
  }
  cout << endl;
}

int main(int argc, char **argv) {
  string sequence_name;
  string local_dir;

  if (argc == 1) {
    sequence_name = "hotel_umd/maryland_hotel3/";
    local_dir     = "/home/alan/DATA/SUN3D/" + sequence_name;
  } else if (argc == 3) {
    sequence_name = argv[1];
    local_dir     = argv[2];
    local_dir     += sequence_name;
  } else {
    cout << "Remote and local directories are needed." << endl;
  }

  string sun3d_path   = "http://sun3d.csail.mit.edu/data/" + sequence_name;
  string sun3d_camera = sun3d_path + "intrinsics.txt";
  string sun3d_image  = sun3d_path + "image/";
  string sun3d_depth  = sun3d_path + "depth/";
  string local_image  = local_dir  + "image/";
  string local_depth  = local_dir  + "depth/";
  string local_camera = local_dir  + "intrinsics.txt";
  string image_ext    = ".jpg";
  string depth_ext    = ".png";

  system_command( "mkdir -p " + local_dir);
  system_command( "mkdir -p " + local_image);
  system_command( "mkdir -p " + local_depth);

  vector<string> file_i;
  vector<string> file_o;

  file_i.push_back(sun3d_camera);
  file_o.push_back(local_camera);

  CURL* curl;
  curl_global_init(CURL_GLOBAL_ALL);
  curl = curl_easy_init();

  cout << "Get file list.." << endl << endl;

  string names;
  get_file_name(curl, &sun3d_image, &names);
  parse_names(names, image_ext, sun3d_image, local_image, &file_i, &file_o);
  names.clear();

  get_file_name(curl, &sun3d_depth, &names);
  parse_names(names, depth_ext, sun3d_depth, local_depth, &file_i, &file_o);
  names.clear();

  cout << "Copy file list.." << endl << endl;

  write_names(curl, &file_i, &file_o);

  curl_easy_cleanup(curl);
  curl_global_cleanup();

  return 0;
}
