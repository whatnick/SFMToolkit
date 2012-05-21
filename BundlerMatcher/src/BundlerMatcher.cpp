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

#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <sstream>
#include <math.h>

#define GL_RGB  0x1907
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401

#include <IL/il.h>

BundlerMatcher::BundlerMatcher(float distanceThreshold, float ratioThreshold, int firstOctave, bool binaryWritingEnabled,
	bool sequenceMatching, int sequenceMatchingLength, bool tileMatching, int tileNum)
{
	mBinaryKeyFileWritingEnabled = binaryWritingEnabled;
	mSequenceMatchingEnabled     = sequenceMatching;
	mSequenceMatchingLength      = sequenceMatchingLength;
	mTiledMatchingEnabled = tileMatching;
	mTileNum = tileNum;

	//DevIL init
	ilInit();
	ilOriginFunc(IL_ORIGIN_UPPER_LEFT);
	ilEnable(IL_ORIGIN_SET);

	std::cout << "[BundlerMatcher]"<<std::endl;
	std::cout << "[Initialization]";

	mIsInitialized = true;
	mDistanceThreshold = distanceThreshold; //0.0 means few match and 1.0 many match (0.0-infinity)
	mRatioThreshold = ratioThreshold; //0.1 means few matches and 1.0 has no effect (0.1-1.0)

	char fo[10];
	sprintf(fo, "%d", firstOctave);
	char* args[] = {"-fo", fo};

	mSift = new SiftGPU;	
	mSift->ParseParam(2, args);	
	mSift->SetVerbose(-2);

	int support = mSift->CreateContextGL();
	if (support != SiftGPU::SIFTGPU_FULL_SUPPORTED)
		mIsInitialized = false;

	if (mIsInitialized)
		mSift->AllocatePyramid(12800, 12800);

	mMatcher = new SiftMatchGPU(8192);
}

BundlerMatcher::~BundlerMatcher()
{
	//DevIL shutdown
	ilShutDown();
}

bool BundlerMatcher::keyAsciiExists(const std::string& imagefile)
{
	std::stringstream filepath;
	filepath << mInputPath << imagefile.substr(0, imagefile.size()-4) << ".key";

	std::ifstream keyfile(filepath.str());
	return keyfile.good();
}

bool BundlerMatcher::keyBinaryExists(const std::string& imagefile)
{
	std::stringstream filepath;
	filepath << mInputPath << imagefile.substr(0, imagefile.size()-4) << ".key.bin";

	std::ifstream keyfile(filepath.str());
	return keyfile.good();
}

void BundlerMatcher::open(const std::string& inputPath, const std::string& inputFilename, const std::string& outMatchFilename)
{
	mInputPath = inputPath;

	if (!mIsInitialized)
	{
		std::cout << "Error : can not initialize opengl context for SiftGPU" <<std::endl;
		return;
	}

	if (!parseListFile(inputFilename))
	{
		std::cout << "Error : can not open file : " <<inputFilename.c_str() <<std::endl;
		return;	
	}
	
	//Sift Feature Extraction
	//Estimate total RAM usage
	long featuresum = 0;
	for (unsigned int i=0; i<mFilenames.size(); ++i)
	{	
		int percent = (int)(((i+1)*100.0f) / (1.0f*mFilenames.size()));
		if(!(keyAsciiExists(mFilenames[i]) || keyBinaryExists(mFilenames[i])))
		{				
			int nbFeature = extractSiftFeature(i);
			featuresum += nbFeature;
			int totalRAM = (featuresum*mFilenames.size())/((i+1)*2097152);
			saveAsciiKeyFile(i);
			if (mBinaryKeyFileWritingEnabled)
				saveBinaryKeyFile(i);
			clearScreen();
			std::cout << "[Saving Sift Key files: ("<<totalRAM<< "GB) "<< percent << "%] - ("<<i+1<<"/"<<mFilenames.size()<<") #" << nbFeature <<" features";
		}
		else
		{
			//Populate internal table with existing key file data
			//TODO: What about binary key files ?
			int nbFeature = readAsciiKeyFile(i);
			featuresum += nbFeature;
			int totalRAM = (featuresum*mFilenames.size())/((i+1)*2097152);
			clearScreen();
			std::cout << "[Reading Sift Key files: ("<<totalRAM<< "GB) "<< percent << "%] - ("<<i+1<<"/"<<mFilenames.size()<<") #" << nbFeature <<" features";
		}
	}
	clearScreen();
	std::cout << "[Sift Feature extracted]"<<std::endl;	
	saveVector();
	clearScreen();		
	std::cout << "[Sift Key files saved]"<<std::endl;	

	delete mSift;
	mSift = NULL;

	mMatcher->VerifyContextGL();

	//Sift Matching
	int currentIteration = 0;

	if (mSequenceMatchingEnabled) //sequence matching (video input)
	{
		std::cout << "[Sequence matching enabled: length " << mSequenceMatchingLength << "]" << std::endl;
		int maxIterations = (int) (mFilenames.size()-mSequenceMatchingLength)*mSequenceMatchingLength + mSequenceMatchingLength*(mSequenceMatchingLength-1)/2; // (N-m).m + m(m-1)/2
		for (unsigned int i=0; i<mFilenames.size()-1; ++i)
		{
			for (int j=1; j<=mSequenceMatchingLength; ++j)
			{
				int indexA = i;
				int indexB = i+j;

				if (indexB >= mFilenames.size())
					continue;
				else
				{
					clearScreen();
					int percent = (int) (currentIteration*100.0f / maxIterations*1.0f);
					std::cout << "[Matching Sift Feature : " << percent << "%] - (" << indexA << "/" << indexB << ")";
					matchSiftFeature(indexA, indexB);
					currentIteration++;
				}
			}
		}
	}
	else //classic quadratic matching
	{
		int maxIterations = (int) mFilenames.size()*((int) mFilenames.size()-1)/2; // Sum(1 -> n) = n(n-1)/2
		for (unsigned int i=0; i<mFilenames.size(); ++i)
		{
			for (unsigned int j=i+1; j<mFilenames.size(); ++j)
			{
				clearScreen();
				int percent = (int) (currentIteration*100.0f / maxIterations*1.0f);
				std::cout << "[Matching Sift Feature : " << percent << "%] - (" << i << "/" << j << ")";
				matchSiftFeature(i, j);
				currentIteration++;
			}
		}
	}

	clearScreen();
	std::cout << "[Sift Feature matched]"<<std::endl;

	delete mMatcher;
	mMatcher = NULL;

	saveMatches(outMatchFilename);
	saveMatrix();
}

bool BundlerMatcher::parseListFile(const std::string& filename)
{
	std::ifstream input(filename.c_str());
	if (input.is_open())
	{
		while(!input.eof())
		{
			std::string line;
			std::getline(input, line);
			if (line != "")
				mFilenames.push_back(line);
		}
	}
	input.close();

	return true;
}

int BundlerMatcher::extractSiftFeature(int fileIndex)
{
	std::stringstream filepath;
	filepath << mInputPath << mFilenames[fileIndex];

	std::string tmp = filepath.str();
	char* filename = &tmp[0];
	bool extracted = true;

	unsigned int imgId = 0;
	ilGenImages(1, &imgId);
	ilBindImage(imgId); 
	int nbFeatureFound = -1;

	if(ilLoadImage(filename))
	{
		int w = ilGetInteger(IL_IMAGE_WIDTH);
		int h = ilGetInteger(IL_IMAGE_HEIGHT);
		int format = ilGetInteger(IL_IMAGE_FORMAT);

		int wtile = w/mTileNum;
		int htile = h/mTileNum;

		SiftKeyDescriptors all_descriptors;
		SiftKeyPoints all_keys;

		for(int woff = 0; woff < w; woff+=wtile)
		{
			for(int hoff = 0; hoff < h; hoff+=htile)
			{

				//If the image is too large use ilCopyPixels to internal buffers
				//to copy subset of images to CPU RAM and call RunSIFT in a loop
				//which does not choke the Graphics RAM
				ILubyte* data = (ILubyte*)malloc(wtile*htile*sizeof(ILubyte));
				ilCopyPixels(woff,hoff,0,wtile,htile,1,IL_LUMINANCE,IL_UNSIGNED_BYTE,data);

				if (mSift->RunSIFT(wtile, htile, data, IL_LUMINANCE, GL_UNSIGNED_BYTE))
				{
					int num = mSift->GetFeatureNum();

					if(num>0)
					{
						SiftKeyDescriptors descriptors(128*num);
						SiftKeyPoints keys(num);

						mSift->GetFeatureVector(&keys[0], &descriptors[0]);

						if(nbFeatureFound == -1) nbFeatureFound = num;
						else nbFeatureFound += num;

						for(int i=0;i<keys.size();i++)
						{
							keys[i].x+=woff;
							keys[i].y+=hoff;
						}

						all_descriptors.insert(all_descriptors.end(),descriptors.begin(),descriptors.end());
						all_keys.insert(all_keys.end(),keys.begin(),keys.end());
					}
				}
				else
				{
					extracted = false;
				}

				free(data);
			}
		}

		//Save Feature in RAM
		//This can get filled up if the number of images is large
		mFeatureInfos.push_back(FeatureInfo(w, h, all_keys, all_descriptors));
	}
	else
	{
		extracted = false;
	}

	ilDeleteImages(1, &imgId); 

	if (!extracted)
	{
		std::cout << "Error while reading : " <<filename <<std::endl;
	}

	return nbFeatureFound;
}

void BundlerMatcher::matchSiftFeature(int fileIndexA, int fileIndexB)
{
	SiftKeyPoints pointsA           = mFeatureInfos[fileIndexA].points;
	SiftKeyDescriptors descriptorsA = mFeatureInfos[fileIndexA].descriptors;

	SiftKeyPoints pointsB           = mFeatureInfos[fileIndexB].points;
	SiftKeyDescriptors descriptorsB = mFeatureInfos[fileIndexB].descriptors;

	int max_size = std::max((int) pointsA.size(),(int) pointsB.size());

	mMatcher->SetDescriptors(0, (int) pointsA.size(), &descriptorsA[0]);
	mMatcher->SetDescriptors(1, (int) pointsB.size(), &descriptorsB[0]);
	
	int (*matchBuffer)[2] = new int[max_size][2];

	//This stage can be farmed off to a remote GPU
	mMatcher->SetMaxSift(max_size);
	int nbMatch = mMatcher->GetSiftMatch(max_size, matchBuffer, mDistanceThreshold, mRatioThreshold);

	//Save Match in RAM
	std::vector<Match> matches(nbMatch);
	for (int i=0; i<nbMatch; ++i)
		matches[i] = Match(matchBuffer[i][0], matchBuffer[i][1]);
	mMatchInfos.push_back(MatchInfo(fileIndexA, fileIndexB, matches));
	delete[] matchBuffer;
}

int BundlerMatcher::readAsciiKeyFile(int fileIndex)
{
	std::stringstream keyfilepath;
	keyfilepath << mInputPath << mFilenames[fileIndex].substr(0, mFilenames[fileIndex].size()-4) << ".key";

	std::stringstream filepath;
	filepath << mInputPath << mFilenames[fileIndex];

	std::string tmp = filepath.str();
	char* filename = &tmp[0];

	int num = 0; 
	int descCount = 0;

	if(ilLoadImage(filename))
	{
		int w = ilGetInteger(IL_IMAGE_WIDTH);
		int h = ilGetInteger(IL_IMAGE_HEIGHT);

		std::ifstream input(keyfilepath.str().c_str());
		if (input.is_open())
		{
			
			input >> num;
			input >> descCount;

			if(descCount!=128)
			{
				std::cout << "Error while reading key file, descriptor count invalid" << std::endl;
				return -1;
			}

			SiftKeyDescriptors descriptors(128*num);
			SiftKeyPoints keys(num);

			FeatureInfo info(w, h, keys, descriptors);

			float* pd = &info.descriptors[0];

			for (unsigned int i=0; i<num; ++i)
			{
				//in y, x, scale, orientation order
				input >> std::setprecision(2) >> info.points[i].y ;
				input >> std::setprecision(2) >> info.points[i].x ;
				input >> std::setprecision(3) >> info.points[i].s ;
				input >> std::setprecision(3) >> info.points[i].o ;
				for (int k=0; k<128; ++k, ++pd)
				{
					unsigned int feature;
					input >> feature;
					*pd = (((float)feature)/512.0f)-0.5;		
				}
			}

			mFeatureInfos.push_back(info);
		}

		input.close();
	}

	return num;
}

void BundlerMatcher::saveAsciiKeyFile(int fileIndex)
{	
	std::stringstream filepath;
	filepath << mInputPath << mFilenames[fileIndex].substr(0, mFilenames[fileIndex].size()-4) << ".key";

	std::ofstream output(filepath.str().c_str());
	if (output.is_open())
	{
		output.flags(std::ios::fixed);

		const FeatureInfo& info = mFeatureInfos[fileIndex];
		
		unsigned int nbFeature = (unsigned int) info.points.size();
		const float* pd = &info.descriptors[0];
		
		output << nbFeature << " 128" <<std::endl;

		for (unsigned int i=0; i<nbFeature; ++i)
		{
			//in y, x, scale, orientation order
			output << std::setprecision(2) << info.points[i].y << " " << std::setprecision(2) << info.points[i].x << " " << std::setprecision(3) << info.points[i].s << " " << std::setprecision(3) <<  info.points[i].o << std::endl;
			for (int k=0; k<128; ++k, ++pd)
			{
				output << ((unsigned int)floor(0.5+512.0f*(*pd)))<< " ";

				if ((k+1)%20 == 0) 
					output << std::endl;
			}
			output << std::endl;
		}
	}
	output.close();
}

void BundlerMatcher::saveBinaryKeyFile(int fileIndex)
{
	std::stringstream filepath;
	filepath << mInputPath << mFilenames[fileIndex].substr(0, mFilenames[fileIndex].size()-4) << ".key.bin";

	std::ofstream output;
	output.open(filepath.str().c_str(), std::ios::out | std::ios::binary);
	if (output.is_open())
	{
		int nbFeature = (int)mFeatureInfos[fileIndex].points.size();
		output.write((char*)&nbFeature, sizeof(nbFeature));

		FeatureInfo featureInfo = mFeatureInfos[fileIndex];
		for (int i=0; i<nbFeature; ++i)
		{			
			float x           = featureInfo.points[i].x;
			float y           = featureInfo.points[i].y;
			float scale       = featureInfo.points[i].s;
			float orientation = featureInfo.points[i].o;
			float* descriptor = &featureInfo.descriptors[i*128];
			output.write((char*)&x, sizeof(x));
			output.write((char*)&y, sizeof(y));
			output.write((char*)&scale, sizeof(scale));
			output.write((char*)&orientation, sizeof(orientation));
			output.write((char*)descriptor, sizeof(float)*128);	
		}
	}
	output.close();
}

void BundlerMatcher::clearScreen()
{
	std::cout << "\r                                                                          \r";
}

void BundlerMatcher::saveMatches(const std::string& filename)
{
	std::ofstream output;
	output.open(filename.c_str());
	for (unsigned int i=0; i<mMatchInfos.size(); ++i)
	{
		int nbMatch = (int) mMatchInfos[i].matches.size();

		output << mMatchInfos[i].indexA << " " << mMatchInfos[i].indexB << std::endl;		
		output << nbMatch <<std::endl;

		for (int j=0; j<nbMatch; ++j)
		{
			unsigned int indexA = mMatchInfos[i].matches[j].first;
			unsigned int indexB = mMatchInfos[i].matches[j].second;

			output << indexA << " " <<indexB << std::endl;
		}
	}
	output.close();
}

void BundlerMatcher::saveMatrix()
{
	std::ofstream output;
	output.open("matrix.txt");

	int nbFile = (int) mFilenames.size();

	std::vector<int> matrix(nbFile*nbFile);

	for (int i=0; i<nbFile*nbFile; ++i)
		matrix[i] = 0;

	for (unsigned int i=0; i<mMatchInfos.size(); ++i)
	{
		int indexA = mMatchInfos[i].indexA;
		int indexB = mMatchInfos[i].indexB;
		int nbMatch = (int) mMatchInfos[i].matches.size();
		matrix[indexA*nbFile+indexB] = nbMatch;
	}

	for (int i=0; i<nbFile; ++i)
	{
		for (int j=0; j<nbFile; ++j)	
		{
			output << matrix[i*nbFile+j] << ";";
		}
		output <<std::endl;
	}

	output.close();
}

void BundlerMatcher::saveVector()
{
	std::ofstream output;
	output.open("vector.txt");
	for (unsigned int i=0; i<mFeatureInfos.size(); ++i)
		output << mFeatureInfos[i].points.size() << std::endl;
	output.close();
}