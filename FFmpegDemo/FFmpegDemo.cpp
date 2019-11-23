
#include "mp4convert/TransCode.hpp"

int main(int argc, char* argv[]) {



	bool exit = false;
	TransCode transCode(exit);

	// string inputPath = "F://d_PT402_ch012019112016124820191120161258_1.mp4";
	// string outputPath = "F://kk.mp4";
    string inputPath = "F://ch01_20191121000002_20191121015621_0.mp4";
	string outputPath = "F://ssuqin.mp4";
	// string outputPath = "F://ssuqin_new.mp4";

	FFMpegUtil::Register();
	transCode.Initial(inputPath.c_str(), outputPath.c_str());
	transCode.TranscodeStep();
	
	getchar();

	return 0;
}