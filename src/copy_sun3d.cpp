/*
 * copy_sun3d.cpp
 *
 *  Created on: Oct 2, 2013
 *      Author: alan
 */

#include <pwd.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <curl/curl.h>
#include <dirent.h>
#include <cerrno>
#include <algorithm>

#include <png++/png.hpp>
#include <jpeglib.h>

using namespace std;

////////////////////////////////////////////////////////////////////////////////

void SystemCommand(string str) {
  if (system(str.c_str()))
    return;
}

////////////////////////////////////////////////////////////////////////////////

size_t WriteString(char *buf, size_t size, size_t nmemb, string *name) {
  (*name) += string(buf, size * nmemb);
  return size * nmemb;
}

////////////////////////////////////////////////////////////////////////////////

void GetServerFileName(CURL *curl, string *url_name, string *name) {
  cout << "Get file list -->" << endl;

  curl_easy_setopt(curl, CURLOPT_URL,           url_name->c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteString);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA,     name);

  curl_easy_perform(curl);
}

////////////////////////////////////////////////////////////////////////////////

void ParseNames(const string &names,
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

  cout << endl << endl;
}

////////////////////////////////////////////////////////////////////////////////

size_t WriteFile(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  size_t written = fwrite(ptr, size, nmemb, stream);
//  cout << "-";
  return written;
}

////////////////////////////////////////////////////////////////////////////////

void WriteNames(CURL *curl, vector<string> *file_i, vector<string> *file_o) {
  cout << "Copy file list -->" << endl;

  for (size_t i = 0; i < file_i->size(); ++i) {
    cout << (*file_o)[i] << endl;
    FILE *fp = fopen((*file_o)[i].c_str(), "wb");

    curl_easy_setopt(curl, CURLOPT_URL,           (*file_i)[i].c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     fp);
    curl_easy_perform(curl);

    fclose(fp);
  }
  cout << endl;
}

////////////////////////////////////////////////////////////////////////////////

void ServerToLocal(
    CURL *curl, string server_dir, string ext, string local_dir) {
  vector<string> file_i;
  vector<string> file_o;

  string names;

  GetServerFileName(curl, &server_dir, &names);
  ParseNames(names, ext, server_dir, local_dir, &file_i, &file_o);
  names.clear();
  WriteNames(curl, &file_i, &file_o);
}

////////////////////////////////////////////////////////////////////////////////

void DataFromServerToLocal(string sequence_name, string local_dir) {
  string sun3d_path   = "http://sun3d.csail.mit.edu/data/" + sequence_name;

  string sun3d_camera = sun3d_path + "intrinsics.txt";
  string sun3d_image  = sun3d_path + "image/";
  string sun3d_depth  = sun3d_path + "depth/";
  string sun3d_pose   = sun3d_path + "extrinsics/";

  string local_camera = local_dir  + "intrinsics.txt";
  string local_image  = local_dir  + "image/";
  string local_depth  = local_dir  + "depth/";
  string local_pose   = local_dir  + "extrinsics/";

  string image_ext    = ".jpg";
  string depth_ext    = ".png";
  string pose_ext     = ".txt";

  SystemCommand( "mkdir -p " + local_dir);
  SystemCommand( "mkdir -p " + local_image);
  SystemCommand( "mkdir -p " + local_depth);
  SystemCommand( "mkdir -p " + local_pose);

  CURL* curl;
  curl_global_init(CURL_GLOBAL_ALL);
  curl = curl_easy_init();

  ServerToLocal(curl, sun3d_path,  pose_ext,  local_dir);
  ServerToLocal(curl, sun3d_image, image_ext, local_image);
  ServerToLocal(curl, sun3d_depth, depth_ext, local_depth);
  ServerToLocal(curl, sun3d_pose,  pose_ext,  local_pose);

  curl_easy_cleanup(curl);
  curl_global_cleanup();
}

////////////////////////////////////////////////////////////////////////////////

void GetFileNames(const string dir, vector<string> *file_list) {
  DIR *dp;
  struct dirent *dirp;
  if((dp  = opendir(dir.c_str())) == NULL) {
      cout << "Error(" << errno << ") opening " << dir << endl;
  }

  while ((dirp = readdir(dp)) != NULL) {
      file_list->push_back(dir + string(dirp->d_name));
  }
  closedir(dp);

  sort( file_list->begin(), file_list->end() );
  file_list->erase(file_list->begin()); //.
  file_list->erase(file_list->begin()); //..
}

////////////////////////////////////////////////////////////////////////////////

const int kImageRows = 480;
const int kImageCols = 640;
const int kSampleFactor = 2;
const int kImageRowsSub = kImageRows / kSampleFactor;
const int kImageColsSub = kImageCols / kSampleFactor;
const int kImageChannels = 3;
const int kFileNameLength = 24;

typedef unsigned char uchar;
typedef unsigned short ushort;

////////////////////////////////////////////////////////////////////////////////

bool GetDepthData(string file_name, ushort *data) {
  png::image< png::gray_pixel_16 > img(file_name.c_str(),
      png::require_color_space< png::gray_pixel_16 >());

  int index = 0;
  for (int i = 0; i < kImageRows; ++i) {
    for (int j = 0; j < kImageCols; ++j) {
      ushort s = img.get_pixel(j, i);
      *(data + index) = (s << 13 | s >> 3);
      ++index;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////

bool GetImageData(string file_name, uchar *data) {
  unsigned char *raw_image = NULL;

  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  JSAMPROW row_pointer[1];

  FILE *infile = fopen(file_name.c_str(), "rb");
  unsigned long location = 0;

  if (!infile) {
    printf("Error opening jpeg file %s\n!", file_name.c_str());
    return -1;
  }
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, infile);
  jpeg_read_header(&cinfo, TRUE);
  jpeg_start_decompress(&cinfo);

  raw_image = (unsigned char*) malloc(
      cinfo.output_width * cinfo.output_height * cinfo.num_components);
  row_pointer[0] = (unsigned char *) malloc(
      cinfo.output_width * cinfo.num_components);
  while (cinfo.output_scanline < cinfo.image_height) {
    jpeg_read_scanlines(&cinfo, row_pointer, 1);
    for (uint i = 0; i < cinfo.image_width * cinfo.num_components; i++)
      raw_image[location++] = row_pointer[0][i];
  }

  int index = 0;
  for (uint i = 0; i < cinfo.image_height; ++i) {
    for (uint j = 0; j < cinfo.image_width; ++j) {
      for (int k = 0; k < kImageChannels; ++k) {
        *(data + index) = raw_image[(i * cinfo.image_width * 3) + (j * 3) + k];
        ++index;
      }
    }
  }

  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  free(row_pointer[0]);
  fclose(infile);

  return true;
}

////////////////////////////////////////////////////////////////////////////////

void SaveDepthFile(string file_name, ushort *data) {
  cout << file_name << endl;

  png::image<png::gray_pixel_16> img(kImageColsSub, kImageRowsSub);

  for (int i = 0; i < kImageRowsSub; ++i) {
    for (int j = 0; j < kImageColsSub; ++j) {
      ushort s = *(data + i * kImageColsSub + j);
      img[i][j] = (s >> 13 | s << 3);
    }
  }

  img.write(file_name.c_str());
}

////////////////////////////////////////////////////////////////////////////////

void DataSubSampling(uchar *i, uchar *i_sub, ushort *d, ushort *d_sub) {
//  memcpy(i_sub, i, kImageRows * kImageCols * kImageChannels * sizeof(uchar));
//  memcpy(d_sub, d, kImageRows * kImageCols * sizeof(ushort));

  for (int i = 0; i < kImageRowsSub; ++i) {
    for (int j = 0; j < kImageColsSub; ++j) {
      *(d_sub + i * kImageColsSub + j) =
          *(d + i * kSampleFactor * kImageCols + j * kSampleFactor);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

void DataLocalProcess(string local_dir) {
  string local_camera    = local_dir + "intrinsics.txt";
  string local_image     = local_dir + "image/";
  string local_depth     = local_dir + "depth/";
  string local_ply       = local_dir + "ply/";
  string local_depth_sub = local_dir + "depthSub/";

  SystemCommand( "mkdir -p " + local_ply);
  SystemCommand( "mkdir -p " + local_depth_sub);

  int i_ret;
  float fx, fy, cx, cy, ff;
  FILE *fp = fopen(local_camera.c_str(), "r");
  i_ret = fscanf(fp, "%f", &fx);
  i_ret = fscanf(fp, "%f", &ff);
  i_ret = fscanf(fp, "%f", &cx);
  i_ret = fscanf(fp, "%f", &ff);
  i_ret = fscanf(fp, "%f", &fy);
  i_ret = fscanf(fp, "%f", &cy);

  vector<string> image_list;
  vector<string> depth_list;
  GetFileNames(local_image, &image_list);
  GetFileNames(local_depth, &depth_list);

  int total_files = 0;
  image_list.size() < depth_list.size() ?
      total_files = image_list.size() :
      total_files = depth_list.size();

  uchar *image_data = (uchar *) malloc(
      kImageRows * kImageCols * kImageChannels * sizeof(uchar));
  uchar *image_data_sub = (uchar *) malloc(
      kImageRowsSub * kImageColsSub * kImageChannels * sizeof(uchar));
  ushort *depth_data = (ushort *) malloc(
      kImageRows * kImageCols * sizeof(ushort));
  ushort *depth_data_sub = (ushort *) malloc(
      kImageRowsSub * kImageColsSub * sizeof(ushort));

  for (int i = 0; i < total_files; ++i) {
    GetImageData(image_list[i], image_data);
    GetDepthData(depth_list[i], depth_data);

    string depth_serial_name = depth_list[i].substr(
        depth_list[i].size() - kFileNameLength, kFileNameLength);
    string ply_full_name = local_ply + depth_serial_name;
    string depth_full_name_sub = local_depth_sub + depth_serial_name;

    DataSubSampling(image_data, image_data_sub, depth_data, depth_data_sub);
    SaveDepthFile(depth_full_name_sub, depth_data_sub);
  }

  free(image_data);
  free(depth_data);
  free(image_data_sub);
  free(depth_data_sub);
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv) {
  string sequence_name;
  string local_dir;

  if (argc == 1) {
    passwd* pw = getpwuid(getuid());
    std::string home_dir(pw->pw_dir);
    sequence_name = "harvard_c11/hv_c11_2/";
    local_dir     = home_dir + "/data/sun3d/" + sequence_name;
  } else if (argc == 3) {
    sequence_name = argv[1];
    local_dir     = argv[2];
    local_dir     += sequence_name;
  } else {
    cout << "Server and local directories are needed." << endl;
  }

//  DataFromServerToLocal(sequence_name, local_dir);
  DataLocalProcess(local_dir);

  return 0;
}
