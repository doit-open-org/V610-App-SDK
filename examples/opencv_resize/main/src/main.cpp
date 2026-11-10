/*
 * OpenCV: draw a test picture, scale it down and
 * write both as JPEG.
 *
 *   opencv_resize [output directory]
 */
#include <cstdio>
#include <string>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

int main(int argc, char **argv)
{
    const std::string dir = argc > 1 ? argv[1] : "/tmp";
    cv::Mat picture(480, 640, CV_8UC3, cv::Scalar(48, 40, 32));
    cv::Mat small;

    cv::rectangle(picture, cv::Point(40, 40), cv::Point(600, 440), cv::Scalar(0, 200, 255), 4);
    cv::circle(picture, cv::Point(320, 240), 120, cv::Scalar(255, 160, 0), cv::FILLED);
    cv::putText(picture, "Cove-610", cv::Point(210, 250), cv::FONT_HERSHEY_SIMPLEX, 1.2,
                cv::Scalar(255, 255, 255), 2);
    cv::resize(picture, small, cv::Size(160, 120), 0, 0, cv::INTER_AREA);

    std::printf("OpenCV %s\n", CV_VERSION);
    if (!cv::imwrite(dir + "/opencv_full.jpg", picture) || !cv::imwrite(dir + "/opencv_small.jpg", small)) {
        std::fprintf(stderr, "cannot write to %s\n", dir.c_str());
        return 1;
    }
    std::printf("wrote %s/opencv_full.jpg (%dx%d) and opencv_small.jpg (%dx%d)\n", dir.c_str(),
                picture.cols, picture.rows, small.cols, small.rows);
    return 0;
}
