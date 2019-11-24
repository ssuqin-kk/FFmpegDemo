
#include "mp4convert/TransCode.hpp"

int main(int argc, char* argv[]) {



	bool exit = false;
	TransCode transCode(exit);

	// string inputPath = "F://xx.mp4";
	// string outputPath = "F://xx.mp4";
    string inputPath = "F://xx.mp4";
	string outputPath = "F://xx.mp4";
	// string outputPath = "F://xx_new.mp4";

	FFMpegUtil::Register();
	transCode.Initial(inputPath.c_str(), outputPath.c_str());
	transCode.TranscodeStep();
	
	getchar();

	return 0;
}
