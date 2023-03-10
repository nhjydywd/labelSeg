# labelSeg

#### Description

LabelSeg is a annotation tool for semantic segmentation, which aims to greatly reduce the time required for annotation while obtaining high-quality annotation images through automatic annotation and a small amount of manual correction. Automatic labeling is accomplished through a trained convolution neural network.

At present, the software only supports the annotation of semantic segmentation of two categories (i.e. 0-1 label).

It supports both Chinese and English, and can be switched in the menu bar.

Labeling process: After opening the picture, click the "Auto Labeling" button in the lower right corner to generate a preliminary automatic labeling (as shown below). You can use the mouse as a paintbrush to modify and correct on the annotation map, draw with the left button, and erase with the right button.

![输入图片说明](example.png)

Shortcut keys：

CTRL+S: Save

CTRL+Z: Withdraw

I: Increase brush size

K: Reduce brush size

↑: Previous picture

↓: Next picture

Wheel Up: Previous picture

Wheel Down: Next picture




#### Neural network model

Automatic labeling is accomplished through a trained convolution neural network. The default model is' OCTA_ FRNet ', which is a vascular segmentation model for OCTA images.

If you need to use your own model, please export it to the onnx format, and then import the onnx file into the '/models' path of the project's resource file.



####Dependency

Qt 6.4

OpenCV 4



#### Run

1.  Open the project using Qt Creator

2.  In CMakeLists.txt, replace

```
set(OpenCV_DIR C:/Libs/Opencv4/opencv/build/x64/vc15/lib)
```
to
```
set(OpenCV_DIR YOUR_OPENCV_PATH)
```

3. Compile and run in Release mode



