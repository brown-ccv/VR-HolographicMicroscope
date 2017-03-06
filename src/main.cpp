
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
using namespace MinVR;

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc.hpp>

// Just included for some simple Matrix math used below
// This is not required for use of MinVR in general
#include <math/VRMath.h>

struct quad {
	float vertices[4][3];
	unsigned int texture;
};

struct DataSet
{
	std::vector <quad> quads;
	std::vector <cv::Mat> opencvImages;
};

std::vector <DataSet> data;

std::vector<string> ReadSubDirectories(const std::string &refcstrRootDirectory)
{
	std::vector<string> subdirectories;
	
	// subdirectories have been found
	HANDLE          hFile;                       // Handle to directory
	std::string     strFilePath;                 // Filepath
	std::string     strPattern;                  // Pattern
	WIN32_FIND_DATA FileInformation;             // File information


	strPattern = refcstrRootDirectory + "\\*.*";
	hFile = ::FindFirstFile(strPattern.c_str(), &FileInformation);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (FileInformation.cFileName[0] != '.')
			{
				strFilePath.erase();
				strFilePath = refcstrRootDirectory + "\\" + FileInformation.cFileName;

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

	return subdirectories;
}

/** MyVRApp is a subclass of VRApp and overrides two key methods: 1. onVREvent(..)
    and 2. onVRRenderGraphics(..).  This is all that is needed to create a
    simple graphics-based VR application and run it on any display configured
    for use with MinVR.
 */
class MyVRApp : public VRApp {
public:
	MyVRApp(int argc, char** argv, const std::string& configFile) : VRApp(argc, argv, configFile), texturesloaded(false), save(false), frame(0){
		folder = argv[2];
		computeCenter(data[0]);
		
    }

    virtual ~MyVRApp() {}

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

		if (startsWith(event.getName(), "HTC_Controller_1"))
		{
			if (event.getInternal()->getDataIndex()->exists("/HTC_Controller_1/State/Axis1Button_Pressed") &&
				(int)event.getInternal()->getDataIndex()->getValue("/HTC_Controller_1/State/Axis1Button_Pressed")){
				save = true;
			}
			else
			{
				save = false;
			}

			if(event.getInternal()->getDataIndex()->exists("/HTC_Controller_1/Pose")){
				controllerpose = event.getDataAsDoubleArray("Pose");
				controllerpose = controllerpose.transpose();
			}
			if (event.getInternal()->getDataIndex()->exists("/HTC_Controller_1/State/Axis0Button_Pressed")&&
				(int) event.getInternal()->getDataIndex()->getValue("/HTC_Controller_1/State/Axis0Button_Pressed")){
				double x = event.getInternal()->getDataIndex()->getValue("/HTC_Controller_1/State/Axis0/XPos");
				double y = event.getInternal()->getDataIndex()->getValue("/HTC_Controller_1/State/Axis0/YPos");
		
				bool rotate = false;
				if (fabs(x) > fabs(y)) rotate = true;
				
				//if (!rotate)
				{
					VRVector3 offset = 0.05 * controllerpose * VRVector3(0, 0, y);
					VRMatrix4 trans = VRMatrix4::translation(offset);
					for (int i = 0; i < 16; i++)
					{
						std::cerr << roompose.m[i] << " ";
					}
					std::cerr << std::endl;
					roompose = trans * roompose;
				}
				//else
				{
					VRMatrix4 rot = VRMatrix4::rotationY(x / 10 / CV_PI);
					roompose = rot * roompose;
				}

			}
		}
			//
        
		if (event.getName() == "KbdEsc_Down") {
            shutdown();
            return;
		}
        
	}

	// Callback for rendering, inherited from VRRenderHandler
	virtual void onVRRenderGraphicsContext(const VRGraphicsState& state) {
		if (!texturesloaded)
		{
			for (int i = 0; i < data.size(); i++)
				uploadTextures(data[i]);

			texturesloaded = true;
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
			glMultMatrixd(&roompose.m[0]);
			for (int i = 0; i < data.size();i++)
				drawQuads(data[i]);
		glPopMatrix();

		glPushMatrix();
			glMultMatrixd(&controllerpose.m[0]);
			glBegin(GL_LINES);                // Begin drawing the color cube with 6 quads
			// Back face (z = -1.0f)
			glColor3f(0.5f, 0.5f, 0.0f);     // Yellow
			glVertex3f(0.0f, 0.0f, -1.0f);
			glVertex3f(0.0f, 0.0f, 0.0f);
			glEnd();  // End of drawing color-cube
		glPopMatrix();
	}

	void drawQuads(DataSet &set)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA);
		for (int i = 0; i < set.quads.size(); i++)
		{
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, set.quads[i].texture);
			glBegin(GL_QUADS);                // Begin drawing the color cube with 6 quads
			glColor3f(1.0f, 1.0f, 1.0f);   
			glTexCoord2f(0, 1);
			glVertex3f(set.quads[i].vertices[0][0], set.quads[i].vertices[0][1], set.quads[i].vertices[0][2]);
			glTexCoord2f(1, 1);
			glVertex3f(set.quads[i].vertices[1][0], set.quads[i].vertices[1][1], set.quads[i].vertices[1][2]);
			glTexCoord2f(1, 0);
			glVertex3f(set.quads[i].vertices[2][0], set.quads[i].vertices[2][1], set.quads[i].vertices[2][2]);
			glTexCoord2f(0, 0);
			glVertex3f(set.quads[i].vertices[3][0], set.quads[i].vertices[3][1], set.quads[i].vertices[3][2]);

			glEnd();
			glDisable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
		glDisable(GL_BLEND);
	}

	void unloadQuads()
	{

	}


	void computeCenter(DataSet &set)
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

		roompose = VRMatrix4::translation(VRVector3(-x, -y, -z));
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
	
	std::string folder;
	bool texturesloaded;
	int frame;
	bool save;

	VRMatrix4 controllerpose;
	VRMatrix4 roompose;
};

void addQuad(float x, float y, float z, float width, float height, std::string filename, DataSet &set)
{
	float scale = 1000;
	quad q;
	q.vertices[0][0] = (x - width / 2) / scale;
	q.vertices[0][1] = (y + height / 2) / scale;
	q.vertices[0][2] = (z) / scale / 10;

	q.vertices[1][0] = (x + width / 2) / scale;
	q.vertices[1][1] = (y + height / 2) / scale;
	q.vertices[1][2] = (z) / scale / 10;

	q.vertices[2][0] = (x + width / 2) / scale;
	q.vertices[2][1] = (y - height / 2) / scale;
	q.vertices[2][2] = (z) / scale / 10;

	q.vertices[3][0] = (x - width / 2) / scale;
	q.vertices[3][1] = (y - height / 2) / scale;
	q.vertices[3][2] = (z) / scale / 10;

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

void loadDataSet(std::string folder)
{
	DataSet set;
	std::string reportName = folder + "\\reportRaw.xml";
	tinyxml2::XMLDocument doc;
	if (doc.LoadFile(reportName.c_str()) == tinyxml2::XML_SUCCESS){
		tinyxml2::XMLElement* titleElement;
		titleElement = doc.FirstChildElement("doc")->FirstChildElement("DATA");
		for (tinyxml2::XMLElement* child = titleElement->FirstChildElement("ROI"); child != NULL; child = child->NextSiblingElement())
		{
			std::cerr << child->FirstChildElement("IMAGE")->GetText() << std::endl;
			// do something with each child element
			addQuad(std::stof(child->FirstChildElement("X")->GetText()),
				std::stof(child->FirstChildElement("Y")->GetText()),
				std::stof(child->FirstChildElement("DEPTH")->GetText()),
				std::stof(child->FirstChildElement("WIDTH")->GetText()),
				std::stof(child->FirstChildElement("HEIGHT")->GetText()),
				folder + "\\" + child->FirstChildElement("IMAGE")->GetText(), set);
		}

	}
	data.push_back(set);
}

int main(int argc, char **argv) {
	std::vector<std::string> subdirs = ReadSubDirectories(argv[2]);
	for (int i = 0; i < subdirs.size(); i++)
		loadDataSet(std::string(argv[2]) + "//" + subdirs[i]);

    MyVRApp app(argc, argv, argv[1]);
  	app.run();
	exit(0);
}

