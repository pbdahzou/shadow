#include "yolo.hpp"

int main(int argc, char const *argv[]) {

  Yolo yolo("./cfg/yolo-refine.conv_adas.json",
            "./models/df/ex2/yolo-refine_80000.weights", 0.2);

  VecBox roi(1);
  roi[0].x = 0.5;
  roi[0].y = 0.2;
  roi[0].w = 0.45;
  roi[0].h = 0.5;
  yolo.Setup(1, nullptr);
  yolo.Test("./data/demo_6.png");
  //yolo.BatchTest("./data/testlist.txt", true);
  //yolo.VideoTest("./data/traffic/set07V000.avi", true, false);
  //yolo.Demo(0, true);
  yolo.Release();
  return 0;
}
