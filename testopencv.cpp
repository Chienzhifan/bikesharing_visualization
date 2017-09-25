#include "opencv2/core/core.hpp"
#include "cv.h"
#include "opencv2/highgui/highgui.hpp"
#include <iostream>
#include <string>
#include <map>
#include <fstream>
#include <vector>
#include <cmath>
#include <ctime>
#include <algorithm>
#include <boost/tokenizer.hpp>
using namespace cv;
using namespace std;
using namespace boost;

const int TIME_DUR=120;   //the time duration (second)
int width = 1680, height = 1050;   //screen width, height
double bigX=-INT_MAX, bigY=-INT_MAX, smallX=INT_MAX, smallY=INT_MAX;   //station location, small x y and big x y
const int DELAY = 10;
const int lineType = CV_AA; // change it to 8 to see non-antialiased graphics
const char wndname[] = "Taipei Bike Traffic";
const char simFilename[] = "smallSample_machinori2.csv";   //the data wnat to simulate
const char locationFilename[] = "location_machinori2.csv";   //the data location
string outputFilename;   //the avi file
int borS, retS;   //for the read data
int start, endt, firstStart, lastEnd;   //for the read data, and some equation
int frameN=0;   //now frame number
int maxS=-1;   //the max same time number
vector<int> obsN;   //the observed stations we want to see
int g_slider_pos = 0;   //for slider position
int pos;   //for image vector
int ringType;   //0: no ring, 1: ring

vector<Mat> image;   //image list
Mat view, tmpMat;

struct Station{
	int no;
	string name;
	double x;
	double y;
	int capacity;
	int curBikeN;   //initial number of bikes
};

struct ExistFrame{
	int stayFrame;
	int firstFrame;
};
bool existFrameSortFunc (ExistFrame i,ExistFrame j) { return (i.stayFrame<j.stayFrame); }

struct Connection{
	map<int, vector<ExistFrame> > rest;   //the rest frame, want to know if the line exists, the vector size is the connect line number
};

struct OnRoadBike {
	int maxOnRoadBikes;
	string maxOnRoadBikeTime;
	OnRoadBike(int t, string s) : maxOnRoadBikes(t), maxOnRoadBikeTime(s){};
};
OnRoadBike totalRoadBike(0, ""), odRoadBike(0, ""), loopRoadBike(0, "");

struct ODData {
	int borrowTime;
	int backTime;
	int duration;
	ODData(int bot, int bat, int dur) : borrowTime(bot), backTime(bat), duration(dur){}
};
vector<ODData> totalRoute[2];   //0 is OD, 1 is self loop number

map<int, Station> sta;
map<int, Connection> connectInf;   //save all the connectInfion
map<string, Point> stName;   //save the station name on the screen
VideoWriter writer("hi.avi", CV_FOURCC('M', 'J', 'P', 'G'), 2, Size(width, height));
typedef tokenizer< escaped_list_separator<char> > Tokenizer;

static void help()
{
    cout << "\nThis program is to show the taipei traffic and output the avi file." << endl;
}

//the traffic of bikes, used the color to distinguish
int setColor(int n){
	if(n<=1)
		return 1;
	else if(n==2 || n==3)
		return n;
	else if(n>=4)
		return 4;
}

//set up the color line (head)
Scalar colorHead(int n){
	return Scalar(250, 205, 123);
}

//set up the color line (tail)
Scalar colorTail(int n){
	if(n<=1)
		return Scalar(0, 255, 127);
	else if(n==2)
		return Scalar(0, 255, 255);
	else if(n==3)
		return Scalar(0, 165, 255);
	else if(n>=4)
		return Scalar(0, 0, 255);
}

void readStation(){   //read station's information
	fstream input;
	int index;
	input.open(locationFilename);
	if(input.is_open()){
		vector<string> vec;
		string line;
		while(getline(input, line)) {
			Tokenizer tok(line);
			vec.assign(tok.begin(), tok.end());
			index = atoi(vec[0].c_str());
			sta[index].no = index;
			sta[index].name = vec[1];
			sta[index].x = atof(vec[2].c_str());
			sta[index].y = atof(vec[3].c_str());
			sta[index].curBikeN = atoi(vec[4].c_str());
			sta[index].capacity = atoi(vec[5].c_str());

			if(sta[index].x > bigX)
				bigX = sta[index].x;
			if(sta[index].x < smallX)
				smallX = sta[index].x;
			if(sta[index].y > bigY)
				bigY = sta[index].y;
			if(sta[index].y < smallY)
				smallY = sta[index].y;
		}

		input.close();
	}
	else
		cout << "location.csv open error." << endl;
}

int screenWidth(double x){   //change to screen location
	double w = bigX - smallX;
	//return 50 + (width - 100) * (x - smallX) * 1.0 / w;
	return 200 + 800 * (x - smallX) *1.0 / w;
}

int screenHeight(double y){   //change to screen location
	double h = bigY - smallY;
	//return -45 + height - (height - 90) * (y - smallY) * 1.0 / h;
	return 50 + 800 - 800 * (y - smallY) * 1.0 / h;
}

string setTimeString(const time_t timeStamp){   //change the time to string
	struct tm *dt;
    char buffer[30];
	dt = localtime(&timeStamp);
	strftime(buffer, sizeof(buffer), "%Y/%m/%d %H:%M", dt);
	return string(buffer);
}

void showTheHelp(Mat &tmp){
	circle(tmp, Point(1300, 200), 15, colorTail(1), CV_FILLED, lineType);
	putText(tmp, ":  1 bikes", Point(1320, 210), FONT_HERSHEY_COMPLEX, 1, colorTail(1), 2, lineType);
	circle(tmp, Point(1300, 240), 15, colorTail(2), CV_FILLED, lineType);
	putText(tmp, ":  2 bikes", Point(1320, 250), FONT_HERSHEY_COMPLEX, 1, colorTail(2), 2, lineType);
	circle(tmp, Point(1300, 280), 15, colorTail(3), CV_FILLED, lineType);
	putText(tmp, ":  3 bikes", Point(1320, 290), FONT_HERSHEY_COMPLEX, 1, colorTail(3), 2, lineType);
	circle(tmp, Point(1300, 320), 15, colorTail(4), CV_FILLED, lineType);
	putText(tmp, ": >3 bikes", Point(1320, 330), FONT_HERSHEY_COMPLEX, 1, colorTail(4), 2, lineType);

	//add the additional information
	putText(tmp, "machinori", Point(600, 950), FONT_HERSHEY_COMPLEX, 1.25, Scalar(1, 126, 24), 2, lineType);
	putText(tmp, "Optimization Algorithm Lab   http://ilin.iim.ncku.edu.tw", Point(20, 1000), FONT_HERSHEY_COMPLEX, 0.55, Scalar(92, 92, 92), 1, lineType);
	putText(tmp, "Prof. I-Lin Wang, IIM, NCKU @ Tainan, Taiwan", Point(20, 1020), FONT_HERSHEY_COMPLEX, 0.55, Scalar(92, 92, 92), 1, lineType);

	if(waitKey(DELAY) >=0) return ;
}

//set station name for the mouse move to show
void setStationName(Point p, string n){
	stName[n]=p;
}

//show station name for the mouse move
string showStationName(int x, int y){
	Point p(x, y);
	for(map<string, Point>::iterator it=stName.begin() ; it!=stName.end() ; ++it){
		if((it->second.x+3>=p.x && it->second.x-3<=p.x) && (it->second.y+3>=p.y && it->second.y-3<=p.y)){
			string tmpS;
			stringstream ss(tmpS);
			ss << sta[atoi(it->first.c_str())].no << "," << sta[atoi(it->first.c_str())].name;
			ss >> tmpS;
			return tmpS;
		}
	}
	return "";
}

//the parameter is the background, and this the is the main function that can print the things
void setUpFrame(Mat back){
	Point pt1, pt2, pt3, midpt;
	int lineColor;   //for setColor(int) and colorHead(int), colorTail(int)
	int onRoadBikes = 0, onRoadBikesLoop = 0;

	//set up the frame
	Mat tmp = back.clone();

	for(map<int, Connection>::iterator it = connectInf.begin() ; it!=connectInf.end() ; ++it){
		for(map<int, vector<ExistFrame> >::iterator it2 = (*it).second.rest.begin() ; it2!=(*it).second.rest.end() ; ++it2){

			if(it->first == it2->first)
				onRoadBikesLoop += it2->second.size();
			else
				onRoadBikes += it2->second.size();
			for(vector<ExistFrame>::iterator it3 = it2->second.begin(); it3 != it2->second.end(); ++ it3){
				//if no connect, no need to show
				//if((*it2).second.size()==0)
				//	continue;

				//set up the color number
				lineColor = setColor((*it2).second.size());

				//to check for the maximum number of the same line, the number of bikes
				if((int)((*it2).second.size())>maxS)
					maxS = (*it2).second.size();

				//set up the line
				pt1.x = screenWidth(sta[(*it).first].x);
				pt1.y = screenHeight(sta[(*it).first].y);

				pt3.x = screenWidth(sta[(*it2).first].x);
				pt3.y = screenHeight(sta[(*it2).first].y);

				// set up the current bike number graph
				if(frameN - (*it3).firstFrame == 0)
					sta[(*it).first].curBikeN -= 1;
				if(frameN - (*it3).stayFrame == 0)
					sta[(*it2).first].curBikeN += 1;

				//because wnat to show llike the animation, so we need to change like this
				pt2.x = pt1.x + (1.0 * (frameN - (*it3).firstFrame + 1) / ((*it3).stayFrame - ((*it3).firstFrame))) * (pt3.x - pt1.x);
				pt2.y = pt1.y + (1.0 * (frameN - (*it3).firstFrame + 1) / ((*it3).stayFrame - ((*it3).firstFrame))) * (pt3.y - pt1.y);

				pt1.x += (1.0 * (frameN - (*it3).firstFrame) / ((*it3).stayFrame - ((*it3).firstFrame))) * (pt3.x - pt1.x);
				pt1.y += (1.0 * (frameN - (*it3).firstFrame) / ((*it3).stayFrame - ((*it3).firstFrame))) * (pt3.y - pt1.y);

				midpt.x = (3*pt1.x + pt2.x)/4;
				midpt.y = (3*pt1.y + pt2.y)/4;

				if(pt1.x == pt3.x && pt1.y == pt3.y)
					continue;
				else{
					line(tmp, pt1, midpt, Scalar(255, 255, 255), 1, lineType);   //line 1, to show where from
					line(tmp, midpt, pt2, colorTail(lineColor), 2, lineType);   //line 1, to show where from
				}
			}
			/*pt2.x = (3*pt1.x + pt3.x)/4;
			pt2.y = (3*pt1.y + pt3.y)/4;

			line(tmp, pt1, pt2, colorHead(lineColor), 2, lineType);   //line 1, to show where from
			line(tmp, pt2, pt3, colorTail(lineColor), 2, lineType);*/   //line 2, to show where go
			if(waitKey(DELAY) >= 0)
				return;

			//clear the unused data
			while((*it2).second.size()>0){
				if((*it2).second[0].stayFrame<=frameN){
					(*it2).second.erase((*it2).second.begin());
				}
				else
					break;
			}
		}
	}
	//set up the time text on the screen
	string timeStr = setTimeString(firstStart+frameN*TIME_DUR);
	putText(tmp, timeStr, Point(1180, 720), FONT_HERSHEY_COMPLEX, 0.8, Scalar(255, 255, 255), 2, lineType);

	// set up the capacity and bike number graph(ring type)
	if(ringType == 1) {
		Point center;
		int totalCur = 0;
		for(map<int, Station>::iterator it=sta.begin() ; it!=sta.end() ; ++it){
			totalCur += it->second.curBikeN;
			center.x = screenWidth((*it).second.x);
			center.y = screenHeight((*it).second.y);

			if((it->second.curBikeN <= it->second.capacity) && (it->second.curBikeN >= 0)) {
				circle(tmp, center, 15, Scalar(0, 0, 255), 1, CV_AA, 0);
				float partialValue = (*it).second.curBikeN*1.0 / (*it).second.capacity;
				ellipse(tmp, center, Size(15, 15), 270, 0, 360*partialValue, Scalar(0, 255, 89), 1, CV_AA, 0);
			}
			else if(it->second.curBikeN > it->second.capacity)
				rectangle(tmp, Point(center.x-16, center.y-16), Point(center.x+16, center.y+16), Scalar(0, 255, 0), 1, CV_AA, 0);
			else if(it->second.curBikeN < 0){
				line(tmp, Point(center.x, center.y-23), Point(center.x+23, center.y+18), Scalar(0, 0, 255), 1, CV_AA, 0);
				line(tmp, Point(center.x+23, center.y+18), Point(center.x-23, center.y+18), Scalar(0, 0, 255), 1, CV_AA, 0);
				line(tmp, Point(center.x-23, center.y+18), Point(center.x, center.y-23), Scalar(0, 0, 255), 1, CV_AA, 0);
			}
		}
		cout << totalCur+onRoadBikes+onRoadBikesLoop << "!!" << endl;
		if(totalCur+onRoadBikes+onRoadBikesLoop < 134) {
			cout << "error" << endl;
			exit(1);
		}
	}

	//calculate some datas

	//on road bikes
	stringstream str, str2, str3;
   	str << "on-road bikes: " << onRoadBikes+onRoadBikesLoop;
	str3 << " (OD: " << onRoadBikes << ", Loop: " << onRoadBikesLoop << ")";
	putText(tmp, str.str(), Point(1180, 650), FONT_HERSHEY_COMPLEX, 0.8, Scalar(255, 255, 255), 2, lineType);
	putText(tmp, str3.str(), Point(1450, 650), FONT_HERSHEY_COMPLEX, 0.6, Scalar(255, 255, 255), 1, lineType);

	//max on total road bikes, od..., loop...
	if(onRoadBikes+onRoadBikesLoop > totalRoadBike.maxOnRoadBikes) {
		totalRoadBike.maxOnRoadBikes = onRoadBikes + onRoadBikesLoop;
		totalRoadBike.maxOnRoadBikeTime = timeStr;
	}
	if(onRoadBikes > odRoadBike.maxOnRoadBikes) {
		odRoadBike.maxOnRoadBikes = onRoadBikes;
		odRoadBike.maxOnRoadBikeTime = timeStr;
	}
	if(onRoadBikesLoop > loopRoadBike.maxOnRoadBikes) {
		loopRoadBike.maxOnRoadBikes = onRoadBikesLoop;
		loopRoadBike.maxOnRoadBikeTime = timeStr;
	}

	//str2 << "max on-road bikes: " << maxOnRoadBikes << " @ " << maxOnRoadBikeTime;
	//putText(tmp, str2.str(), Point(1180, 680), FONT_HERSHEY_COMPLEX, 0.6, Scalar(255, 255, 255), 1, lineType);

	//test the image
	//imshow(wndname, tmp);
	//waitKey();
	//image.push_back(tmp);
	if(writer.isOpened()){
		writer << tmp;
	}

	cout << frameN << endl;
	++frameN;
}

//set up the slider
void onTrackbarSlide(int g_pos, void*){
	view = image[g_pos].clone();
	imshow(wndname, view);
	return;
}

//add the mouse control call back
void mouseControl(int event, int x, int y, int, void*){
	if ( event == EVENT_MOUSEMOVE ){
		tmpMat.release();   //release the memory
		tmpMat = view.clone();

		putText(tmpMat, showStationName(x, y), Point(370, 60), FONT_HERSHEY_COMPLEX, 1, Scalar(255, 255, 255), 1, lineType);
		imshow(wndname, tmpMat);
		waitKey(30);
	}
	else if(event == EVENT_LBUTTONDOWN){   //can resize bigger
		cout << "position (" << x << ", " << y << ")" << endl;
		tmpMat.release();
		Rect roi = Rect(x-50, y-45, 100, 90);
		tmpMat = view(roi);
		Size dsize = Size(width, height);
		resize(tmpMat, tmpMat, dsize, CV_INTER_LINEAR);
		imshow(wndname, tmpMat);
	}
}

string changeTime(double t) {
	t = ceil(t);
	int hour = t/3600, min = (t - hour*3600)/60, sec = (int)t % 60;
	stringstream ss;
	ss << setw(2) << setfill('0') << hour << ":" << setw(2) << setfill('0') << min;// << ":" << setw(2) << setfill('0') << sec;
	return ss.str();
}

string calData(vector<ODData> tmp) {
	double sumT = 0, avgT = 0, stvT = 0, minT = INT_MAX, maxT = INT_MIN;
	for(vector<ODData>::iterator it = tmp.begin(); it != tmp.end(); ++it) {
		avgT += it->duration;
		stvT += pow(it->duration, 2);
		if(it->duration < minT)
			minT = it->duration;
		if(it->duration > maxT)
			maxT = it->duration;
	}
	avgT = avgT * 1.0 / tmp.size();
	stvT = sqrt(stvT * 1.0 / tmp.size() - pow(avgT, 2));

	stringstream ss;
	ss << setw(12) << tmp.size() << setw(12) << changeTime(avgT) << setw(12) << changeTime(stvT) << setw(12) << changeTime(minT) << setw(12) << changeTime(maxT);
	return ss.str();
}

void showFinalPages(Mat back) {
	//show the final pages
	setUpFrame(back);   //let the data clean

	Mat tmp = back.clone();
	stringstream ss, ss2, ss3;

	//max on road bikes, od, loop
	ss << "max on-road ODs:" << setw(9) << "@";
   	ss2 << setw(3) << odRoadBike.maxOnRoadBikes << right << setw(20) << odRoadBike.maxOnRoadBikeTime;
	putText(tmp, ss.str(), Point(1030, 640), FONT_HERSHEY_COMPLEX, 0.8, Scalar(255, 255, 0), 1, lineType);
	putText(tmp, ss2.str(), Point(1330, 640), FONT_HERSHEY_COMPLEX, 0.8, Scalar(239, 228, 216), 1, lineType);
	ss.str("");
	ss2.str("");
	ss << "max on-road Loops: " << setw(6) << "@";
	ss2 << setw(3) << loopRoadBike.maxOnRoadBikes << right << setw(20) << loopRoadBike.maxOnRoadBikeTime;
	putText(tmp, ss.str(), Point(1030, 680), FONT_HERSHEY_COMPLEX, 0.8, Scalar(0, 255, 204), 1, lineType);
	putText(tmp, ss2.str(), Point(1330, 680), FONT_HERSHEY_COMPLEX, 0.8, Scalar(239, 228, 216), 1, lineType);
	ss.str("");
	ss2.str("");
	ss << "max on-road bikes:" << setw(7) << "@";
	ss2 << setw(3) << totalRoadBike.maxOnRoadBikes << right << setw(20) << totalRoadBike.maxOnRoadBikeTime;
	putText(tmp, ss.str(), Point(1030, 720), FONT_HERSHEY_COMPLEX, 0.8, Scalar(128, 91, 253), 1, lineType);
	putText(tmp, ss2.str(), Point(1330, 720), FONT_HERSHEY_COMPLEX, 0.8, Scalar(239, 228, 216), 1, lineType);

	int totalBike = 0, totalCap = 0;
	for(map<int, Station>::iterator it=sta.begin() ; it!=sta.end() ; ++it){
		totalBike += it->second.curBikeN;
		totalCap += it->second.capacity;
	}
	ss.str("");
	ss2.str("");
	ss << "#Bikes/#Docks= ";
    ss2	<< totalBike << "/" << totalCap;
	putText(tmp, ss.str(), Point(1030, 760), FONT_HERSHEY_COMPLEX, 0.8, Scalar(246, 121, 204), 1, lineType);
	putText(tmp, ss2.str(), Point(1250, 760), FONT_HERSHEY_COMPLEX, 0.8, Scalar(239, 228, 216), 1, lineType);
	ss.str("");
	ss2.str("");
	ss << "Turnover rate: ";
   	ss2 << setprecision(2) << fixed << (totalRoute[0].size()+totalRoute[1].size()) * 1.0 / totalBike;
	putText(tmp, ss.str(), Point(1385, 760), FONT_HERSHEY_COMPLEX, 0.8, Scalar(246, 121, 204), 1, lineType);
	putText(tmp, ss2.str(), Point(1605, 760), FONT_HERSHEY_COMPLEX, 0.8, Scalar(239, 228, 216), 1, lineType);

	ss.str("");
	ss2.str("");
	ss << setw(18) << "     #Trips" << setw(12) << "avg" << setw(12) << "stv" << setw(12) << "min" << setw(12) << "max";
	putText(tmp, ss.str(), Point(1030, 820), FONT_HERSHEY_COMPLEX, 0.55, Scalar(128, 91, 253), 1, lineType);
	ss.str("");
	ss2.str("");
	ss << setw(6) << "OD:";
   	ss2 << calData(totalRoute[0]);
	putText(tmp, ss.str(), Point(1030, 860), FONT_HERSHEY_COMPLEX, 0.55, Scalar(255, 255, 0), 1, lineType);
	putText(tmp, ss2.str(), Point(1085, 860), FONT_HERSHEY_COMPLEX, 0.55, Scalar(239, 228, 216), 1, lineType);
	ss.str("");
	ss2.str("");
	ss << setw(6) << "Loop:";
	ss2	<< calData(totalRoute[1]);
	putText(tmp, ss.str(), Point(1030, 900), FONT_HERSHEY_COMPLEX, 0.55, Scalar(0, 255, 204), 1, lineType);
	putText(tmp, ss2.str(), Point(1085, 900), FONT_HERSHEY_COMPLEX, 0.55, Scalar(239, 228, 216), 1, lineType);


	//add bike number and capacity graph
	if(ringType == 1) {
		Point center;
		for(map<int, Station>::iterator it=sta.begin() ; it!=sta.end() ; ++it){
			center.x = screenWidth((*it).second.x);
			center.y = screenHeight((*it).second.y);

			if((it->second.curBikeN <= it->second.capacity) && (it->second.curBikeN >= 0)) {
				circle(tmp, center, 15, Scalar(0, 0, 255), 1, CV_AA, 0);
				float partialValue = (*it).second.curBikeN*1.0 / (*it).second.capacity;
				ellipse(tmp, center, Size(15, 15), 270, 0, 360*partialValue, Scalar(0, 255, 89), 1, CV_AA, 0);
			}
			else if(it->second.curBikeN > it->second.capacity)
				rectangle(tmp, Point(center.x-16, center.y-16), Point(center.x+16, center.y+16), Scalar(0, 255, 0), 1, CV_AA, 0);
			else if(it->second.curBikeN < 0){
				line(tmp, Point(center.x, center.y-23), Point(center.x+23, center.y+18), Scalar(0, 0, 255), 1, CV_AA, 0);
				line(tmp, Point(center.x+23, center.y+18), Point(center.x-23, center.y+18), Scalar(0, 0, 255), 1, CV_AA, 0);
				line(tmp, Point(center.x-23, center.y+18), Point(center.x, center.y-23), Scalar(0, 0, 255), 1, CV_AA, 0);
			}
		}
	}

	if(writer.isOpened()){
		writer << tmp;
	}
}

int main()
{

		ringType = 1;
		obsN.push_back(-1);

	readStation();   //read the station information
    help();
	//RNG rng(0xFFFFFFFF);   //use for random number, maybe no use, but declaring here

	//to save every 5 minutes picture, set up the back first, and the point name
    Mat back = Mat::zeros(height, width, CV_8UC3);
	//back = Scalar(255, 255, 255);
	Point center1, center2;
	for(map<int, Station>::iterator it=sta.begin() ; it!=sta.end() ; ++it){
		center2.x = screenWidth((*it).second.x);
		center2.y = screenHeight((*it).second.y);
		if(ringType == 0) {   //no ring
			center1.x = center2.x + 10;
			center1.y = center2.y + 8;
			circle(back, center2, 3, Scalar(255, 255, 255), CV_FILLED, lineType);
		}
		else {   //draw ring number
			center1.x = center2.x - 11;
			center1.y = center2.y + 4;
		}

		//set up the name on the map
		string tmpStr;
		stringstream ss(tmpStr);
		ss << (*it).first;
		ss >> tmpStr;
		setStationName(center2, tmpStr);

		putText(back, tmpStr, center1, FONT_HERSHEY_COMPLEX, 0.6, Scalar(255, 255, 255), 1.5, lineType);

		if(waitKey(DELAY) >= 0) return 0;
	}

	//add the help box on the screen
	showTheHelp(back);

	//imshow(wndname, back);
	//waitKey();
	//--------------------------------------------------------------


	//because of the data is so big, so we need to read the file and write the data simultaneously.
	// int countNN=0;
	fstream input;
	input.open(simFilename);
	if(input.is_open()){
		vector<string> vec;
		string line;

		getline(input, line);   //first data
		Tokenizer tok(line);
		vec.assign(tok.begin(), tok.end());
		borS = atoi(vec[0].c_str());
		retS = atoi(vec[1].c_str());
		firstStart = atoi(vec[2].c_str());
		endt = atoi(vec[3].c_str());

		//calculate data
		if(borS == retS)
			totalRoute[1].push_back(ODData(firstStart, endt, endt - firstStart));
		else
			totalRoute[0].push_back(ODData(firstStart, endt, endt - firstStart));

		lastEnd = endt;

		//(first time, for setting up the time based) only push back the observations we want to see
		if(obsN[0] == -1){
			ExistFrame tmp;
			tmp.stayFrame = (endt-firstStart)/TIME_DUR;
			tmp.firstFrame = 0;
			connectInf[borS].rest[retS].push_back(tmp);   //push back the frame it can rest
		}
		else{
			for(vector<int>::iterator it=obsN.begin() ; it!=obsN.end() ; ++it){
				if((*it)==borS){
					ExistFrame tmp;
					tmp.stayFrame = (endt-firstStart)/TIME_DUR;
					tmp.firstFrame = 0;
					connectInf[borS].rest[retS].push_back(tmp);   //push back the frame it can rest
					break;
				}
			}
		}

		int flag=0;   //check for the observations if we want to add(line 308)
		while(getline(input, line)){
			tok.assign(line);
			vec.assign(tok.begin(), tok.end());
			borS = atoi(vec[0].c_str());
			retS = atoi(vec[1].c_str());
			start = atoi(vec[2].c_str());
			endt = atoi(vec[3].c_str());

			if(borS == retS)
				totalRoute[1].push_back(ODData(start, endt, endt - start));
			else
				totalRoute[0].push_back(ODData(start, endt, endt - start));

			//do the check for the wrong data
			/*if(borS>506)
				borS-=500;
			if(retS>506)
				retS-=500;*/
			//--------------------------------------
			//(modified OK) need to modify the end calculate, and the OD test---to show the station we want to see
			//only push back the observations we want to see
			flag=0;
			if(obsN[0] != -1){
				for(vector<int>::iterator it=obsN.begin() ; it!=obsN.end() ; ++it){
					if((*it)!=borS){
						++flag;
					}
				}
			}
			if(flag == ((int)obsN.size()))
				continue;

			//--------------------------------------

			//read the last end time point
			if(endt > lastEnd)
				lastEnd = endt;

			while(start-firstStart >= (frameN+1)*TIME_DUR){   //if every five minutes,it need to output a frame
				//set up the frame
				setUpFrame(back);
			}
			// if((frameN>=420 && frameN <479) || (frameN>=1440*1+420 && frameN<1440*1+479)|| (frameN>=1440*2+420 && frameN<1440*2+479)|| (frameN>=1440*3+420 && frameN<1440*3+479)|| (frameN>=1440*4+420 && frameN<1440*4+479))
			// 	countNN++;

			//push back the frame it can rest(minus the firstStart time)
			ExistFrame tmp;
			tmp.stayFrame = (endt-firstStart)/TIME_DUR;
			tmp.firstFrame = frameN;
			connectInf[borS].rest[retS].push_back(tmp);   //push back the frame it can rest(minus the firstStart time)

			//because we want to delete the rest frame is smaller, so need to sort it when adding the data
			sort(connectInf[borS].rest[retS].begin(), connectInf[borS].rest[retS].end(), existFrameSortFunc);
		}

		//the rest must need to output into the rest frames
		while(lastEnd-firstStart >= frameN*TIME_DUR){
			//set up the frame
			setUpFrame(back);
			cout << frameN << endl;
			//frameN++;
		}
	}
	else{
		cout << simFilename << " open error." << endl;
		return -1;
	}

	//show the final page
	showFinalPages(back);

//----------------Display On The Screen-----------------------------
	// cout << countNN << endl;

	//if nothing to show, then close the program
	if((int)image.size()<=0){
		cout << "Nothing to show...\nClose the program." << endl;
		return -1;
	}
	else{
	//char c;
	//create the window, and add the mouse control
	/*namedWindow(wndname, CV_WINDOW_AUTOSIZE);
	setMouseCallback(wndname, mouseControl, NULL);

	createTrackbar("Position", wndname, &g_slider_pos, (int)image.size()-1, onTrackbarSlide);

	view = image[g_slider_pos].clone();
	imshow(wndname, view);
	while(1){
		c=waitKey();
		if(c==27 || c==-1)
			break;
		else if(c==97){   //turn left
			setTrackbarPos("Position", wndname, g_slider_pos-1);
		}
		else if(c==100){   //turn right
			setTrackbarPos("Position", wndname, g_slider_pos+1);
		}
	}
	destroyWindow(wndname);*/

	//ask if want to write into the video
	/*cout << endl << "Output the video file?(Y/N): ";
	string check;
	while(getline(cin, check)){
		if(check.compare("y")==0 || check.compare("Y")==0){
			cout << "output file name: ";
			getline(cin, outputFilename);
			VideoWriter writer(outputFilename.c_str(), CV_FOURCC('M', 'J', 'P', 'G'), 2, Size(width, height));
			if(writer.isOpened()){
				for(int i=0 ; i<image.size() ; i++){
					writer << image[i];
				}
			}
			break;
		}
		else if(check.compare("n")==0 || check.compare("N")==0){
			break;
		}
		else{
			cout << "Wrong input." << endl;
		}
	}*/}
	cout << endl << "Bye Bye!" << endl;
	//cout << "!!!" << maxS << endl;
    //waitKey();
    return 0;
}
