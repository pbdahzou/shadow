#ifndef SHADOW_BLOB_HPP
#define SHADOW_BLOB_HPP

#include "shadow/kernel.hpp"
#include "shadow/util/util.hpp"

#if defined(USE_CL)
#define BACKEND cl_mem
#else
#define BACKEND Dtype
#endif

template <typename Dtype>
class Blob {
 public:
  explicit Blob(const std::string &name = "") : name_(name) {}
  explicit Blob(int count, const std::string &name = "") : name_(name) {
    allocate_data(count);
  }
  explicit Blob(int count, const Dtype *data, const std::string &name = "")
      : name_(name) {
    allocate_data(count);
    set_data(data);
  }

  inline const BACKEND *data() const { return data_; }
  inline BACKEND *mutable_data() { return data_; }

  inline void allocate_data(int count) {
#if !defined(USE_CUDA) & !defined(USE_CL)
    data_ = new Dtype[count];
    on_gpu_ = false;

#else
    data_ = Kernel::MakeBuffer<BACKEND>(count, static_cast<Dtype *>(nullptr));
    on_gpu_ = true;
#endif
    if (shape_.size() == 0) add_shape(count);
  }

  inline void set_data(const Dtype *data) {
    if (data == nullptr) Fatal("Set data for blob is nullptr!");
#if !defined(USE_CUDA) & !defined(USE_CL)
    memcpy(data_, data, count() * sizeof(Dtype));
    on_gpu_ = false;

#else
    Kernel::WriteBuffer(count(), data, data_);
    on_gpu_ = true;
#endif
  }

  inline void read_data(Dtype *data) const {
    if (data == nullptr) Fatal("Read data for blob is nullptr!");
#if !defined(USE_CUDA) & !defined(USE_CL)
    memcpy(data, data_, count() * sizeof(Dtype));

#else
    Kernel::ReadBuffer(count(), data_, data);
#endif
  }

  inline void share_data(BACKEND *data) {
    if (data == nullptr) Fatal("Share data for blob is nullptr!");
    data_ = data;
    shared_ = true;
#if !defined(USE_CUDA) & !defined(USE_CL)
    on_gpu_ = false;

#else
    on_gpu_ = true;
#endif
  }

  inline void reshape(const VecInt &shape) {
    if (shape.size() == 0) return;
    int new_count = 1;
    for (int i = 0; i < shape.size(); ++i) new_count *= shape[i];
    if (new_count <= 0) Fatal("Shape is valid!");
    if (data_ == nullptr || new_count > count()) {
      clear();
      allocate_data(new_count);
    }
    set_shape(shape);
  }
  inline void reshape(int num = 1, int channels = 1, int height = 1,
                      int width = 1) {
    VecInt shape(4);
    shape[0] = num;
    shape[1] = channels;
    shape[2] = height;
    shape[3] = width;
    reshape(shape);
  }

  inline const std::string name() const { return name_; }
  inline void set_name(const std::string &name) { name_ = name; }

  inline const VecInt shape() const { return shape_; }
  inline VecInt *mutable_shape() { return &shape_; }

  inline const int shape(int index) const {
    if (index < 0 || index >= shape_.size()) {
      Fatal("Index out of blob shape range!");
    }
    return shape_[index];
  }
  inline void set_shape(int index, int value) {
    if (index < 0 || index >= shape_.size()) {
      Fatal("Index out of blob shape range!");
    }
    shape_[index] = value;
  }
  inline void set_shape(const VecInt &shape) { shape_ = shape; }
  inline void add_shape(int value) { shape_.push_back(value); }

  inline const int num_axes() const { return shape_.size(); }
  inline const int num() const { return count() / shape(0); }
  inline const int count() const { return count(0); }
  inline const int count(int start_axis) const {
    return count(start_axis, shape_.size() - 1);
  }
  inline const int count(int start_axis, int end_axis) const {
    if (start_axis < 0 || end_axis >= shape_.size()) {
      Fatal("Index out of blob shape range!");
    }
    if (start_axis > end_axis) return 0;
    int count = 1;
    for (int i = start_axis; i <= end_axis; ++i) count *= shape(i);
    return count;
  }

  inline void clear() {
    if (data_ != nullptr && !shared_) {
#if !defined(USE_CUDA) & !defined(USE_CL)
      delete[] data_;

#else
      Kernel::ReleaseBuffer(data_);
#endif
    }
    data_ = nullptr;
    shape_.clear();
  }

 private:
  BACKEND *data_ = nullptr;

  std::string name_ = "";
  VecInt shape_;
  bool on_gpu_ = false;
  bool shared_ = false;
};

typedef std::vector<Blob<float> *> VecBlob;

inline static Blob<float> *get_blob_by_name(const VecBlob &blobs,
                                            const std::string &name) {
  for (int i = 0; i < blobs.size(); ++i) {
    if (!name.compare(blobs.at(i)->name())) return blobs.at(i);
  }
  return nullptr;
}

#endif  // SHADOW_BLOB_HPP
