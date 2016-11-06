#include "shadow/layers/reshape_layer.hpp"

void ReshapeLayer::Setup(VecBlob *blobs) {
  Layer::Setup(blobs);

  CHECK_NE(tops_[0], bottoms_[0]);

  const shadow::ReshapeParameter &reshape_param = layer_param_.reshape_param();

  axis_ = reshape_param.axis();
  num_axes_ = reshape_param.num_axes();
  CHECK_GE(num_axes_, -1);

  inferred_axis_ = -1;
  copy_axes_.clear();
  constant_count_ = 1;
  for (int i = 0; i < reshape_param.shape().dim_size(); ++i) {
    int top_dim = reshape_param.shape().dim(i);
    if (top_dim == 0) {
      copy_axes_.push_back(i);
    } else if (top_dim == -1) {
      CHECK_EQ(inferred_axis_, -1);
      inferred_axis_ = i;
    } else {
      constant_count_ *= top_dim;
    }
  }
}

void ReshapeLayer::Reshape() {
  int start_axis = (axis_ >= 0) ? axis_ : (bottoms_[0]->num_axes() + axis_ + 1);
  CHECK_GE(start_axis, 0);
  CHECK_LE(start_axis, bottoms_[0]->num_axes());
  int end_axis =
      (num_axes_ == -1) ? bottoms_[0]->num_axes() : (start_axis + num_axes_);
  CHECK_LE(end_axis, bottoms_[0]->num_axes());
  int num_axes_replaced = end_axis - start_axis;
  int num_axes_retained = bottoms_[0]->num_axes() - num_axes_replaced;
  const shadow::BlobShape &shape = layer_param_.reshape_param().shape();
  VecInt top_shape(num_axes_retained + shape.dim_size());
  int top_shape_index = 0;
  for (int i = 0; i < start_axis; ++i) {
    top_shape[top_shape_index++] = bottoms_[0]->shape(i);
  }
  for (int i = 0; i < shape.dim_size(); ++i) {
    top_shape[top_shape_index++] = shape.dim(i);
  }
  for (int i = end_axis; i < bottoms_[0]->num_axes(); ++i) {
    top_shape[top_shape_index++] = bottoms_[0]->shape(i);
  }
  CHECK_EQ(top_shape_index, top_shape.size());
  for (int i = 0; i < copy_axes_.size(); ++i) {
    CHECK_GT(bottoms_[0]->num_axes(), start_axis + copy_axes_[i]);
    top_shape[start_axis + copy_axes_[i]] =
        bottoms_[0]->shape(start_axis + copy_axes_[i]);
  }
  if (inferred_axis_ >= 0) {
    int explicit_count = constant_count_;
    explicit_count *= bottoms_[0]->count(0, start_axis);
    explicit_count *= bottoms_[0]->count(end_axis);
    for (int i = 0; i < copy_axes_.size(); ++i) {
      explicit_count *= top_shape[start_axis + copy_axes_[i]];
    }
    CHECK_EQ(0, (bottoms_[0]->count() % explicit_count));
    top_shape[start_axis + inferred_axis_] =
        bottoms_[0]->count() / explicit_count;
  }
  tops_[0]->reshape(top_shape);
  CHECK_EQ(tops_[0]->count(), bottoms_[0]->count());

  std::stringstream out;
  out << layer_name_ << ": "
      << Util::format_vector(bottoms_[0]->shape(), ",", "(", ")") << " -> "
      << Util::format_vector(tops_[0]->shape(), ",", "(", ")");
  DInfo(out.str());
}

void ReshapeLayer::Forward() {
  tops_[0]->share_data(bottoms_[0]->mutable_data());
}

void ReshapeLayer::Release() {
  bottoms_.clear();
  tops_.clear();

  // DInfo("Free ReshapeLayer!");
}