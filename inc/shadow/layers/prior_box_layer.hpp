#ifndef SHADOW_LAYERS_PRIOR_BOX_LAYER_HPP
#define SHADOW_LAYERS_PRIOR_BOX_LAYER_HPP

#include "shadow/layers/layer.hpp"

class PriorBoxLayer : public Layer {
 public:
  PriorBoxLayer() {}
  explicit PriorBoxLayer(const shadow::LayerParameter &layer_param)
      : Layer(layer_param) {}
  ~PriorBoxLayer() { Release(); }

  void Setup(VecBlob *blobs);
  void Reshape();
  void Forward();
  void Release();

 private:
  int num_priors_;
  float min_size_, max_size_;
  bool flip_, clip_, is_initial_;
  VecFloat aspect_ratios_, variance_, top_data_;
};

#endif  // SHADOW_LAYERS_PRIOR_BOX_LAYER_HPP
