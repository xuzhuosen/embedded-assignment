#include <cstdlib>
#include <iostream>
#include <vector>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>

#include "GPIOlib.h"
#define PI 3.1415926

//Uncomment this line at run-time to skip GUI rendering
#define _DEBUG

using namespace cv;
using namespace std;
using namespace GPIO;

const string CAM_PATH="/dev/video0";
const string MAIN_WINDOW_NAME="Processed Image";
const string CANNY_WINDOW_NAME="Canny";

const int CANNY_LOWER_BOUND=50;
const int CANNY_UPPER_BOUND=250;
const int HOUGH_THRESHOLD=150;

const int MAINTAIN = 20;
const float COE = -0.5;
const int STEP = 5;
struct Pid {
	float setDis;
	float actAng;
	float err;
	float err_last;
	float err_pre;
	float kp,ki,kd;
}pid;
void pid_init() {
	pid.setDis = 0.0;
	pid.actAng = 0.0;
	pid.err = 0.0;
	pid.err_last = 0.0;
	pid.err_pre = 0.0;
	pid.kp = 0.2;
	pid.ki = 0.015;
	pid.kd = 0.2;
}
int main()
{
	init();
	turnTo(0);
	pid_init();
	VideoCapture capture(CAM_PATH);
	//If this fails, try to open as a video camera, through the use of an integer param
	if (!capture.isOpened())
	{
		capture.open(atoi(CAM_PATH.c_str()));
	}

	double dWidth=capture.get(CV_CAP_PROP_FRAME_WIDTH);			//the width of frames of the video
	double dHeight=capture.get(CV_CAP_PROP_FRAME_HEIGHT);		//the height of frames of the video
	clog<<"Frame Size: "<<dWidth<<"x"<<dHeight<<endl;

	Mat image;
	while(true)
	{
		capture>>image;
		int mid = image.cols/2;
		if(image.empty())
			break;

		//Set the ROI for the image
		Rect roi(0,2*image.rows/3,image.cols,image.rows/3);
		Mat imgROI=image(roi);

		//Canny algorithm
		Mat imgROI_Gray,imgROI_Bin,imgROI_Dilation,imgROI_Erosion;
		Mat imgROI_Perspective = Mat::zeros( 160,640, CV_8UC3);
		 Mat element = getStructuringElement(MORPH_RECT, Size(2, 2));
		cvtColor(imgROI, imgROI_Gray,CV_RGB2GRAY);
		threshold(imgROI_Gray, imgROI_Bin, 150, 200.0, CV_THRESH_BINARY);
		dilate(imgROI_Bin,imgROI_Dilation,element);
		erode( imgROI_Dilation, imgROI_Erosion,element);

		//fanzhuan
		for(int i =0 ;i<imgROI_Erosion.rows;i++){
			for(int j=0;j<imgROI_Erosion.cols*imgROI_Erosion.channels();j++){
				imgROI_Erosion.at<uchar>(i, j)= 255- imgROI_Erosion.at<uchar>(i, j);
			}
		}

		//tou shi bian huan
		//Point2f srcFixed[] = {Point2f(5,100), Point2f(5,540),Point2f(160,5),Point2f(160,640)};
		//Point2f dstFixed[] = { Point2f(5,5),  Point2f(5,640),Point2f(160,5),Point2f(160,640)};
		//Mat M = getPerspectiveTransform(srcFixed, dstFixed);
		//warpPerspective(imgROI_Erosion, imgROI_Perspective, M, imgROI_Perspective.size());

	
		Mat contours;		
		Canny(imgROI,contours,CANNY_LOWER_BOUND,CANNY_UPPER_BOUND);
		#ifdef _DEBUG
		imshow(CANNY_WINDOW_NAME,contours);
		#endif

		vector<Vec2f> lines;
		HoughLines(contours,lines,1,PI/180,HOUGH_THRESHOLD);
		Mat result(imgROI.size(),CV_8U,Scalar(255));
		imgROI.copyTo(result);
		clog<<lines.size()<<endl;
		
		float maxRad=-2*PI;
		float minRad=2*PI;
		//Draw the lines and judge the slope
		
		float rho1 = 0;
		floatrho2 = 0;
		float theta1 = 0;
		float theta2 = 0;
		for(vector<Vec2f>::const_iterator it=lines.begin();it!=lines.end();++it)
		{
			float rho=(*it)[0];			//First element is distance rho
			float theta=(*it)[1];		//Second element is angle theta

			//Filter to remove vertical and horizontal lines,
			//and atan(0.09) equals about 5 degrees.
			if(rho<0&&theta>1.62&&theta<3.05) {
				theta2 = theta;
				rho2 = -rho;
			}
			if(rho>0&&theta>0.09&&theta<1.48) {
				theta1 = theta;
				rho1 = rho;
			}
			if((theta>0.09&&theta<1.48)||(theta>1.62&&theta<3.05))
			{
				if(theta>maxRad)
					maxRad=theta;
				if(theta<minRad)
					minRad=theta;
				
				#ifdef _DEBUG
				//point of intersection of the line with first row
				Point pt1(rho/cos(theta),0);
				//point of intersection of the line with last row
				Point pt2((rho-result.rows*sin(theta))/cos(theta),result.rows);
				//Draw a line
				line(result,pt1,pt2,Scalar(0,255,255),3,CV_AA);
				#endif
			}
			
			#ifdef _DEBUG
			clog<<"Line: ("<<rho<<","<<theta<<")\n";
			#endif
		}

		#ifdef _DEBUG
		//中点离左边直线距离,正表示中点在其右边，反之。
		clog<<"theta1 "<<theta1<<" rho1 "<<rho1<<endl;
		float dis1 = (mid*cos(theta1)-rho1);
		//右边
		clog<<"theta2 "<<theta2<<"rho2"<<rho2<<endl;
		float dis2 = (mid*cos(theta2)+rho2);
		//>0表示小车车头偏右 <0表示小车车头偏左
		float dis = dis1-dis2;
		//clog
		cout<<"dis"<<dis1<<" "<<dis2<<endl;
		if(dis>200||dis<-200) {
			dis = dis/100;
		}
		if(rho1==0&&rho2!=0){
			dis = 50;
		}
		if(rho2==0&&rho1!=0) { dis = -50;}
		cout<<"disss"<<" "<<dis<<endl;
		pid.err_pre = pid.err_last;
		pid.err_last = pid.err;
		pid.err = dis;
		pid.setDis = dis;
		
		float changeAngle = pid.kp*(pid.err-pid.err_last)+pid.ki*(pid.err)+pid.kd*(pid.err-2*pid.err_last+pid.err_pre);
		
		pid.actAng += changeAngle*COE;
		//clog
		cout<<"changeAng"<<pid.actAng<<endl;
		turnTo(pid.actAng);
		controlLeft(FORWARD,STEP);
		controlRight(FORWARD,STEP);
		
		stringstream overlayedText;
		overlayedText<<"Lines: "<<lines.size();
		putText(result,overlayedText.str(),Point(10,result.rows-10),2,0.8,Scalar(0,0,255),0);
		imshow(MAIN_WINDOW_NAME,result);
		#endif

		lines.clear();
		waitKey(1);
	}
	return 0;
}
