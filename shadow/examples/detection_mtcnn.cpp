#include "detection_mtcnn.hpp"

namespace Shadow {

inline bool SortBoxesDescend(const BoxInfo &box_a, const BoxInfo &box_b) {
  return box_a.box.score > box_b.box.score;
}

inline float IoU(const BoxF &box_a, const BoxF &box_b, bool is_iom = false) {
  float inter = Boxes::Intersection(box_a, box_b);
  float size_a = Boxes::Size(box_a), size_b = Boxes::Size(box_b);
  if (is_iom) {
    return inter / std::min(size_a, size_b);
  } else {
    return inter / (size_a + size_b - inter);
  }
}

inline VecBoxInfo NMS(const VecBoxInfo &boxes, float threshold,
                      bool is_iom = false) {
  auto all_boxes = boxes;
  std::stable_sort(all_boxes.begin(), all_boxes.end(), SortBoxesDescend);
  for (int i = 0; i < all_boxes.size(); ++i) {
    auto &box_info_i = all_boxes[i];
    if (box_info_i.box.label == -1) continue;
    for (int j = i + 1; j < all_boxes.size(); ++j) {
      auto &box_info_j = all_boxes[j];
      if (box_info_j.box.label == -1) continue;
      if (IoU(box_info_i.box, box_info_j.box, is_iom) > threshold) {
        box_info_j.box.label = -1;
        continue;
      }
    }
  }
  VecBoxInfo out_boxes;
  for (const auto &box_info : all_boxes) {
    if (box_info.box.label != -1) {
      out_boxes.push_back(box_info);
    }
  }
  all_boxes.clear();
  return out_boxes;
}

void DetectionMTCNN::Setup(const VecString &model_files, const VecInt &classes,
                           const VecInt &in_shape) {
  net_p_.Setup();

  net_p_.LoadModel(model_files[0], VecInt{1, 3, 360, 360});
  net_r_.LoadModel(model_files[1], VecInt{50});
  net_o_.LoadModel(model_files[2], VecInt{20});

  net_p_in_shape_ = net_p_.in_shape();
  net_p_stride_ = 2, net_p_cell_size_ = 12;

  net_r_in_shape_ = net_r_.in_shape();
  net_r_in_c_ = net_r_in_shape_[1];
  net_r_in_h_ = net_r_in_shape_[2];
  net_r_in_w_ = net_r_in_shape_[3];
  net_r_in_num_ = net_r_in_c_ * net_r_in_h_ * net_r_in_w_;

  net_o_in_shape_ = net_o_.in_shape();
  net_o_in_c_ = net_o_in_shape_[1];
  net_o_in_h_ = net_o_in_shape_[2];
  net_o_in_w_ = net_o_in_shape_[3];
  net_o_in_num_ = net_o_in_c_ * net_o_in_h_ * net_o_in_w_;

  factor_ = 0.709f, max_side_ = 360, min_side_ = 40;
  thresholds_ = {0.6f, 0.6f, 0.7f};
}

void DetectionMTCNN::Predict(const JImage &im_src, const VecRectF &rois,
                             std::vector<VecBoxF> *Gboxes,
                             std::vector<std::vector<VecPointF>> *Gpoints) {
  Gboxes->clear(), Gpoints->clear();
  net_p_boxes_.clear(), net_r_boxes_.clear(), net_o_boxes_.clear();
  for (const auto &roi : rois) {
    float crop_h = roi.h <= 1 ? roi.h * im_src.h_ : roi.h;
    float crop_w = roi.w <= 1 ? roi.w * im_src.w_ : roi.w;
    CalculateScales(crop_h, crop_w, factor_, max_side_, min_side_, &scales_);

    for (auto scale : scales_) {
      auto scale_h = static_cast<int>(std::ceil(crop_h * scale));
      auto scale_w = static_cast<int>(std::ceil(crop_w * scale));
      net_p_in_shape_[2] = scale_w, net_p_in_shape_[3] = scale_h;
      net_p_in_data_.resize(1 * 3 * scale_w * scale_h);
      ConvertData(im_src, net_p_in_data_.data(), roi, 3, scale_h, scale_w, 1,
                  true);
      VecBoxInfo boxes;
      Process_net_p(net_p_in_data_.data(), net_p_in_shape_, thresholds_[0],
                    scale, &boxes);
      net_p_boxes_.insert(net_p_boxes_.end(), boxes.begin(), boxes.end());
    }
    net_p_boxes_ = NMS(net_p_boxes_, 0.7);
    BoxRegression(net_p_boxes_);
    Box2SquareWithConstrain(net_p_boxes_, crop_h, crop_w);
    if (net_p_boxes_.empty()) {
      Gboxes->push_back({});
      Gpoints->push_back({});
      continue;
    }

    net_r_in_shape_[0] = static_cast<int>(net_p_boxes_.size());
    net_r_in_data_.resize(net_r_in_shape_[0] * net_r_in_num_);
    for (int n = 0; n < net_p_boxes_.size(); ++n) {
      const auto &net_12_box = net_p_boxes_[n].box;
      ConvertData(im_src, net_r_in_data_.data() + n * net_r_in_num_,
                  net_12_box.RectFloat(), net_r_in_c_, net_r_in_h_, net_r_in_w_,
                  1, true);
    }
    Process_net_r(net_r_in_data_.data(), net_r_in_shape_, thresholds_[1],
                  net_p_boxes_, &net_r_boxes_);
    net_r_boxes_ = NMS(net_r_boxes_, 0.7);
    BoxRegression(net_r_boxes_);
    Box2SquareWithConstrain(net_r_boxes_, crop_h, crop_w);
    if (net_r_boxes_.empty()) {
      Gboxes->push_back({});
      Gpoints->push_back({});
      continue;
    }

    net_o_in_shape_[0] = static_cast<int>(net_r_boxes_.size());
    net_o_in_data_.resize(net_o_in_shape_[0] * net_o_in_num_);
    for (int n = 0; n < net_r_boxes_.size(); ++n) {
      const auto &net_24_box = net_r_boxes_[n].box;
      ConvertData(im_src, net_o_in_data_.data() + n * net_o_in_num_,
                  net_24_box.RectFloat(), net_o_in_c_, net_o_in_h_, net_o_in_w_,
                  1, true);
    }
    Process_net_o(net_o_in_data_.data(), net_o_in_shape_, thresholds_[2],
                  net_r_boxes_, &net_o_boxes_);
    BoxRegression(net_o_boxes_);
    net_o_boxes_ = NMS(net_o_boxes_, 0.7, true);
    BoxWithConstrain(net_o_boxes_, crop_h, crop_w);

    VecBoxF boxes;
    std::vector<VecPointF> points;
    for (const auto &box_info : net_o_boxes_) {
      boxes.push_back(box_info.box);
      VecPointF mark_points;
      for (int k = 0; k < 5; ++k) {
        mark_points.emplace_back(box_info.landmark[2 * k],
                                 box_info.landmark[2 * k + 1]);
      }
      points.push_back(mark_points);
    }
    Gboxes->push_back(boxes);
    Gpoints->push_back(points);
  }
}

#if defined(USE_OpenCV)
void DetectionMTCNN::Predict(const cv::Mat &im_mat, const VecRectF &rois,
                             std::vector<VecBoxF> *Gboxes,
                             std::vector<std::vector<VecPointF>> *Gpoints) {
  im_ini_.FromMat(im_mat, true);
  Predict(im_ini_, rois, Gboxes, Gpoints);
}
#endif

void DetectionMTCNN::Release() {
  net_p_.Release();
  net_r_.Release();
  net_o_.Release();
}

void DetectionMTCNN::Process_net_p(const float *data, const VecInt &in_shape,
                                   float threshold, float scale,
                                   VecBoxInfo *boxes) {
  net_p_.Reshape(in_shape);
  net_p_.Forward(data);

  const auto *loc_blob = net_p_.GetBlobByName<float>("conv4-2");
  const auto *loc_data = const_cast<BlobF *>(loc_blob)->cpu_data();
  const auto *conf_data = net_p_.GetBlobDataByName<float>("prob1");

  int out_h = loc_blob->shape(2), out_w = loc_blob->shape(3);
  int out_spatial_dim = out_h * out_w;

  boxes->clear();
  for (int i = 0; i < out_spatial_dim; ++i) {
    int h = i / out_w, w = i % out_w;
    float conf = conf_data[out_spatial_dim + i];
    if (conf > threshold) {
      float x_min = (net_p_stride_ * h) / scale;
      float y_min = (net_p_stride_ * w) / scale;
      float x_max = (net_p_stride_ * h + net_p_cell_size_ - 1) / scale;
      float y_max = (net_p_stride_ * w + net_p_cell_size_ - 1) / scale;
      BoxF box(x_min, y_min, x_max, y_max);
      BoxInfo box_info;
      box_info.box = box;
      box_info.box.score = conf;
      box_info.box.label = 1;
      for (int k = 0; k < 4; ++k) {
        box_info.box_reg[k] = loc_data[k * out_spatial_dim + i];
      }
      boxes->push_back(box_info);
    }
  }
  *boxes = NMS(*boxes, 0.5);
}

void DetectionMTCNN::Process_net_r(const float *data, const VecInt &in_shape,
                                   float threshold,
                                   const VecBoxInfo &net_12_boxes,
                                   VecBoxInfo *boxes) {
  net_r_.Reshape(in_shape);
  net_r_.Forward(data);

  const auto *loc_data = net_r_.GetBlobDataByName<float>("conv5-2");
  const auto *conf_data = net_r_.GetBlobDataByName<float>("prob1");

  boxes->clear();
  for (int b = 0; b < in_shape[0]; ++b) {
    int loc_offset = 4 * b;
    float conf = conf_data[2 * b + 1];
    if (conf > threshold) {
      BoxInfo box_info;
      box_info.box = net_12_boxes[b].box;
      box_info.box.score = conf;
      box_info.box.label = 1;
      for (int k = 0; k < 4; ++k) {
        box_info.box_reg[k] = loc_data[loc_offset + k];
      }
      boxes->push_back(box_info);
    }
  }
}

void DetectionMTCNN::Process_net_o(const float *data, const VecInt &in_shape,
                                   float threshold,
                                   const VecBoxInfo &net_24_boxes,
                                   VecBoxInfo *boxes) {
  net_o_.Reshape(in_shape);
  net_o_.Forward(data);

  const auto *loc_data = net_o_.GetBlobDataByName<float>("conv6-2");
  const auto *mark_data = net_o_.GetBlobDataByName<float>("conv6-3");
  const auto *conf_data = net_o_.GetBlobDataByName<float>("prob1");

  boxes->clear();
  for (int b = 0; b < in_shape[0]; ++b) {
    int loc_offset = 4 * b, mark_offset = 10 * b;
    float conf = conf_data[2 * b + 1];
    if (conf > threshold) {
      BoxInfo box_info;
      box_info.box = net_24_boxes[b].box;
      box_info.box.score = conf;
      box_info.box.label = 1;
      for (int k = 0; k < 4; ++k) {
        box_info.box_reg[k] = loc_data[loc_offset + k];
      }
      float box_h = box_info.box.ymax - box_info.box.ymin + 1,
            box_w = box_info.box.xmax - box_info.box.xmin + 1;
      for (int k = 0; k < 5; ++k) {
        box_info.landmark[2 * k] =
            mark_data[mark_offset + k] * box_w + box_info.box.xmin;
        box_info.landmark[2 * k + 1] =
            mark_data[mark_offset + k + 5] * box_h + box_info.box.ymin;
      }
      boxes->push_back(box_info);
    }
  }
}

void DetectionMTCNN::CalculateScales(float height, float width, float factor,
                                     float max_side, float min_side,
                                     VecFloat *scales) {
  scales->clear();
  float pr_scale = max_side / std::max(height, width);  // 12.f / min_side
  float pr_min = pr_scale * std::min(height, width);
  while (pr_min > 12) {
    scales->push_back(pr_scale);
    pr_scale *= factor, pr_min *= factor;
  }
}

void DetectionMTCNN::BoxRegression(VecBoxInfo &boxes) {
  for (auto &box_info : boxes) {
    auto &box = box_info.box;
    float box_h = box.ymax - box.ymin + 1, box_w = box.xmax - box.xmin + 1;
    box.xmin += box_info.box_reg[0] * box_w;
    box.ymin += box_info.box_reg[1] * box_h;
    box.xmax += box_info.box_reg[2] * box_w;
    box.ymax += box_info.box_reg[3] * box_h;
  }
}

void DetectionMTCNN::Box2SquareWithConstrain(VecBoxInfo &boxes, float height,
                                             float width) {
  for (auto &box_info : boxes) {
    auto &box = box_info.box;
    float box_h = box.ymax - box.ymin + 1, box_w = box.xmax - box.xmin + 1;
    float box_l = std::max(box_h, box_w);
    float x_min = box.xmin + (box_w - box_l) / 2;
    float y_min = box.ymin + (box_h - box_l) / 2;
    float x_max = x_min + box_l;
    float y_max = y_min + box_l;
    box.xmin = std::max(0.f, x_min);
    box.ymin = std::max(0.f, y_min);
    box.xmax = std::min(width, x_max);
    box.ymax = std::min(height, y_max);
  }
}

void DetectionMTCNN::BoxWithConstrain(VecBoxInfo &boxes, float height,
                                      float width) {
  for (auto &box_info : boxes) {
    auto &box = box_info.box;
    box.xmin = std::max(0.f, box.xmin);
    box.ymin = std::max(0.f, box.ymin);
    box.xmax = std::min(width, box.xmax);
    box.ymax = std::min(height, box.ymax);
  }
}

}  // namespace Shadow
