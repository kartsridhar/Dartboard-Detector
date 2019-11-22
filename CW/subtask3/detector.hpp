#ifndef DETECTOR_HPP
#define DETECTOR_HPP

#include <stdio.h>
#include <stdlib.h>
#include <opencv/cv.h>        //you may need to
#include <opencv/highgui.h>   //adjust import locations
#include <opencv/cxcore.h>    //depending on your machine setup
#include <math.h>

using namespace std;
using namespace cv;

#define MIN_VOTES 18

std::vector <Rect> draw_box(Mat original_img, int ***accumulator, Mat thresholded_hough, int max_radius)
{
    vector <Rect> houghoutput;
    std::vector<Point3i> xyr_vals;
    for(int y = 0; y < thresholded_hough.rows; y++)
    {
        for(int x = 0; x < thresholded_hough.cols; x++)
        {
            if(thresholded_hough.at<uchar>(y, x) == 255)
            {
                int argmax = 0, max = 0;
                for(int r = max_radius; r >= 0; r--)
                {
                    if(accumulator[y][x][r] > max)
                    {
                        max = accumulator[y][x][r];
                        argmax = r;
                    }
                }
                if(accumulator[y][x][argmax] > MIN_VOTES)
                {
                    xyr_vals.push_back(Point3i(y, x, argmax));
                    break;
                }
            }
        }
    }

    // Drawing the rectangles from the xyr_vals
    for(auto val : xyr_vals)
    {
        int r = val.z;
        Point p1 = Point(val.y - r, val.x - r);
        Point p2 = Point(val.y + r, val.x + r);

       cv::rectangle(original_img, p1, p2, Scalar(0, 255, 0), 2);
    //    houghoutput.push_back(Rect(p1.y,p1.x,abs(p2.y-p1.y),abs(p2.x-p1.x))) ;
               houghoutput.push_back(Rect(p1.x,p1.y,abs(p2.x-p1.x),abs(p2.y-p1.y))) ;
       std::cout << "houghoutput x in detector is : " << houghoutput[0].x << std::endl;
    }
    return houghoutput;
}
#endif