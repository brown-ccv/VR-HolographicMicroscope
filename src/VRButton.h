#ifndef VRVRBUTTON_H
#define VRVRBUTTON_H

#include "VRMenuElement.h"

class VRButton : public VRMenuElement {
	public:
		VRButton(std::string name, std::string text = "");
		virtual ~VRButton();

		virtual void draw();
		virtual void click(double x, double y, bool isDown);

	private:

	};

#endif //VRVRBUTTON_H