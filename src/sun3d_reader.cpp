// This code is a demo to download data from the SUN3D server (http://sun3d.csail.mit.edu/data/) and load the data in C++.

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

void ServerToLocal( CURL *curl,
    string server_dir, const string &ext, const string &local_dir) {
  vector<string> file_i;
  vector<string> file_o;

  string names;

  GetServerFileName(curl, &server_dir, &names);
  ParseNames(names, ext, server_dir, local_dir, &file_i, &file_o);
  names.clear();
  WriteNames(curl, &file_i, &file_o);
}

////////////////////////////////////////////////////////////////////////////////

void DataFromServerToLocal(const string &sequence_name,
                           const string &local_dir) {
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

void GetLocalFileNames(const string &dir, vector<string> *file_list) {
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
const int kSampleFactor = 30;
const int kImageChannels = 3;
const int kFileNameLength = 24;
const int kTimeStampPos = 8;
const int kTimeStampLength = 12;

typedef unsigned char uchar;
typedef unsigned short ushort;

struct _cam_k {
  float fx;
  float fy;
  float cx;
  float cy;
} cam_K;

typedef struct _extrinsic {
  float R[3][3];
  float t[3];
} extrinsic;

////////////////////////////////////////////////////////////////////////////////

bool GetDepthData(const string &file_name, ushort *data) {
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

bool GetImageData(const string &file_name, uchar *data) {
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

void GetExtrinsicData(const string &file_name, int size,
                      vector<extrinsic> *poses) {
  FILE *fp = fopen(file_name.c_str(), "r");

  for (int i = 0; i < size; ++i) {
    extrinsic m;
    for (int d = 0; d < 3; ++d) {
      int iret;
      iret = fscanf(fp, "%f", &m.R[d][0]);
      iret = fscanf(fp, "%f", &m.R[d][1]);
      iret = fscanf(fp, "%f", &m.R[d][2]);
      iret = fscanf(fp, "%f", &m.t[d]);
    }
    poses->push_back(m);
  }

  fclose(fp);
}

////////////////////////////////////////////////////////////////////////////////

void WritePlyHead(FILE *fp, int size) {
  cout << "Write PLY head -->" << endl;

  fprintf(fp, "ply\n");
  fprintf(fp, "format binary_little_endian 1.0\n");
  fprintf(fp, "element vertex %d\n", kImageRows * kImageCols * size);
  fprintf(fp, "property float x\n");
  fprintf(fp, "property float y\n");
  fprintf(fp, "property float z\n");
  fprintf(fp, "property uchar red\n");
  fprintf(fp, "property uchar green\n");
  fprintf(fp, "property uchar blue\n");
  fprintf(fp, "end_header\n");
}

////////////////////////////////////////////////////////////////////////////////

void WritePlyFile(FILE *fp,
                  const extrinsic ex, uchar *image, ushort *depth) {
  for (int v = 0; v < kImageRows; ++v) {
    for (int u = 0; u < kImageCols; ++u) {
      float iz = *(depth + v * kImageCols + u) / 1000.f;
      float ix = iz * (u - cam_K.cx) / cam_K.fx;
      float iy = iz * (v - cam_K.cy) / cam_K.fy;

      float x, y, z;
      x = ex.R[0][0] * ix + ex.R[0][1] * iy + ex.R[0][2] * iz + ex.t[0];
      y = ex.R[1][0] * ix + ex.R[1][1] * iy + ex.R[1][2] * iz + ex.t[1];
      z = ex.R[2][0] * ix + ex.R[2][1] * iy + ex.R[2][2] * iz + ex.t[2];

      fwrite(&x, sizeof(float), 1, fp);
      fwrite(&y, sizeof(float), 1, fp);
      fwrite(&z, sizeof(float), 1, fp);

      uchar r = *(image + kImageChannels * (v * kImageCols + u) + 0);
      uchar g = *(image + kImageChannels * (v * kImageCols + u) + 1);
      uchar b = *(image + kImageChannels * (v * kImageCols + u) + 2);

      fwrite(&r, sizeof(uchar), 1, fp);
      fwrite(&g, sizeof(uchar), 1, fp);
      fwrite(&b, sizeof(uchar), 1, fp);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

int GetTimeStamp(const string &file_name) {
  return atoi(file_name.substr(
              file_name.size() - kFileNameLength + kTimeStampPos,
              kTimeStampLength).c_str());
}

////////////////////////////////////////////////////////////////////////////////

void AssignDepthList(vector<string> image_list, vector<string> *depth_list) {
  vector<string> depth_temp;
  depth_temp.swap(*depth_list);
  depth_list->clear();
  depth_list->reserve(image_list.size());

  int idx = 0;
  int depth_time = GetTimeStamp(depth_temp[idx]);
  int time_low = depth_time;


  for (unsigned int i = 0; i < image_list.size(); ++i) {
    int image_time = GetTimeStamp(image_list[i]);

    while (depth_time < image_time) {
      if (idx == depth_temp.size() - 1)
        break;

      time_low = depth_time;
      depth_time = GetTimeStamp(depth_temp[++idx]);
    }

    if (idx == 0 && depth_time > image_time) {
      depth_list->push_back(depth_temp[idx]);
      continue;
    }

    if (abs(image_time - time_low) < abs(depth_time - image_time)) {
      depth_list->push_back(depth_temp[idx-1]);
    } else {
      depth_list->push_back(depth_temp[idx]);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

void DataLocalProcess(string local_dir) {
  string local_camera    = local_dir + "intrinsics.txt";
  string local_image     = local_dir + "image/";
  string local_depth     = local_dir + "depth/";
  string local_ply       = local_dir + "ply/";
  string local_extrinsic = local_dir + "extrinsics/";

  SystemCommand( "mkdir -p " + local_ply);
  SystemCommand( "mkdir -p " + local_depth_sub);

  int i_ret;
  float ff;
  FILE *fp = fopen(local_camera.c_str(), "r");
  i_ret = fscanf(fp, "%f", &cam_K.fx);
  i_ret = fscanf(fp, "%f", &ff);
  i_ret = fscanf(fp, "%f", &cam_K.cx);
  i_ret = fscanf(fp, "%f", &ff);
  i_ret = fscanf(fp, "%f", &cam_K.fy);
  i_ret = fscanf(fp, "%f", &cam_K.cy);
  fclose(fp);

  vector<string> image_list;
  vector<string> depth_list;
  vector<string> extrinsic_list;

  GetLocalFileNames(local_image, &image_list);
  GetLocalFileNames(local_depth, &depth_list);
  GetLocalFileNames(local_extrinsic, &extrinsic_list);
  AssignDepthList(image_list, &depth_list);

  vector<extrinsic> extrinsic_poses;
  GetExtrinsicData(
      extrinsic_list[extrinsic_list.size() - 1],
      image_list.size(),
      &extrinsic_poses);

  int total_files = 0;
  image_list.size() < depth_list.size() ?
      total_files = image_list.size() :
      total_files = depth_list.size();

  uchar *image_data = (uchar *) malloc(
      kImageRows * kImageCols * kImageChannels * sizeof(uchar));
  ushort *depth_data = (ushort *) malloc(
      kImageRows * kImageCols * sizeof(ushort));

  fp = fopen((local_dir + "ply/scene.ply").c_str(), "w");
  WritePlyHead(fp, (int)image_list.size() / kSampleFactor);

  cout << "Write PLY file -->" << endl;

  for (int i = 0; i < image_list.size(); i += kSampleFactor) {
    cout << depth_list[i] << endl;

    GetImageData(image_list[i], image_data);
    GetDepthData(depth_list[i], depth_data);

    string depth_serial_name = depth_list[i].substr(
        depth_list[i].size() - kFileNameLength, kFileNameLength);
    string ply_full_name = local_ply +
        depth_serial_name.substr(0, kFileNameLength - 4) + ".ply";

    WritePlyFile(fp, extrinsic_poses[i], image_data, depth_data);
  }

  fclose(fp);

  free(image_data);
  free(depth_data);
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

  DataFromServerToLocal(sequence_name, local_dir);
  DataLocalProcess(local_dir);

  return 0;
}

