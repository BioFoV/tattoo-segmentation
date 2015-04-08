#include <opencv2/opencv.hpp>
#include <iostream>
#include <time.h>
#include <math.h>

#define WINDOW_FLAGS CV_WINDOW_KEEPRATIO | CV_WINDOW_NORMAL | CV_GUI_EXPANDED
#define ORIG_NAME "Original"
#define FG_NAME "Foreground"

#define LINE_THICKNESS 2

class ROI {
	private:
		cv::Point p1;
		cv::Point p2;
		
		bool holding;
		bool keycrtl;
		cv::Mat orig;	// original image
		cv::Mat drawn;	// original that will hold the displayed image
		cv::Mat mask;
		
		// rectangle colour (BGR)
		cv::Scalar colour;
	
	public:
		ROI(cv::Mat image){
			holding = false;
			keycrtl = false; // TODO: usar isto para desenhar o bg mask
			// fg mask n vale a pena pq as tatuagens podem ser mt finas
			orig = image;
			mask = cv::Mat(image.size(), CV_8UC1, cv::GC_PR_FGD);
			// set colour of rectangle
			colour = cv::Scalar(0,0,255);
		}
		
		void click(cv::Point p){
			hover(p);
			holding = !holding;
			if (holding){
				p1 = p;
			} else {
				p2 = p;
			}
		}
		
		void draw(cv::Point p){
			hover(p);
			keycrtl = !keycrtl;
		}
		
		void hover(cv::Point p){
			if (holding){
				drawn = orig.clone();
				cv::rectangle(drawn, p1, p, colour, LINE_THICKNESS);
				cv::imshow(ORIG_NAME, drawn);
			} else if (keycrtl) {
				cv::circle(drawn,
					p,
					3,
					cv::Scalar( 255, 0, 0 ),
					-1,
					8);
				cv::circle(mask,
					p,
					3,
					cv::GC_BGD,
					-1,
					8);
				cv::imshow(ORIG_NAME, drawn);
			}
		}
		
		cv::Rect get_rect(){
			return cv::Rect(p1,p2);
		}
		
		cv::Mat get_mask(){
			return mask;
		}
};

void mouseHandler(int event, int x, int y, int flags, void* param)
{
	//circle(Mat& img, Point center, int radius, const Scalar& color, int thickness=1, int lineType=8, int shift=0)
	ROI * rect = (ROI *) param;
	
	switch(event){
		case CV_EVENT_LBUTTONDOWN:
			if(flags & CV_EVENT_FLAG_CTRLKEY){
				rect->draw(cv::Point(x,y));
			} else {
				rect->click(cv::Point(x,y));
			}
			break;
		case CV_EVENT_LBUTTONUP:
			if(flags & CV_EVENT_FLAG_CTRLKEY) {
				rect->draw(cv::Point(x,y));
			} else {
				rect->click(cv::Point(x,y));
			}
			break;
		default:
			rect->hover(cv::Point(x,y));
	}
}

void trackHandler(int pos, void* param){
	cv::VideoCapture * capture = (cv::VideoCapture *) param;
	capture->set(CV_CAP_PROP_POS_FRAMES, pos);
}

int main(int argc, char ** argv){
	
	std::string inputFileName;
	if (argc>1)
		inputFileName = argv[1];
	else
		return 0;
	
	cv::Mat image = cv::imread(inputFileName);
	cv::Mat imageHSV;
	
	if(image.empty()){ 
		std::cout << "Couldn't open " << inputFileName << std::endl; 
		return -1;
	}
	cv::cvtColor(image, imageHSV, CV_BGR2HSV);
	
	// create window and UI
	cv::namedWindow(ORIG_NAME, WINDOW_FLAGS);
	cv::namedWindow(FG_NAME, WINDOW_FLAGS);
	
	ROI rect = ROI(image);
	
	// event handlers
	//int mouseParam = CV_EVENT_FLAG_LBUTTON;
	cv::setMouseCallback(ORIG_NAME,mouseHandler, &rect);
	
	cv::imshow(ORIG_NAME, image);
	//cv::imshow(HSV_NAME, imageHSV);
	
	cv::Mat bgdModel,fgdModel;
	cv::Rect rec;
	cv::Mat mask = cv::Mat::ones(image.size(), CV_8U) * cv::GC_PR_BGD;
	// result of grabCut
	cv::Mat newmask;
	int iterCount = 1;	// number of iterations for each pass (1 pass done only)
	cv::Mat foreground;
	int c;
	int iterations = 0;
	float margin = .05;	// size of the bezel to be taken as sure
						//background (proportion of the image)
	
	while (true){
		c = cv::waitKey(10);
		
		switch (c){
			case -1:		// No key
				continue;
			case 1048603:	// ESC Numpad on
			case 27:		//
			case 113:		//
				cv::destroyWindow(ORIG_NAME);
				cv::destroyWindow(FG_NAME);
				return 0;
			case 1048586:	// Enter Numpad on
			case 10:		// Enter
				// If the initial rectangle is to be defined
				if (iterations == 0){
					std::cout << "initialized margin" << std::endl;
					
					//rec = rect.get_rect();
					rec = cv::Rect(cv::Point(floor((image.cols-1) * margin),
						floor((image.rows-1) * margin)),
						cv::Point(image.cols-1-floor((image.cols-1) * margin),
						image.rows-1-floor((image.rows-1) * margin)));
					//FIXME Test if rect is in image or if it has any null size
					
					cv::grabCut(image,			// input image
						mask,					// segmentation result
						rec,					// rectangle containing the foreground
						bgdModel, fgdModel,		// models
						iterCount,				// number of iterations
						cv::GC_INIT_WITH_RECT & cv::GC_INIT_WITH_MASK);	// use rectangle and mask
					
					cv::compare(mask,cv::GC_PR_FGD,	// compare these two
						newmask,					// and save here
						cv::CMP_EQ);				// if they are equal
					
					image.copyTo(foreground,newmask);
					cv::imshow(FG_NAME, foreground);
					cv::imwrite("./tattoo.png", foreground);
					iterations ++;
				}
				// If the initial rectangle was already defined
				else {
					std::cout << "colour segmentation" << std::endl;
					// SKIN
					/*
					int lowerH=0, upperH=90,
						lowerS=0, upperS=255,
						lowerV=200, upperV=255;
					*/
					int lowerH=160, upperH=255,
						lowerS=160, upperS=255,
						lowerV=160, upperV=255;
					
					cv::Scalar lower(lowerH,lowerS,lowerV);
					cv::Scalar upper(upperH,upperS,upperV);
					
					cv::Mat skin;
					cv::Mat skinmask;
					
					//cv::cvtColor(foreground, skin, CV_BGR2HSV);
					skin = foreground.clone();
					/*
					cv::inRange(skin,
						cv::Scalar(lowerH,0,0),
						cv::Scalar(upperH,255,255),
						skinmask);
					mask.setTo(cv::GC_PR_BGD, skinmask);
					cv::inRange(skin,
						cv::Scalar(0,lowerS,0),
						cv::Scalar(255,upperS,255),
						skinmask);
					mask.setTo(cv::GC_PR_BGD, skinmask);
					cv::inRange(skin,
						cv::Scalar(0,0,lowerV),
						cv::Scalar(255,255,upperV),
						skinmask);
					mask.setTo(cv::GC_PR_BGD, skinmask);
					*/
					cv::inRange(skin,
						cv::Scalar(0,lowerS,lowerV),
						cv::Scalar(255,upperS,upperV),
						skinmask);
					mask.setTo(cv::GC_PR_BGD, skinmask);
					cv::inRange(skin,
						cv::Scalar(lowerH,0,lowerV),
						cv::Scalar(upperH,255,upperV),
						skinmask);
					mask.setTo(cv::GC_PR_BGD, skinmask);
					cv::inRange(skin,
						cv::Scalar(lowerH,lowerS,0),
						cv::Scalar(upperH,upperS,255),
						skinmask);
					mask.setTo(cv::GC_PR_BGD, skinmask);
					// TATTOO
					/*
					lowerH=0; upperH=255;
					lowerS=0; upperS=255;
					lowerV=0; upperV=175;
					
					lower = cv::Scalar(lowerH,lowerS,lowerV);
					upper = cv::Scalar(upperH,upperS,upperV);
					
					cv::cvtColor(foreground, skin, CV_BGR2HSV);
					cv::inRange(skin, lower, upper, skinmask);
					
					mask.setTo(cv::GC_PR_FGD, skinmask);
					*/
					// CONTINUE
					cv::imshow(FG_NAME, mask*32);
					cv::waitKey(0);
					
					mask.setTo(cv::GC_PR_BGD, skinmask);
					
					cv::grabCut(foreground,		// input image
						mask,					// segmentation result
						rec,					// rectangle containing the foreground
						bgdModel, fgdModel,		// models
						iterCount,				// number of iterations
						cv::GC_EVAL);				// 
					
					newmask = cv::GC_PR_FGD;
					cv::compare(mask,cv::GC_PR_FGD,	// compare these two
						newmask,					// and save here
						cv::CMP_EQ);				// if they are equal
					
					// reset foreground image
					foreground = cv::Scalar(0);
					// apply newmask to image and save it in the foreground
					image.copyTo(foreground,newmask);
					
					cv::imshow(FG_NAME, foreground);
					iterations ++;
				}
				break;
			case 114:		// r key
				std::cout << "reset mask!" << std::endl;
				iterations = 0;
				break;
			default:
				std::cout << c << std::endl;
		}
	}
	
	cv::destroyWindow(ORIG_NAME);
	cv::destroyWindow(FG_NAME);
	
	return 0;
}
