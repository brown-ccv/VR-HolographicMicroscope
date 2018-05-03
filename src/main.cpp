
// OpenGL platform-specific headers
#if defined(WIN32)
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#include <gl/GLU.h>
#elif defined(__APPLE__)
#include <OpenGL/OpenGL.h>
#include <OpenGL/glu.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glu.h>
#endif


// MinVR header
#include <api/MinVR.h>
#include "main/VREventInternal.h"
#include "tinyxml2.h"
#include "main/VRGraphicsStateInternal.h"
#include "VRMenu.h"
#include "VRButton.h"
#include "VRMenuHandler.h"
#include "VRMultiLineTextBox.h"
#include "VRTextBox.h"
#include "VRGraph.h"
#include "VRToggle.h"
using namespace MinVR;

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

// Just included for some simple Matrix math used below
// This is not required for use of MinVR in general
#include <math/VRMath.h>

// deg2rad * degrees = radians
#define deg2rad (3.14159265/180.0)
// rad2deg * radians = degrees
#define rad2deg (180.0/3.14159265)
#define 	M_PI   3.14159265358979323846	/* pi */

#define REPORTNAME "reportRaw_refined.xml"
int skip_nth_Image = 1;
double min_Z = 500;
double max_Z = 25000;
bool draw_Boundary = true;

#define MOVIE_FPS_MODIFIER 1.0/6.0
#define LOAD_LIMIT 1000000000
#define SHOW_LIMIT 7
#define SCALE 200.0
#define Z_SCALE 1.0 //10
#define MOVE_SCALE 5.0f;


#define ALLOW_ROTATE 1

#define SCREEN_TO_SOURCE 100000.0f
#define PIXEL_SIZE 7.4f
#define RATIO_P_TO_UM 1024.0f / SCALE / SCREEN_TO_SOURCE * PIXEL_SIZE 


#ifdef _MSC_VER
	#define slash "\\"
#else
	#define slash "/"
	#include <dirent.h>
#endif

struct hologram {
	float vertices[4][3];
	unsigned int texture;
	float esd;
	float esv;
	std::string type;
	int setID;
	int ID;
};


int mode = 0;
bool show_menu = 0;
bool move_menu = 1;
bool show_measure = 1;
bool show_info = 1;
bool play = false;
bool trace = false;
float frame_strength = 1.0;

struct DataSet
{
	std::vector <hologram> quads;
	std::vector <cv::Mat> opencvImages;
	std::vector <std::string> value_names;
	std::vector <std::string> values;
	int id;
	std::string filename;
};

std::vector <DataSet> data;

int getContourByID(int frame, int contourID)
{
	if (frame < 0 || frame >= data.size())
		return -1;

	for (int i = 0; i < data[frame].quads.size(); i++)
	{
		if (data[frame].quads[i].ID == contourID)
			return i;
	}

	return -1;
}

std::istream& safeGetline(std::istream& is, std::string& t)
{
	t.clear();

	// The characters in the stream are read one-by-one using a std::streambuf.
	// That is faster than reading them one-by-one using the std::istream.
	// Code that uses streambuf this way must be guarded by a sentry object.
	// The sentry object performs various tasks,
	// such as thread synchronization and updating the stream state.

	std::istream::sentry se(is, true);
	std::streambuf* sb = is.rdbuf();

	for (;;)
	{
		int c = sb->sbumpc();
		switch (c)
		{
		case '\n':
			return is;
		case '\r':
			if (sb->sgetc() == '\n')
				sb->sbumpc();
			return is;
		case EOF:
			// Also handle the case when the last line has no line ending
			if (t.empty())
			{
				is.setstate(std::ios::eofbit);
			}
			return is;
		default:
			t += (char)c;
		}
	}
}

std::vector<string> ReadSubDirectories(const std::string &refcstrRootDirectory)
{
	std::vector<string> subdirectories;
	
#ifdef _MSC_VER
	// subdirectories have been found
	HANDLE          hFile;                       // Handle to directory
	std::string     strFilePath;                 // Filepath
	std::string     strPattern;                  // Pattern
	WIN32_FIND_DATA FileInformation;             // File information


	strPattern = refcstrRootDirectory + slash + "*.*";
	hFile = ::FindFirstFile(strPattern.c_str(), &FileInformation);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (FileInformation.cFileName[0] != '.')
			{
				strFilePath.erase();
				strFilePath = refcstrRootDirectory + slash + FileInformation.cFileName;

				if (FileInformation.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					// Delete subdirectory
					subdirectories.push_back(FileInformation.cFileName);
				}
			}
		} while (::FindNextFile(hFile, &FileInformation) == TRUE);

		// Close handle
		::FindClose(hFile);
	}
#else
	DIR *dir;
	struct dirent *entry;
	char path[256];

	dir = opendir(refcstrRootDirectory.c_str());
	if (dir == NULL) {
		return subdirectories;
	}

	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
			if (entry->d_type == DT_DIR) {
				subdirectories.push_back(std::string(entry->d_name));
			}
		}
	}
	closedir(dir);

#endif
	std::sort(subdirectories.begin(), subdirectories.end());
	return subdirectories;
}

void addHologram(float x, float y, float z, float width, float height, std::string filename, double esd, double esv, std::string type, DataSet &set, int ID)
{
	hologram q;
	q.vertices[0][0] = (x - width / 2) / SCALE;
	q.vertices[0][1] = -(y + height / 2) / SCALE;
	q.vertices[0][2] = (-z + min_Z) / SCALE / Z_SCALE;

	q.vertices[1][0] = (x + width / 2) / SCALE;
	q.vertices[1][1] = -(y + height / 2) / SCALE;
	q.vertices[1][2] = (-z + min_Z) / SCALE / Z_SCALE;

	q.vertices[2][0] = (x + width / 2) / SCALE;
	q.vertices[2][1] = -(y - height / 2) / SCALE;
	q.vertices[2][2] = (-z + min_Z) / SCALE / Z_SCALE;

	q.vertices[3][0] = (x - width / 2) / SCALE;
	q.vertices[3][1] = -(y - height / 2) / SCALE;
	q.vertices[3][2] = (-z + min_Z) / SCALE / Z_SCALE;

	q.esd = esd;
	q.esv = esv;
	q.type = type;
	q.setID = set.id;
	q.ID = ID;
	set.quads.push_back(q);

	cv::Mat image_orig = cv::imread(filename, cv::IMREAD_COLOR);
	cv::Mat image_transparent = cv::Mat(image_orig.rows, image_orig.cols, CV_8UC4);

	for (int i = 0; i < image_orig.rows; i++)
	{
		for (int j = 0; j < image_orig.cols; j++)
		{
			uchar val = image_orig.at<cv::Vec3b>(i, j)[0];
			image_transparent.at<cv::Vec4b>(i, j)[0] = val;
			image_transparent.at<cv::Vec4b>(i, j)[1] = val;
			image_transparent.at<cv::Vec4b>(i, j)[2] = val;
			if (image_orig.at<cv::Vec3b>(i, j)[2] != image_orig.at<cv::Vec3b>(i, j)[0])
			{
				image_transparent.at<cv::Vec4b>(i, j)[3] = 0;
			}
			else
			{
				image_transparent.at<cv::Vec4b>(i, j)[3] = 255;
			}
		}
	}
	set.opencvImages.push_back(image_transparent);
}

void loadDataSet(std::string parentFolder, std::string folder, int id)
{
	DataSet set;
	std::string reportName = parentFolder + slash + folder + slash + REPORTNAME;
	tinyxml2::XMLDocument doc;
	if (doc.LoadFile(reportName.c_str()) == tinyxml2::XML_SUCCESS){
		tinyxml2::XMLElement* titleElement;
		titleElement = doc.FirstChildElement("doc")->FirstChildElement("DATA");

		set.id = id;
		set.filename = folder;
		set.value_names.push_back("NB Particles detected");
		set.values.push_back(titleElement->FirstChildElement("NBCONTOURS")->GetText());

		//Load values
		std::ifstream fin(parentFolder + slash + folder + slash + "data" + slash + folder.substr(0, folder.rfind(".")) + ".txt");
		if (fin.good())
		{
			std::istringstream in;
			std::string line;
			std::string s;
			int count = 0;
			while (!safeGetline(fin, line).eof())
			{
				in.clear();
				in.str(line);
				std::vector<std::string> tmp;
				while (getline(in, s, ' ')) {
					tmp.push_back(s);
				}

				if (tmp.size() == 2)
				{
					set.value_names.push_back(tmp[0]);
					set.values.push_back(tmp[1]);
				}
				line.clear();
				tmp.clear();
				count++;
			}
			fin.close();
		}

		for (tinyxml2::XMLElement* child = titleElement->FirstChildElement("ROI"); child != NULL; child = child->NextSiblingElement())
		{
			// do something with each child element
			addHologram(std::atof(child->FirstChildElement("X")->GetText()),
						std::atof(child->FirstChildElement("Y")->GetText()),
						std::atof(child->FirstChildElement("DEPTH")->GetText()),
						std::atof(child->FirstChildElement("WIDTH")->GetText()),
						std::atof(child->FirstChildElement("HEIGHT")->GetText()),
						parentFolder + slash + folder + slash + child->FirstChildElement("IMAGE")->GetText(),
						std::atof(child->FirstChildElement("ESD")->GetText()), 
						std::atof(child->FirstChildElement("ESV")->GetText()), 
						"Diatom",
						set,
						std::atof(child->FirstChildElement("CONTOUR")->GetText()));
		}
		set.values[0] = std::to_string(set.quads.size());
	}
	

	data.push_back(set);
}

/** MyVRApp is a subclass of VRApp and overrides two key methods: 1. onVREvent(..)
    and 2. onVRRenderGraphics(..).  This is all that is needed to create a
    simple graphics-based VR application and run it on any display configured
    for use with MinVR.
 */
class MyVRApp : public VRApp, VRMenuHandler {
public:
	MyVRApp(int argc, char** argv, const std::string& configFile) : currentSet(0), VRApp(argc, argv), texturesloaded(false), movement_y(0.0), movement_x(0.0), currentMenu(0), hoverHologram(NULL), menuVisible(false), measuring(false), measureSet(false){
		if (argc >= 5)
		{
			mode = stoi(argv[4]);
		}
		if (argc >= 6)
		{
			min_Z = stof(argv[5]);
		}
		if (argc >= 7)
		{
			max_Z = stof(argv[6]);
		}
		if (argc >= 8)
		{
			skip_nth_Image = stoi(argv[7]);
		}
		if (argc >= 8)
		{
			frame_strength = stof(argv[8]);
		}
		if (mode == 3)
		{
			mode = 2;
			trace = true;
		}

		if (mode == 0)
		{
			show_menu = 1;
			move_menu = 1;
			show_measure = 1;
			show_info = 1;
		}
		else if (mode == 1)
		{
			show_menu = 0;
			move_menu = 0;
			show_measure = 1;
			show_info = 1;
		}
		else if (mode == 2)
		{
			show_menu = 0;
			move_menu = 1;
			show_measure = 0;
			show_info = 0;
			play = true;
		}

		menupose = VRMatrix4::translation(VRVector3(-1, 0.9, 0)) * VRMatrix4::rotationY(M_PI / 2);
		std::vector<std::string> subdirs = ReadSubDirectories(argv[3]);
		for (int i = 0; i < subdirs.size() && i < LOAD_LIMIT; i++){
			if (i % skip_nth_Image == 0){
				std::cerr << "Load " << subdirs[i] << std::endl;
				loadDataSet(argv[3], subdirs[i], i);
			}
		}

		centerHologram(data[0]);
		computeHologramSize();
		currentSet = 0;
		graph_currentValue = 0;
		createMenu();
    	}

    virtual ~MyVRApp()
	{
		for (std::vector<VRMenu*>::const_iterator it = menus.begin(); it != menus.end(); ++it)
			delete (*it);
	}

	void createMenu()
	{
		
			VRMenu * ctd_data_current_menu = new VRMenu(0.5, 0.5, 2, 10, "CTD data current Hologram");
			ctd_data_current_filename = new VRTextBox("ctd_data_current_filename", data[0].filename, VRFontHandler::LEFT);
			ctd_data_current_menu->addElement(ctd_data_current_filename, 1, 1, 2, 1);
			ctd_data_current_textBox_valueNames = new VRMultiLineTextBox("ctd_data_current_textBox_valueNames", data[0].value_names, VRFontHandler::RIGHT);
			ctd_data_current_menu->addElement(ctd_data_current_textBox_valueNames, 1, 2, 1, 9);
			ctd_data_current_textBox_values = new VRMultiLineTextBox("ctd_data_current_textBox_values", data[0].values, VRFontHandler::LEFT);
			ctd_data_current_menu->addElement(ctd_data_current_textBox_values, 2, 2, 1, 9);

		if (mode != 2 ){
			menus.push_back(ctd_data_current_menu);
			ctd_data_current_menu->addMenuHandler(this);
			ctd_data_current_menu->setVisible(false);
		}

		VRMenu * ctd_data_graph_menu = new VRMenu(0.5, 0.5, 7, 10, (mode != 2) ? "CTD data" : "Movie");
		if (mode != 2){
			ctd_data_graph_next = new VRButton("ctd_data_graph_next", ">");
			ctd_data_graph_menu->addElement(ctd_data_graph_next, 7, 1, 1, 1);
			ctd_data_graph_prev = new VRButton("ctd_data_graph_prev", "<");
			ctd_data_graph_menu->addElement(ctd_data_graph_prev, 1, 1, 1, 1);
		}
		if (data[0].value_names.size() > graph_currentValue){
			ctd_data_graph_ValueName = new VRTextBox("ctd_data_graph_ValueName", data[0].value_names[graph_currentValue]);
			ctd_data_graph_currentValue = new VRTextBox("ctd_data_graph_currentValue", data[0].values[graph_currentValue]);
		}
		else
		{
			ctd_data_graph_ValueName = new VRTextBox("ctd_data_graph_ValueName", "");
			ctd_data_graph_currentValue = new VRTextBox("ctd_data_graph_currentValue", "");
		}

		if (mode == 2)
		{
			ctd_data_graph_play = new VRToggle("ctd_data_graph_play", "Play");
			ctd_data_graph_play->setToggled(true);
			ctd_data_graph_menu->addElement(ctd_data_graph_play, 1, 1, 7, 1);
		}

		ctd_data_graph_menu->addElement(ctd_data_graph_ValueName, (mode != 2) ? 2 : 1, (mode != 2) ? 1 : 2, (mode != 2) ? 5 : 7, 1);
		ctd_data_graph_menu->addElement(ctd_data_graph_currentValue, 1, (mode != 2) ? 2 : 3, 7, 1);

		ctd_data_graph_graph = new VRGraph("ctd_data_graph_graph", getDataFromHologram(graph_currentValue), (mode!=2));
		ctd_data_graph_graph->setCurrent(currentSet);
		ctd_data_graph_menu->addElement(ctd_data_graph_graph, 1, (mode != 2) ? 3 : 4, 7, (mode != 2) ? 7 : 6);
		menus.push_back(ctd_data_graph_menu);

		ctd_data_graph_menu->addMenuHandler(this);
	}

	void drawMenus()
	{
		for (std::vector<VRMenu*>::const_iterator it = menus.begin(); it != menus.end(); ++it)
			(*it)->draw();
	}

	void updateMenus()
	{
		for (std::vector<VRMenu*>::const_iterator it = menus.begin(); it != menus.end(); ++it)
			(*it)->setTransformation(menupose);

		VRPoint3 pos = controllerpose * VRPoint3(0, 0, 0);
		VRVector3 dir = controllerpose * VRVector3(0, 0, -2);

		double distance;
		for (std::vector<VRMenu*>::const_iterator it = menus.begin(); it != menus.end(); ++it){
			(*it)->intersect(pos, dir, distance);
		}
	}

	void clickMenus(bool isDown)
	{
		for (std::vector<VRMenu*>::const_iterator it = menus.begin(); it != menus.end(); ++it)
			(*it)->click(isDown);
	}

	void displayMenu(int menuID)
	{
		std::cerr << menus.size() << std::endl;
		if (show_menu || move_menu){
			if (menuID == -1)
				menuVisible = false;
			for (int i = 0; i < menus.size(); i++){
				if (i != menuID)
				{
					menus[i]->setVisible(false);
				}
				else
				{
					menus[i]->setVisible(true);
				}
			}
		}
	}

	void setMeasurePoint(bool setStart)
	{
		if (setStart)
		{
			measureSet = false;
		}
		VRPoint3 pos = roompose.inverse() * controllerpose * VRPoint3(0, 0, 0);
		VRVector3 dir = roompose.inverse() * controllerpose * VRVector3(0, 0, -5);

		int maxRange = 0;
		
		int start = currentSet - maxRange;
		int end = currentSet + maxRange;
		if (mode != 0)
		{
			start = 0;
			end = data.size() - 1;
		}
		
		if (start < 0)
			start = 0;
		if (end > data.size() - 1)
			end = data.size() - 1;

		double distance = 5;

		for (int i = start; i <= end; i++){
			for (std::vector<hologram>::iterator it = data[i].quads.begin(); it != data[i].quads.end(); ++it){
				double z = (mode == 0) ? it->vertices[0][2] - it->setID * hologramSize[2] : it->vertices[0][2];
				double d = (z - pos.z) / dir.z;

				if (d > 0 && d < distance){
					MinVR::VRPoint3 m_interactionPoint = pos + dir * d;
					if (m_interactionPoint.x >= it->vertices[0][0]
						&& m_interactionPoint.y >= it->vertices[1][1]
						&& m_interactionPoint.x <= it->vertices[1][0]
						&& m_interactionPoint.y <= it->vertices[2][1])
					{
						distance = d;
						if (setStart)startMeasure = m_interactionPoint;
						endMeasure = m_interactionPoint;
						measureSet = true;
					}
				}
			}
		}
	}

	void checkHologramIntersect()
	{
		hoverHologram = NULL;
		VRPoint3 pos = roompose.inverse() * controllerpose * VRPoint3(0, 0, 0);
		VRVector3 dir = roompose.inverse() * controllerpose * VRVector3(0, 0, -5);

		int maxRange = 10.0f / hologramSize[2];
		int start = currentSet - maxRange;
		int end = currentSet + maxRange;

		if (mode != 0)
		{
			start = 0;
			end = data.size() - 1;
		}


		if (start < 0) 
			start = 0;
		if (end > data.size() - 1)
			end = data.size() - 1;

		double distance = 5;

		for (int i = start; i <= end; i++){
			for (std::vector<hologram>::iterator it = data[i].quads.begin(); it != data[i].quads.end(); ++it){
				double z = (mode == 0) ? it->vertices[0][2] - it->setID * hologramSize[2] : it->vertices[0][2];
				double d = (z - pos.z) / dir.z;

				if (d > 0 && d < distance){
					MinVR::VRPoint3 m_interactionPoint = pos + dir * d;
					if (m_interactionPoint.x >= it->vertices[0][0] 
						&& m_interactionPoint.y >= it->vertices[1][1]
						&& m_interactionPoint.x <= it->vertices[1][0]
						&& m_interactionPoint.y <= it->vertices[2][1] )
					{
						distance = d;
						hoverHologram = &(*it);
					}
				}
			}
		}
	}

	std::vector<double> getDataFromHologram(int valueID)
	{
		std::vector<double> data_out;

		for (std::vector<DataSet>::const_iterator it = data.begin(); it != data.end(); ++it)
		{
			if (it->values.size() > valueID && valueID >= 0)
			{
				data_out.push_back(std::stof(it->values[valueID]));
			}
			else
			{
				data_out.push_back(GRAPHUNDEFINEDVALUE);
			}
		}

		return data_out;
	}

	bool startsWith(std::string string1, std::string string2)
	{
		if (strlen(string1.c_str()) < strlen(string2.c_str())) return false;

		return !strncmp(string1.c_str(), string2.c_str(), strlen(string2.c_str()));
	}
	
	template <typename T> int sgn(T val) {
		return (T(0) < val) - (val < T(0));
	}

	// Callback for event handling, inherited from VRApp
	virtual void onVREvent(const VREvent &event) {
		if (event.getName() == "HTC_Controller_Left")
		{
			if (event.getInternal()->getDataIndex()->exists("/HTC_Controller_Left/Pose")){
				if (!move_menu){
					menupose = event.getDataAsFloatArray("Pose");
					menupose = menupose *  VRMatrix4::translation(VRVector3(0, -0.2, 0));
				}
				updateMenus();
			}
		}

		if (event.getName() == "HTC_Controller_Right_Axis1Button_Pressed"){
			clickMenus(true);
			setMeasurePoint(true);
			measuring = true;
		}
		if (event.getName() == "HTC_Controller_Right_Axis1Button_Released"){
			clickMenus(false);
			measuring = false;
		}
		if (event.getName() == "HTC_Controller_Left_Axis1Button_Pressed"){
			displayMenu(currentMenu);
			if (move_menu) updateMenus();
		}
		if (event.getName() == "HTC_Controller_Left_Axis1Button_Released"){
			if (!move_menu) displayMenu(-1);
		}
		if (event.getName() == "HTC_Controller_Right_AButton_Pressed"){
			if (mode != 2 && (show_menu || move_menu)){
				currentMenu++;
				currentMenu = currentMenu % menus.size();
				displayMenu(currentMenu);
			}
			else{
				float tmp = currentSet - 1;
				if (tmp < 0) tmp = data.size() - 1;
				setCurrentSet(tmp);			
			}
		}
		if (event.getName() == "HTC_Controller_Right_ApplicationMenuButton_Pressed" && (mode == 2)){
			
			float tmp = currentSet + 1;
			if (tmp >= data.size()) tmp = 0;
			setCurrentSet(tmp);	
		}

		if (event.getName() == "HTC_Controller_Left" && move_menu)
		{
			if (event.getInternal()->getDataIndex()->exists("/HTC_Controller_Left/State/Axis1Button_Pressed") &&
				(int) event.getInternal()->getDataIndex()->getValue("/HTC_Controller_Left/State/Axis1Button_Pressed"))
			{
				if (event.getInternal()->getDataIndex()->exists("/HTC_Controller_Left/Pose")){
					menupose = event.getDataAsFloatArray("Pose");
					menupose = menupose * VRMatrix4::translation(VRVector3(0, -0.2, 0));;
					updateMenus();
				}
			}
		}

		if (event.getName() == "HTC_Controller_Right")
		{
			if(event.getInternal()->getDataIndex()->exists("/HTC_Controller_Right/Pose")){
				controllerpose = event.getDataAsFloatArray("Pose");
				updateMenus();
				checkHologramIntersect();
				if (measuring) setMeasurePoint(false);
			}

			{
			
				movement_x = (ALLOW_ROTATE) ? 0.2f * (float) event.getInternal()->getDataIndex()->getValue("/HTC_Controller_Right/State/Axis0/XPos") : 0.0; 
				movement_y = (float)event.getInternal()->getDataIndex()->getValue("/HTC_Controller_Right/State/Axis0/YPos") * MOVE_SCALE;
			}
		}

		if (event.getName() == "KbdEsc_Down") {
            		shutdown();
            		return;
		}  
	}

	virtual void handleEvent(VRMenuElement * element)
	{
		if (element == ctd_data_graph_graph)
		{
			if (mode != 2){
				centerHologram(data[ctd_data_graph_graph->getSelection()]);
				setCurrentSet();
			}
			else
			{
				setCurrentSet(ctd_data_graph_graph->getSelection());
			}
		}
		if (element == ctd_data_graph_next)
		{
			graph_currentValue++;
			if (graph_currentValue >= data[0].value_names.size()) 
				graph_currentValue = 0;
			updateGraph();
		}
		if (element == ctd_data_graph_prev)
		{
			graph_currentValue--;
			if (graph_currentValue < 0) 
				graph_currentValue = data[0].value_names.size() - 1;
			updateGraph();
		}
		if (element == ctd_data_graph_play)
		{
			play = ctd_data_graph_play->isToggled();
		}
	}

	// Callback for rendering, inherited from VRRenderHandler
	virtual void onVRRenderGraphicsContext(const VRGraphicsState& state) {
		if (!texturesloaded)
		{
			for (int i = 0; i < data.size(); i++)
				uploadTextures(data[i]);

			texturesloaded = true;
			updateMenus();
			displayMenu(currentMenu);
		}

		if(fabs(movement_x) > 0.1 || fabs(movement_y) > 0.1){
			VRVector3 offset = 0.1 * controllerpose * VRVector3(0, 0, movement_y);
			VRMatrix4 trans = VRMatrix4::translation(offset);
			roompose = trans * roompose;

			VRMatrix4 rot = VRMatrix4::rotationY(movement_x / 10 / CV_PI);
			roompose = rot * roompose;	
		}
		if (mode == 2)
		{
			if (play){
				float tmp = MOVIE_FPS_MODIFIER + currentSet;
				if (tmp > data.size()) tmp = 0;
				setCurrentSet(tmp);
			}
			else
			{
				setCurrentSet(currentSet);
			}
		}
		else{
			setCurrentSet();
		}
	}

	// Callback for rendering, inherited from VRRenderHandler
    virtual void onVRRenderGraphics(const VRGraphicsState &state) {
		glDisable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glClearDepth(1.0f);
		glClearColor(0.0, 0.0, 0.0, 1.f);
		glDisable(GL_LIGHTING);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(state.getProjectionMatrix());

		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(state.getViewMatrix());

    		glPushMatrix();
			glMultMatrixf(roompose.getArray());
			int maxRange = (mode ==0) ? SHOW_LIMIT : (mode ==1)? 100000 : 0;
			int start = currentSet - maxRange;
			int end = currentSet + maxRange;
			if (start < 0)
				start = 0;
			if (end > data.size() - 1)
				end = data.size() - 1;

			for (int i = start; i <= end; i++)
				drawQuads(data[i]);

			glEnable(GL_DEPTH_TEST);
			if (draw_Boundary)
			{
				for (int i = start; i <= end; i++)
					drawBoundaries(data[i]);
			}
			if (trace)
				drawTraces(currentSet);

		glPopMatrix();



		if (show_info){
			if (hoverHologram != NULL)
			{
				std::cerr << "Hover" << std::endl;
				glPushMatrix();
				glMultMatrixf(roompose.getArray());
				glBegin(GL_LINE_STRIP);
				glColor3f(1.0f, 1.0f, 0.5f);
				double offset = (mode == 0) ? -hoverHologram->setID  * hologramSize[2] : 0;
				glVertex3f(hoverHologram->vertices[0][0], hoverHologram->vertices[0][1], hoverHologram->vertices[0][2] + offset);
				glVertex3f(hoverHologram->vertices[1][0], hoverHologram->vertices[1][1], hoverHologram->vertices[1][2] + offset);
				glVertex3f(hoverHologram->vertices[2][0], hoverHologram->vertices[2][1], hoverHologram->vertices[2][2] + offset);
				glVertex3f(hoverHologram->vertices[3][0], hoverHologram->vertices[3][1], hoverHologram->vertices[3][2] + offset);
				glVertex3f(hoverHologram->vertices[0][0], hoverHologram->vertices[0][1], hoverHologram->vertices[0][2] + offset);
				glEnd();


				std::vector<std::string> text;
				//text.push_back("Type:  " + hoverHologram->type);
				text.push_back("ESD:  " + std::to_string((long double)hoverHologram->esd));
				text.push_back("ESV:  " + std::to_string((long double)hoverHologram->esv));
				VRFontHandler::getInstance()->renderMultiLineTextBox(text,
					hoverHologram->vertices[1][0], hoverHologram->vertices[1][1] - 0.3, hoverHologram->vertices[0][2] + offset,
					0.6, 0.3, VRFontHandler::LEFT, (controllerpose * VRVector3(0,0,-1)).z > 0 );
				glPopMatrix();
			}
		}
		
		if (show_measure){
			if (measureSet)
			{
				glPushMatrix();
				glMultMatrixf(roompose.getArray());
				glBegin(GL_LINE_STRIP);
				glColor3f(0.9f, 0.0f, 0.0f);
				glVertex3f(startMeasure.x, startMeasure.y, startMeasure.z);
				glVertex3f(endMeasure.x, endMeasure.y, endMeasure.z);
				glEnd();

				double dist = std::sqrt(
					std::pow((startMeasure.x - endMeasure.x) * SCALE, 2) +
					std::pow((startMeasure.y - endMeasure.y) * SCALE, 2) +
					std::pow((startMeasure.z - endMeasure.z) * SCALE * Z_SCALE, 2)
					);

				VRFontHandler::getInstance()->renderTextBox(std::to_string((long double)dist),
					endMeasure.x, endMeasure.y - 0.1, endMeasure.z,
					0.6, 0.1, VRFontHandler::LEFT, (controllerpose * VRVector3(0, 0, -1)).z > 0);
				glPopMatrix();
			}
		}

		glPushMatrix();
			glMultMatrixf(controllerpose.getArray());
			glBegin(GL_LINES);                // Begin drawing the color cube with 6 quads
			// Back face (z = -1.0f)
			glColor3f(0.5f, 0.5f, 0.0f);     // Yellow
			glVertex3f(0.0f, 0.0f, -5.0f);
			glVertex3f(0.0f, 0.0f, 0.0f);
			glEnd();  // End of drawing color-cube
		glPopMatrix();

		if (show_menu || move_menu){
			drawMenus();
		}
	}

	void drawBoundaries(DataSet &set)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		double offset = (mode == 0) ? -set.id * hologramSize[2] : 0;
		double alpha = 1.0 / (3 * (std::fabs((float) currentSet - set.id) + 1));
		glColor4f(alpha* frame_strength, alpha* frame_strength, 0.0f, alpha * frame_strength);

		for (int i = 0; i < set.quads.size(); i++)
		{	
			glBegin(GL_LINE_STRIP);
			glVertex3f(-RATIO_P_TO_UM * min_Z, RATIO_P_TO_UM * min_Z, offset);
			glVertex3f(RATIO_P_TO_UM * min_Z, RATIO_P_TO_UM * min_Z, offset);
			glVertex3f(RATIO_P_TO_UM * min_Z, -RATIO_P_TO_UM * min_Z, offset);
			glVertex3f(-RATIO_P_TO_UM * min_Z, -RATIO_P_TO_UM * min_Z, offset);
			glVertex3f(-RATIO_P_TO_UM * min_Z, RATIO_P_TO_UM * min_Z, offset);
			glEnd();

			glBegin(GL_LINE_STRIP);
			glVertex3f(-RATIO_P_TO_UM * max_Z, RATIO_P_TO_UM * max_Z, -hologramSize[2] + offset);
			glVertex3f(RATIO_P_TO_UM * max_Z, RATIO_P_TO_UM * max_Z, -hologramSize[2] + offset);
			glVertex3f(RATIO_P_TO_UM * max_Z, -RATIO_P_TO_UM * max_Z, -hologramSize[2] + offset);
			glVertex3f(-RATIO_P_TO_UM * max_Z, -RATIO_P_TO_UM * max_Z, -hologramSize[2] + offset);
			glVertex3f(-RATIO_P_TO_UM * max_Z, RATIO_P_TO_UM * max_Z, -hologramSize[2] + offset);
			glEnd();

			glBegin(GL_LINES);
			glVertex3f(-RATIO_P_TO_UM * min_Z, RATIO_P_TO_UM * min_Z, offset);
			glVertex3f(-RATIO_P_TO_UM * max_Z, RATIO_P_TO_UM * max_Z, -hologramSize[2] + offset);

			glVertex3f(RATIO_P_TO_UM * min_Z, RATIO_P_TO_UM * min_Z,offset);
			glVertex3f(RATIO_P_TO_UM * max_Z, RATIO_P_TO_UM * max_Z, -hologramSize[2] + offset);

			glVertex3f(RATIO_P_TO_UM * min_Z, -RATIO_P_TO_UM * min_Z, offset);
			glVertex3f(RATIO_P_TO_UM * max_Z, -RATIO_P_TO_UM * max_Z, -hologramSize[2] + offset);

			glVertex3f(-RATIO_P_TO_UM * min_Z, -RATIO_P_TO_UM * min_Z, offset);
			glVertex3f(-RATIO_P_TO_UM * max_Z, -RATIO_P_TO_UM * max_Z, -hologramSize[2] + offset);
			glEnd();
		}
		glDisable(GL_BLEND);
	}

	void drawTraces(int frame)
	{
		for (int j = 0; j < data[frame].quads.size(); j++)
		{
			int id = data[frame].quads[j].ID;
			int prev_slot = j;
			for (int i = frame - 1; i > frame - 20 && i >= 0; i--)
			{
				int next_slot = getContourByID(i, id);

				//draw
				if (prev_slot >= 0 && next_slot >= 0){
					float pts[2][3] = { { 0, 0, 0 }, { 0, 0, 0 } };
					for (int y = 0; y < 3; y++)
					{
						for (int x = 0; x < 4; x++)
						{
							pts[0][y] += data[i + 1].quads[prev_slot].vertices[x][y];
							pts[1][y] += data[i].quads[next_slot].vertices[x][y];
						}
						pts[0][y] /= 4.0;
						pts[1][y] /= 4.0;
					}
					glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
					glBegin(GL_LINES);
					glVertex3f(pts[0][0], pts[0][1], pts[0][2]);
					glVertex3f(pts[1][0], pts[1][1], pts[1][2]);
					glEnd();
				}

				//next
				prev_slot = next_slot;
			}
		}
	}

	void drawQuads(DataSet &set)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA);
		double offset = (mode == 0) ? -set.id * hologramSize[2] : 0;
		glEnable(GL_TEXTURE_2D);

		for (int i = 0; i < set.quads.size(); i++)
		{
			
			glBindTexture(GL_TEXTURE_2D, set.quads[i].texture);
			glBegin(GL_QUADS);                // Begin drawing the color cube with 6 quads
			glColor3f(1.0f, 1.0f, 1.0f);   
			glTexCoord2f(0, 1);
			glVertex3f(set.quads[i].vertices[0][0], set.quads[i].vertices[0][1], set.quads[i].vertices[0][2] + offset);
			glTexCoord2f(1, 1);
			glVertex3f(set.quads[i].vertices[1][0], set.quads[i].vertices[1][1], set.quads[i].vertices[1][2] + offset);
			glTexCoord2f(1, 0);
			glVertex3f(set.quads[i].vertices[2][0], set.quads[i].vertices[2][1], set.quads[i].vertices[2][2] + offset);
			glTexCoord2f(0, 0);
			glVertex3f(set.quads[i].vertices[3][0], set.quads[i].vertices[3][1], set.quads[i].vertices[3][2] + offset);
			glEnd();		
		}
		glDisable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_BLEND);
	}

	void setCurrentSet(float id = -1)
	{ 
		float new_currentSet = id;
		if (new_currentSet < 0){
			new_currentSet = roompose.inverse().getColumn(3)[2] / -hologramSize[2];
			if (new_currentSet < 0) new_currentSet = 0;
			if (new_currentSet >= data.size()) new_currentSet = data.size() - 1;
		}
		if (new_currentSet != currentSet)
		{
			currentSet = new_currentSet;
			ctd_data_current_filename->setText(data[currentSet].filename);
			ctd_data_current_textBox_values->setText(data[currentSet].values);
			ctd_data_graph_graph->setCurrent(currentSet);
			if (data[currentSet].value_names.size() > graph_currentValue){
				ctd_data_graph_currentValue->setText(data[currentSet].values[graph_currentValue]);
			}
			else
			{
				ctd_data_graph_currentValue->setText("");
			}
		}
	} 

	void updateGraph(){
		if (data[0].value_names.size() > graph_currentValue){
			ctd_data_graph_ValueName->setText(data[0].value_names[graph_currentValue]);
		}
		else
		{
			ctd_data_graph_ValueName->setText("");
		}
		if (data[currentSet].value_names.size() > graph_currentValue){
			ctd_data_graph_currentValue->setText(data[currentSet].values[graph_currentValue]);
		}
		else
		{
			ctd_data_graph_currentValue->setText("");
		}
		ctd_data_graph_graph->setData(getDataFromHologram(graph_currentValue));
	}

	void unloadQuads()
	{

	}

	void centerHologram(DataSet &set)
	{
		double x = 0;
		double y = 0;
		double z = 0;

		for (int i = 0; i < set.quads.size(); i++)
		{
			for (int j = 0; j <4; j++)
			{
				x += set.quads[i].vertices[j][0];
				y += set.quads[i].vertices[j][1];
				z += set.quads[i].vertices[j][2];
			}
		}

		x = x / set.quads.size() / 4;
		y = y / set.quads.size() / 4;
		z = z / set.quads.size() / 4;

		double offset = (mode == 0) ? set.id * hologramSize[2] : 0;
		roompose = VRMatrix4::translation(VRVector3(0, 0, hologramSize[2] *0.5 + offset));
	}

	void computeHologramSize()
	{
		VRVector3 min_vector3 = VRVector3(10000000, 10000000, 10000000);
		VRVector3 max_vector3 = VRVector3(-10000000, -10000000, -10000000);
		for (std::vector<DataSet>::const_iterator it = data.begin(); it != data.end(); ++it)
		{
			for (std::vector<hologram>::const_iterator it_h = it->quads.begin(); it_h != it->quads.end(); ++it_h)
			{
				for (int j = 0; j <4; j++)
				{
					for (int k = 0; k <3; k++)
					{
						if (min_vector3[k] > it_h->vertices[j][k])
							min_vector3[k] = it_h->vertices[j][k];
						if (max_vector3[k] < it_h->vertices[j][k])
							max_vector3[k] = it_h->vertices[j][k];
					}
				}
			}
		}

		hologramSize = max_vector3 - min_vector3;
		hologramSize[2] = (max_Z - min_Z) / SCALE / Z_SCALE;;
	}

	void uploadTextures(DataSet &set)
	{
		
			for (int i = 0; i < set.quads.size(); i++)
			{
				//use fast 4-byte alignment (default anyway) if possible
				//glPixelStorei(GL_UNPACK_ALIGNMENT, (opencvImages[i].step & 3) ? 1 : 4);

				//set length of one complete row in data (doesn't need to equal image.cols)
				//glPixelStorei(GL_UNPACK_ROW_LENGTH, opencvImages[i].step / opencvImages[i].elemSize());

				glGenTextures(1, &set.quads[i].texture);
				glBindTexture(GL_TEXTURE_2D, set.quads[i].texture);

				glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

				// Set texture clamping method
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

				glTexImage2D(GL_TEXTURE_2D,     // Type of texture
					0,                 // Pyramid level (for mip-mapping) - 0 is the top level
					GL_RGBA,            // Internal colour format to convert to
					set.opencvImages[i].cols,          // Image width  i.e. 640 for Kinect in standard mode
					set.opencvImages[i].rows,          // Image height i.e. 480 for Kinect in standard mode
					0,                 // Border width in pixels (can either be 1 or 0)
					GL_RGBA, // Input image format (i.e. GL_RGB, GL_RGBA, GL_BGR etc.)
					GL_UNSIGNED_BYTE,  // Image data type
					set.opencvImages[i].ptr());        // The actual image data itself

				glBindTexture(GL_TEXTURE_2D, 0);
			}

			
			set.opencvImages.clear();
		
	}

protected:
	
	bool texturesloaded;

	hologram* hoverHologram;

	std::vector<VRMenu*> menus;
	int currentMenu;
	bool menuVisible;
	//VRMenu * ctd_data_current_menu;
	VRTextBox * ctd_data_current_filename;
	VRMultiLineTextBox * ctd_data_current_textBox_valueNames;
	VRMultiLineTextBox * ctd_data_current_textBox_values;

	//VRMenu * ctd_data_graph_menu;
	VRGraph * ctd_data_graph_graph;
	VRButton * ctd_data_graph_next;
	VRButton * ctd_data_graph_prev;
	VRTextBox * ctd_data_graph_currentValue;
	VRTextBox * ctd_data_graph_ValueName;
	VRToggle * ctd_data_graph_play;
	int graph_currentValue;

	bool measuring;
	bool measureSet;
	VRPoint3 startMeasure;
	VRPoint3 endMeasure;

	VRMatrix4 menupose;
	VRMatrix4 controllerpose;
	VRMatrix4 roompose;
	float currentSet;
	float movement_y, movement_x;
	VRVector3 hologramSize;
};


int main(int argc, char **argv) {
	MyVRApp app(argc, argv, argv[1]);
  	app.run();
	exit(0);
}

