#ifndef SHADOW_JIMAGE_HPP
#define SHADOW_JIMAGE_HPP

#include "boxes.hpp"
#include "util.hpp"

#include <string>

#ifdef USE_OpenCV
#include <opencv2/opencv.hpp>
#endif

#define USE_ArcSoft
#ifdef USE_ArcSoft
#include "asvloffscreen.h"
#endif

class Scalar {
public:
  Scalar() {}
  Scalar(int r_t, int g_t, int b_t) {
    r = (unsigned char)constrain(0, 255, r_t);
    g = (unsigned char)constrain(0, 255, g_t);
    b = (unsigned char)constrain(0, 255, b_t);
  }
  unsigned char r, g, b;
};

enum Order { kRGB, kBGR, kArc };

class JImage {
public:
  JImage();
  explicit JImage(std::string im_path);
  JImage(int channel, int height, int width, Order order = kRGB);
  ~JImage();

  void Read(std::string im_path);
  void Write(std::string im_path);
  void Show(std::string show_name);
  void CopyTo(JImage *im_copy);
  void Resize(JImage *im_res, int height, int width);
  void Crop(JImage *im_crop, RectF crop);
  void CropWithResize(JImage *im_res, RectF crop, int height, int width);
  void Filter2D(float *kernel, int height, int width);

#ifdef USE_OpenCV
  void FromMat(const cv::Mat &im_mat);
  void FromMatWithCropResize(const cv::Mat &im_mat, RectF crop, int resize_h,
                             int resize_w, float *batch_data);
#endif

#ifdef USE_ArcSoft
  void FromArcImage(const ASVLOFFSCREEN &im_arc);
  void FromArcImageWithCropResize(const ASVLOFFSCREEN &im_arc, RectF crop,
                                  int resize_h, int resize_w,
                                  float *batch_data);
  void JImageToArcImage(int arc_format);
#endif

  void Rectangle(const VecBox &boxes, bool console_show = true);
  void GetBatchData(float *batch_data);

  void Release();

  int c_, h_, w_;

#ifdef USE_ArcSoft
  ASVLOFFSCREEN arc_image_;
  unsigned char *arc_data_;
#endif

private:
  void GetInv(unsigned char *im_inv);

  unsigned char *data_;
  Order order_;
};

#endif // SHADOW_JIMAGE_HPP
