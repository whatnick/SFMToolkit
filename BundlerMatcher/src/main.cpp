/*
	Copyright (c) 2010 ASTRE Henri (http://www.visual-experiments.com)

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.
*/

#include "BundlerMatcher.h"

int main(int argc, char* argv[])
{
	if (argc < 7)
	{
		std::cout << "Usage: " << argv[0] << " <inputPath> <list.txt> <outfile matches> <distanceThreshold> <ratioThreshold> <firstOctave>" <<std::endl;
		std::cout << "<distanceThreshold> : 0.0 means few match and 1.0 many match (float)" <<std::endl;
		std::cout << "<ratioThreshold> : 0.0 means few match and 1.0 many match (float)" <<std::endl;
		std::cout << "<firstOctave>: specify on which octave start sampling (int)" <<std::endl;
		std::cout << "<firstOctave>: low value (0) means many features and high value (2) means less features" << std::endl;		
		std::cout << "Optional feature:" << std::endl;
		std::cout << "	- bin: generate binary files (needed for Augmented Reality tracking)" << std::endl;
		std::cout << "	- sequence NUMBER: matching optimized for video sequence" << std::endl;
		std::cout << "		-> example: sequence 3 (will match image N with N+1,N+2,N+3)" <<std::endl;
		std::cout << "  - tile NUMBER: break image up in NUMBER of tiles in Width and Height" << std::endl;
		std::cout << "      -> example: tile 2 (will divide image into 4 tiles)" << std::endl;
		std::cout << "  - tilepercent FRACTION: use a fraction of the tile specified" << std::endl;
		std::cout << "      -> example: tilepercent 0.9 will use 90% of a given tile" << std::endl;
		std::cout << "  - pairs pairfile.txt: pairwise matching only using the pairs supplied" << std::endl;
		std::cout << "Example: " << argv[0] << " your_folder/ list.txt gpu.matches.txt 0.6 0.8 1" << std::endl;

		return -1;
	}

	bool binnaryWritingEnabled  = false;
	bool sequenceMatching       = false;
	int  sequenceMatchingLength = 0;
	bool tileMatching = false;
	int tileNum = 1;
	float tilePercent = 1.0f;
	bool pairMatching = false;
	std::string pairfile = "";

	for (int i=1; i<argc; ++i)
	{
		std::string current(argv[i]);
		if (current == "bin")
			binnaryWritingEnabled = true;
		else if (current == "sequence")
		{
			if (i+1<argc)
			{				
				sequenceMatchingLength = atoi(argv[i+1]);
				if (sequenceMatchingLength > 0)
					sequenceMatching = true;
				i++;
			}
		}
		else if (current == "tile")
		{
			if (i+1<argc)
			{				
				tileNum = atoi(argv[i+1]);
				if (tileNum > 1)
					tileMatching = true;
				i++;
			}
		}
		else if (current == "tilepercent")
		{
			if (i+1<argc)
			{				
				tilePercent = (float)atof(argv[i+1]);
				i++;
			}
		}
		else if (current == "pairs")
		{
			if (i+1<argc)
			{				
				pairfile = std::string(argv[i+1]);
				pairMatching = true;
				i++;
			}
		}
	}

	if(pairMatching && sequenceMatching)
	{
		std::cerr << "Can not enable both paired matching and sequence matching" << std::endl;
		return 1;
	}

	if(tilePercent>1.0 || tilePercent <= 0.0)
	{
		std::cerr << "Tile percent ["<<tilePercent<< "] invalid" << std::endl;
		return 1;
	}

	BundlerMatcher matcher((float) atof(argv[4]),(float) atof(argv[5]), atoi(argv[6]), binnaryWritingEnabled,
		sequenceMatching, sequenceMatchingLength, tileMatching, tileNum, tilePercent, pairMatching);
	matcher.open(std::string(argv[1]), std::string(argv[2]), std::string(argv[3]),pairfile);
	
	return 0;
}
