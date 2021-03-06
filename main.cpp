// header inclusion
#include <stdio.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <opencv/cv.h>        //you may need to
#include <opencv/highgui.h>   //adjust import locations
#include <opencv/cxcore.h>    //depending on your machine setup
#include "hpp/detector.hpp"
#include "hpp/hough.hpp"
#include "hpp/sobel.hpp"
#include "hpp/ellipse.hpp"
#include "hpp/line.hpp"
#define BoxDistance 30

using namespace cv;
using namespace std;

/** Function Headers */
vector <Rect> detectAndDisplay1(Mat frame);

/** Function to store the Ground Truths*/
std::vector<Rect> ground_darts(int n);

/** Function to Calculate F1-Score */
void f1_score(std::vector<Rect> approved_viola, int img);

/** Function combining Viola and Hough */
std::vector<Rect> viola_hough(Mat img, std::vector<Rect> viola_detected, std::vector<Point> hough_centers, std::vector<Point> line_intersections);

/** Function that draws the best ellipses*/
void draw_best_detected(cv::Mat original_img, 
            std::vector<Rect> hough, std::vector<Rect> ellipses);

/** Global variables */
String cascade_name = "cascade.xml";
CascadeClassifier cascade;
 
/** @function main */
int main(int argc, const char** argv)
{
    // 1. Read Input Image
    Mat frame = imread(argv[1], CV_LOAD_IMAGE_COLOR);
    Mat oldframe = imread(argv[1], CV_LOAD_IMAGE_COLOR);
    Mat lastoldframe = imread(argv[1], CV_LOAD_IMAGE_COLOR);
    Mat ellipseoldframe = imread(argv[1], CV_LOAD_IMAGE_COLOR);
	Mat allframe = imread(argv[1], CV_LOAD_IMAGE_COLOR);
	Mat lineframe = imread(argv[1], CV_LOAD_IMAGE_COLOR);

    // 2. Load the Strong Classifier in a structure called `Cascade'
    if (!cascade.load(cascade_name)) { printf("--(!)Error loading cascade\n"); return -1; };
 
    // 3. Detect Faces and Display Result
    vector <Rect> violaoutput = detectAndDisplay1(frame);
 
    // 4. Save Result Image
    imwrite("cascade_detected.jpg", frame);
 
    // Hough Transform stuff starts here
    Mat gray_image;
    cvtColor(oldframe, gray_image, CV_BGR2GRAY);

 
    Mat mag_img(gray_image.rows, gray_image.cols, CV_32FC1, Scalar(0));
    Mat dir_img(gray_image.rows, gray_image.cols, CV_32FC1, Scalar(0));
 
    // Performing Sobel Edge detection, getting the magnitude and direction imgs
    sobel(gray_image, mag_img, dir_img);
    cv::imwrite("magnew.jpg", mag_img);
    cv::imwrite("dirnew.jpg", dir_img);
    Mat magnitude_img = imread( "magnew.jpg", 0 );
 
    // Storing the unnormalised direction
    Mat unnormalised_dir = dir_img;
 
    // Thresholding the magnitude
    Mat thresholded_mag = thresholdd(normalise(magnitude_img), 140);

    cv::imwrite("thresholded_mag.jpg", thresholded_mag);
	
	// Hough Lines
	Mat hough_lines = line_detection(thresholded_mag, unnormalised_dir);
	std::vector<Point> line_intersections = get_intersection_points(hough_lines);
	cv::imwrite("hough_lines.jpg", hough_lines);

    // Creating the hough space, assuming 0 rotation.
    // Min radius and Max radius 40 and 115 respectively
    int ***hough_space = create_hough_space(thresholded_mag, unnormalised_dir, 40, 150);
 
    // Generating the hough image
    Mat hough_img = view_hough_space(hough_space, thresholded_mag, 40, 150);
    Mat final_hough(mag_img.rows, mag_img.cols, CV_8UC1, Scalar(0));
 
    cv::normalize(hough_img, final_hough, 0, 255, NORM_MINMAX);
    cv::imwrite("normalised_hough_img.jpg", final_hough);
 
    // Reading in normalised hough image to ensure values are uchar instead of float
    Mat new_hough_img = imread("normalised_hough_img.jpg", 0);
 
    // Thresholding the newly read hough image
    Mat thresholded_hough = thresholdd(new_hough_img, 140);

    cv::imwrite("thresholded_hough.jpg", thresholded_hough);

    // Drawing the box around the detected stuff
    std::vector <Rect> hough_output;
	std::vector<Point> hough_centers;
    hough_output = draw_box(oldframe, hough_space, thresholded_hough, 150, hough_centers);

	// Getting the Ellipses 
    std::vector<RotatedRect> ellipses = ellipse_detector(magnitude_img, dir_img);
    std::vector<Rect> ellipse_output = convert_rotated_rect(ellipses);
    
	// Combining Viola and Hough Detections
	std::vector<Rect> approved_viola = viola_hough(allframe, violaoutput, hough_centers, line_intersections);
	std::vector<Rect> approved_viola_filtered;
	std::vector<Rect> approved_viola_final;

	// Filtering out duplicate hits in the approved viola detections
	approved_viola.erase(unique(approved_viola.begin(),approved_viola.end()),approved_viola.end());
	int **viola_lookup = create2DArray(approved_viola.size(),approved_viola.size());
	for (int i=0; i < approved_viola.size(); i++) 
	{
		for (int j=0; j<approved_viola.size(); j++)
		{
			viola_lookup[i][j] = -1;
		}
	}	

	//Now calculate whether intersection has taken place - 2 for loops iterate through viola.size
	for (int i=0;i<approved_viola.size();i++){
		for (int j=0;j<approved_viola.size();j++){
			if (i != j){
				if ((viola_lookup[i][j] == -1) && (viola_lookup[j][i]==-1)){
				if ((approved_viola[i] & approved_viola[j]).area()>0){
					viola_lookup[i][j] = 1;
					viola_lookup[j][i] = 1;
					approved_viola_filtered.push_back(approved_viola[i]);

				}
				else {
					viola_lookup[i][j] = 0;
					viola_lookup[j][i] = 0;
				}
				}
			}
		}
	}
	for (int i=0;i<approved_viola.size();i++){
		for (int j=0;j<approved_viola_filtered.size();j++){
			if (approved_viola[i] == approved_viola_filtered[j]){
				approved_viola.erase(std::remove(approved_viola.begin(),approved_viola.end(),approved_viola[i]),approved_viola.end());
			}

		}
		approved_viola_final = approved_viola;
	}

	// // Trying ellipse stuff for F1 here
	// std::vector<Point> ellipse_centers;
	// for(int i = 0; i < ellipse_output.size(); i++)
	// {	
	// 	Point center = (ellipse_output[i].tl() + ellipse_output[i].br()) * 0.5;
	// 	ellipse_centers.push_back(center);
	// }
	
	// std::vector<Rect> approved_viola_ellipse;
	// for(int i = 0; i < violaoutput.size(); i++)
	// {
	// 	Rect r = violaoutput[i];
	// 	// printf("here2\n");
	// 	Point center = (r.tl() + r.br()) * 0.5;
	// 	// printf("%d\n", line_intersections.size());
	// 	for(int l = 0; l < ellipse_centers.size(); l++)
	// 	{	
	// 		double distance = cv::norm(center - ellipse_centers[l]);
	// 		if(distance < 10 && std::find(approved_viola_final.begin(), approved_viola_final.end(), violaoutput[i]) != approved_viola_final.end())
	// 		{
	// 			approved_viola_ellipse.push_back(violaoutput[i]);
	// 		}
	// 		// circle(img, line_intersections[l], 1, Scalar(245, 66, 197), 2);
	// 	}
	// }

	// for(int i = 0; i < approved_viola_ellipse.size(); i++)
	// {
	// 	std::cout << "value of i in approved vio is : " << approved_viola_ellipse[i]<< std::endl;
	// 	rectangle(allframe, approved_viola_ellipse[i], Scalar(0, 255, 0), 2);
	// }

	for(int i = 0; i < approved_viola_final.size(); i++)
	{
		rectangle(allframe, approved_viola_final[i], Scalar(0, 255, 0), 2);
	}

	const int img_num = 0;

	for(int i = 0; i < ground_darts(img_num).size(); i++)
	{
		rectangle(allframe, ground_darts(img_num)[i], Scalar(0, 0, 255), 2);
	}

	f1_score(approved_viola_final, img_num);
	// f1_score(approved_viola_ellipse, img_num);

	cv::imwrite("detected.jpg", allframe);
    return 0;
}
 
/** @function detectAndDisplay */
vector <Rect> detectAndDisplay1(Mat frame)
{
    std::vector<Rect> faces;
    std::vector<Rect> vout;
    Mat frame_gray;
 
    // 1. Prepare Image by turning it into Grayscale and normalising lighting
    cvtColor(frame, frame_gray, CV_BGR2GRAY);
    equalizeHist(frame_gray, frame_gray);
 
    // 2. Perform Viola-Jones Object Detection
    cascade.detectMultiScale(frame_gray, faces, 1.1, 1, 0 | CV_HAAR_SCALE_IMAGE, Size(50, 50), Size(500, 500));
 
    // 3. Print number of Faces found
    std::cout << faces.size() << std::endl;
 
    // 4. Draw box around faces found
    for (int i = 0; i < faces.size(); i++)
    {
        rectangle(frame, Point(faces[i].x, faces[i].y), Point(faces[i].x + faces[i].width, faces[i].y + faces[i].height), Scalar(0, 255, 0), 2);  
    }
    return faces;
}

/** Function to get the detected rectangles*/
std::vector<Rect> detected_darts(int n) {
	std::vector<Rect> result;
	switch (n)
	{
	case 0: result.push_back(Rect(536, 288, 58, 58));
		result.push_back(Rect(146, 345, 52, 52));
		result.push_back(Rect(314, 82, 57, 57));
		result.push_back(Rect(578, 184, 57, 57));
		result.push_back(Rect(291, 202, 57, 57));
		result.push_back(Rect(516, 259, 62, 62));
		result.push_back(Rect(286, 84, 66, 66));
		result.push_back(Rect(306, 114, 69, 69));
		result.push_back(Rect(522, 216, 71, 71));
		result.push_back(Rect(334, 131, 76, 76));
		result.push_back(Rect(128, 186, 76, 76));
		result.push_back(Rect(455, 47, 134, 134));
		result.push_back(Rect(465, 10, 101, 101));
		result.push_back(Rect(394, 22, 179, 179));
		break;
	case 1: result.push_back(Rect(292, 313, 61, 61));
		result.push_back(Rect(286, 73, 63, 63));
		result.push_back(Rect(208, 138, 179, 179));
		result.push_back(Rect(264, 247, 135, 135));
		result.push_back(Rect(246, 59, 159, 159));
		break;
	case 2: result.push_back(Rect(241, 42, 54, 54));
		result.push_back(Rect(285, 46, 60, 60));
		result.push_back(Rect(321, 56, 53, 53));
		result.push_back(Rect(56, 172, 52, 52));
		result.push_back(Rect(144, 194, 54, 54));
		result.push_back(Rect(151, 247, 57, 57));
		result.push_back(Rect(117, 252, 52, 52));
		result.push_back(Rect(84, 79, 124, 124));
		result.push_back(Rect(45, 253, 65, 65));
		result.push_back(Rect(268, 95, 208, 208));
		result.push_back(Rect(404, 15, 197, 197));
		break;
	case 3: result.push_back(Rect(330, 158, 62, 62));
		result.push_back(Rect(202, 24, 94, 94));
		result.push_back(Rect(252, 74, 65, 65));
		result.push_back(Rect(374, 52, 84, 84));
		result.push_back(Rect(56, 70, 80, 80));
		result.push_back(Rect(298, 100, 135, 135));
		result.push_back(Rect(198, 8, 156, 156));
		break;
	case 4: result.push_back(Rect(557, 205, 54, 54));
		result.push_back(Rect(498, 328, 54, 54));
		result.push_back(Rect(484, 357, 54, 54));
		result.push_back(Rect(306, 466, 52, 52));
		result.push_back(Rect(156, 604, 63, 63));
		result.push_back(Rect(551, 70, 65, 65));
		result.push_back(Rect(93, 236, 65, 65));
		result.push_back(Rect(516, 373, 71, 71));
		result.push_back(Rect(84, 275, 124, 124));
		result.push_back(Rect(404, 26, 138, 138));
		result.push_back(Rect(180, 114, 184, 184));
		result.push_back(Rect(342, 328, 142, 142));
		result.push_back(Rect(146, 26, 160, 160));
		result.push_back(Rect(229, 10, 189, 189));
		result.push_back(Rect(82, 0, 333, 333)); 
		break;
	case 5: result.push_back(Rect(357, 44, 52, 52));
		result.push_back(Rect(291, 50, 54, 54));
		result.push_back(Rect(534, 90, 52, 52));
		result.push_back(Rect(261, 355, 52, 52));
		result.push_back(Rect(375, 364, 54, 54));
		result.push_back(Rect(252, 56, 61, 61));
		result.push_back(Rect(332, 92, 63, 63));
		result.push_back(Rect(67, 242, 60, 60));
		result.push_back(Rect(354, 277, 57, 57));
		result.push_back(Rect(386, 167, 101, 101));
		result.push_back(Rect(425, 134, 130, 130));
		result.push_back(Rect(634, 376, 142, 142));
		break;
	case 6: result.push_back(Rect(209, 103, 76, 76));
		result.push_back(Rect(154, 200, 52, 52));
		result.push_back(Rect(230, 330, 53, 53));
		result.push_back(Rect(189, 99, 67, 67));
		result.push_back(Rect(246, 161, 66, 66));
		result.push_back(Rect(114, 274, 63, 63));
		result.push_back(Rect(400, 28, 80, 80));
		result.push_back(Rect(352, 228, 92, 92));
		break;
	case 7: result.push_back(Rect(447, 95, 54, 54));
		result.push_back(Rect(451, 118, 56, 56));
		result.push_back(Rect(489, 262, 52, 52));
		result.push_back(Rect(619, 512, 53, 53));
		result.push_back(Rect(582, 498, 58, 58));
		result.push_back(Rect(633, 536, 57, 57));
		result.push_back(Rect(490, 229, 63, 63));
		result.push_back(Rect(146, 548, 70, 70));
		result.push_back(Rect(49, 559, 66, 66));
		result.push_back(Rect(193, 385, 69, 69));
		result.push_back(Rect(382, 343, 84, 84));
		result.push_back(Rect(176, 496, 96, 96));
		result.push_back(Rect(358, 474, 135, 135));
		result.push_back(Rect(224, 181, 146, 146));
		result.push_back(Rect(171, 151, 219, 219));
		break;
	case 8: result.push_back(Rect(256, 444, 53, 53));
		result.push_back(Rect(227, 447, 59, 59));
		result.push_back(Rect(498, 515, 55, 55));
		result.push_back(Rect(183, 638, 52, 52));
		result.push_back(Rect(266, 669, 52, 52));
		result.push_back(Rect(348, 684, 58, 58));
		result.push_back(Rect(240, 702, 52, 52));
		result.push_back(Rect(659, 203, 67, 67));
		result.push_back(Rect(863, 234, 82, 82));
		result.push_back(Rect(520, 540, 63, 63));
		result.push_back(Rect(275, 551, 62, 62));
		result.push_back(Rect(696, 594, 60, 60));
		result.push_back(Rect(209, 602, 61,61));
		result.push_back(Rect(232, 630, 60, 60));
		result.push_back(Rect(54, 229, 67, 67));
		result.push_back(Rect(62, 581, 63, 63));
		result.push_back(Rect(195, 680, 65, 65));
		result.push_back(Rect(566, 169, 69, 69));
		result.push_back(Rect(310, 585, 76, 76));
		result.push_back(Rect(326, 595, 76, 76));
		result.push_back(Rect(33, 255, 88, 88));
		result.push_back(Rect(30, 354, 80, 80));
		result.push_back(Rect(311, 394, 86, 86));
		result.push_back(Rect(245, 558, 88, 88));
		result.push_back(Rect(237, 672, 87, 87));
		result.push_back(Rect(37, 518, 102, 102));
		result.push_back(Rect(43, 194, 118, 118));
		result.push_back(Rect(799, 533, 142, 142));
		result.push_back(Rect(655, 118, 148, 148));
		result.push_back(Rect(782, 220, 176, 176));
		result.push_back(Rect(24, 204, 163, 163));
		result.push_back(Rect(284, 390, 171, 171));
		break;
	case 10: result.push_back(Rect(459, 54, 66, 66));
		result.push_back(Rect(905, 125, 62, 62));
		result.push_back(Rect(561, 307, 54, 54));
		result.push_back(Rect(134, 348, 52, 52));
		result.push_back(Rect(79, 355, 52, 52));
		result.push_back(Rect(557, 477, 58, 58));
		result.push_back(Rect(89, 516, 55, 55));
		result.push_back(Rect(48, 529, 52, 52));
		result.push_back(Rect(383, 564, 55, 55));
		result.push_back(Rect(949, 614, 52, 52));
		result.push_back(Rect(942, 637, 54, 54));
		result.push_back(Rect(834, 656, 61, 61));
		result.push_back(Rect(770, 658, 60, 60));
		result.push_back(Rect(556, 670, 52, 52));
		result.push_back(Rect(742, 666, 55, 55));
		result.push_back(Rect(571, 94, 81, 81));
		result.push_back(Rect(95, 543, 62, 62));
		result.push_back(Rect(585, 660, 61, 61));
		result.push_back(Rect(48, 215, 91, 91));
		result.push_back(Rect(196, 470, 66, 66));
		result.push_back(Rect(772, 639, 65, 65));
		result.push_back(Rect(122, 532, 69, 69));
		result.push_back(Rect(442, 67, 101, 101));
		result.push_back(Rect(573, 110, 102, 102));
		result.push_back(Rect(167, 515, 80, 80));
		result.push_back(Rect(703, 622, 76, 76));
		result.push_back(Rect(450, 496, 92, 92));
		result.push_back(Rect(59, 48, 158, 158));
		result.push_back(Rect(519, 134, 123, 123));
		result.push_back(Rect(22, 110, 149, 149));
		result.push_back(Rect(778, 56, 172, 172));
		result.push_back(Rect(386, 468, 180, 180));
		break;
	case 9: result.push_back(Rect(105, 369, 58, 58));
		result.push_back(Rect(92, 408, 54, 54));
		result.push_back(Rect(143, 258, 80, 80));
		result.push_back(Rect(120, 408, 93, 93));
		result.push_back(Rect(55, 190, 92, 92));
		result.push_back(Rect(140, 448, 96, 96));
		result.push_back(Rect(287, 475, 106, 106));
		result.push_back(Rect(236, 58, 204, 204));
		break;
	case 11: result.push_back(Rect(32, 8, 57, 57));
		result.push_back(Rect(350, 48, 74, 74));
		result.push_back(Rect(164, 83, 76, 76));
		result.push_back(Rect(278, 164, 72, 72));
		result.push_back(Rect(80, 22, 180, 180));
		break;
	case 12: result.push_back(Rect(376, 98, 54, 54));
		result.push_back(Rect(377, 127, 52, 52));
		result.push_back(Rect(84, 4, 63, 63));
		result.push_back(Rect(156, 204, 66, 66));
		result.push_back(Rect(127, 125, 106, 106));
		result.push_back(Rect(141, 96, 98, 98));
		result.push_back(Rect(115, 40, 174, 174));
		break;
	case 13: result.push_back(Rect(385, 131, 59, 59));
		result.push_back(Rect(344, 174, 70, 70));
		result.push_back(Rect(242, 197, 65, 65));
		result.push_back(Rect(306, 136, 104, 104));
		result.push_back(Rect(30, 60, 109, 109));
		result.push_back(Rect(30, 118, 101, 101));
		result.push_back(Rect(156, 120, 131, 131));
		result.push_back(Rect(18, 21, 149, 149));
		result.push_back(Rect(293, 148, 149, 149));
		result.push_back(Rect(247, 13, 271, 271));
		break;
	case 14: result.push_back(Rect(320, 62, 54, 54));
		result.push_back(Rect(323, 128, 54, 54));
		result.push_back(Rect(982, 69, 149, 149));
		result.push_back(Rect(1171, 202, 52, 52));
		result.push_back(Rect(660, 302, 54, 54));
		result.push_back(Rect(205, 334, 55, 55));
		result.push_back(Rect(354, 350, 83, 83));
		result.push_back(Rect(390, 460, 55, 55));
		result.push_back(Rect(1158, 464, 58, 58));
		result.push_back(Rect(1130, 536, 54, 54));
		result.push_back(Rect(33, 563, 57, 57));
		result.push_back(Rect(948, 102, 63, 63));
		result.push_back(Rect(995, 323, 60, 60));
		result.push_back(Rect(1056, 325, 63, 63));
		result.push_back(Rect(1001, 444, 60, 60));
		result.push_back(Rect(1130, 448, 57, 57));
		result.push_back(Rect(9, 456, 62, 62));
		result.push_back(Rect(42, 472, 62, 62));
		result.push_back(Rect(706, 506, 60, 60));
		result.push_back(Rect(323, 192, 64, 64));
		result.push_back(Rect(1155, 276, 63, 63));
		result.push_back(Rect(136, 511, 70, 70));
		result.push_back(Rect(532, 45, 71, 71));
		result.push_back(Rect(946, 141, 72, 72));
		result.push_back(Rect(28, 517, 72, 72));
		result.push_back(Rect(1124, 537, 72, 72));
		result.push_back(Rect(1025, 165, 96, 96));
		result.push_back(Rect(1116, 504, 99, 99));
		result.push_back(Rect(512, 113, 131, 131));
		result.push_back(Rect(529, 199, 122, 122));
		result.push_back(Rect(326, 316, 138, 138));
		result.push_back(Rect(455, 365, 128, 128));
		result.push_back(Rect(414, 75, 155, 155));
		result.push_back(Rect(91, 49, 233, 233));
		result.push_back(Rect(464, 26, 207, 207));
		result.push_back(Rect(599, 38, 263, 263));
		result.push_back(Rect(104, 317, 281, 281));
		break;
	case 15: result.push_back(Rect(36, 219, 69, 69));
		result.push_back(Rect(186, 55, 87, 87));
		result.push_back(Rect(155, 56, 173, 173));
		break;
	default: return result;
	}
	return result;
}

/** Function to get the ground truth rectangles*/
std::vector<Rect> ground_darts(int n) {
	std::vector<Rect> result;
	switch (n)
	{
	case 0: result.push_back(Rect(443, 14, (598 - 443), (193 - 14))); break; 
	case 1: result.push_back(Rect(197, 133, (391 - 197), (323 - 133))); break; 
	case 2: result.push_back(Rect(90, 83, (204 - 90), (198 - 83))); break; 
	case 3: result.push_back(Rect(325, 148, (388 - 325), (219 - 148))); break; 
	case 4: result.push_back(Rect(185, 97, (373 - 185), (291 - 97))); break;
	case 5: result.push_back(Rect(435, 140, (530 - 435), (246 - 140))); break;
	case 6: result.push_back(Rect(205, 108, (282 - 205), (189 - 108))); break; 
	case 7: result.push_back(Rect(255, 172, (389 - 255), (314 - 172))); break; 
	case 8: result.push_back(Rect(846, 218, (959 - 846), (337 - 218)));
			result.push_back(Rect(67, 251, (126 - 67), (340 - 251))); break;  
	case 9: result.push_back(Rect(204, 49, (436 - 204), (278 - 49))); break; 
	case 10: result.push_back(Rect(91, 103, (188 - 91), (214 - 103)));
			result.push_back(Rect(585, 129, (638 - 585), (212 - 129)));
			result.push_back(Rect(916, 152, (952 - 916), (212 - 152))); break; 
	case 11: result.push_back(Rect(168, 92, (241 - 168), (154 - 92))); break; 
	case 12: result.push_back(Rect(140, 93, (241 - 140), (209 - 93))); break; 
	case 13: result.push_back(Rect(287, 140, (402 - 287), (249 - 140))); break;
	case 14: result.push_back(Rect(121, 101, (247 - 121), (227 - 101)));
		result.push_back(Rect(989, 96, (1110 - 989), (218 - 96)));
		break;
	case 15: result.push_back(Rect(155, 57, (290 - 155), (199 - 57))); break;
	default: return result;
	}
	return result;
}

std::vector<Rect> viola_hough(Mat img, std::vector<Rect> viola_detected, std::vector<Point> hough_centers, std::vector<Point> line_intersections)
{
	std::vector<Rect> approved_viola;		// storing best Viola from hough circles
	std::vector<Rect> approved_viola2;		// storing best Viola from hough lines after hough circles
	
	/*
		First go through all violas
		Calculate distance of hough centers from viola centers
		Choose the ones where distance is < 30

		Loop through the chosen violas
		Calculate distance of line intersections from viola centers
		Choose the ones where distance < 20

		if the latter is empty, choose the first one
	*/
	for(int i = 0; i < viola_detected.size(); i++)
	{
		Rect r = viola_detected[i];
		Point center = (r.tl() + r.br()) * 0.5;

		// circle(img, center, 1, Scalar(255, 255, 0), 2);
		for(int j = 0; j < hough_centers.size(); j++)
		{	
			double distance = cv::norm(center - hough_centers[j]);
			if(distance < BoxDistance)
			{
				approved_viola.push_back(viola_detected[i]);
				// circle(img, hough_centers[j], 1, Scalar(255, 0, 0), 2);
			}
		}
	}
	for(int k = 0; k < approved_viola.size(); k++)
	{
		Rect r = approved_viola[k];
		Point center = (r.tl() + r.br()) * 0.5;
		for(int l = 0; l < line_intersections.size(); l++)
		{
			double distance = cv::norm(center - line_intersections[l]);
			if(distance < 20)
			{
				approved_viola2.push_back(approved_viola[k]);
				// circle(img, line_intersections[l], 1, Scalar(245, 66, 197), 2);
			}
		}
	}

	if(approved_viola2.size() == 0)
	{	
		if (approved_viola.size() != 0)
		{
			std::cout << "Chosen Hough = CIRCLES" << std::endl;
			return approved_viola;
		}
		else {
			for(int k = 0; k < viola_detected.size(); k++)
			{	
				Rect r = viola_detected[k];
				Point center = (r.tl() + r.br()) * 0.5;
				for(int l = 0; l < line_intersections.size(); l++)
				{	
					double distance = cv::norm(center - line_intersections[l]);
					if(distance < 20)
					{
						approved_viola2.push_back(viola_detected[k]);
					}
					// circle(img, line_intersections[l], 1, Scalar(245, 66, 197), 2);
				}
			}
			if(approved_viola2.size() == 0)
			{
				std::cout << "Chosen Hough = ORIGINAL VIOLA" << std::endl;
				return viola_detected;
			} 
			else
			{
				std::cout << "Chosen Hough = LINES ONLY" << std::endl;
				return approved_viola2;
			} 
		}
	}
	else
	{	
		std::cout << "Chosen Hough = LINES" << std::endl;
		return approved_viola2;
	}
}

void f1_score(std::vector<Rect> approved_viola, int img)
{
    std::vector<Rect> ground = ground_darts(img);
    float actual_hits = 0;
 
    for (int d = 0; d < approved_viola.size(); ++d)
    {
        for (int g = 0; g < ground.size(); ++g)
        {  
            float _intersection = (ground[g] & approved_viola[d]).area();
            float _union = (ground[g] | approved_viola[d]).area();
            float iou = _intersection/_union;
            if(iou > 0.3)
            {
                actual_hits += 1;
            }
        }
    }
    float tpr = actual_hits / ground.size();
    float fpr = 1 - tpr;
    float fnr = actual_hits - ground.size();
    float precision = actual_hits / approved_viola.size();
    float f1 = 2 * tpr * precision / (precision + tpr);
 
    std::cout << "Actual Faces: " << ground.size() << std::endl;
    std::cout << "Detected Faces: " << approved_viola.size() << std::endl;
    std::cout << "Actual Hits: " << actual_hits << std::endl;
    std::cout << "TPR: " << tpr << std::endl;
    std::cout << "FPR: " << fpr << std::endl;
    std::cout << "FNR: " << fnr << std::endl;
    std::cout << "F1-Score: " << f1 << std::endl;
}