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

#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <fstream>

#include "SiftGPU.h"

//GPU Buffer usage for large key-point set matching
#define MATCH_BUFFER 24576

typedef std::pair<unsigned int, unsigned int> Match;

typedef std::vector<SiftGPU::SiftKeypoint> SiftKeyPoints;
typedef std::vector<float> SiftKeyDescriptors;
typedef std::vector<Match> Pairs;

struct MatchInfo
{
	MatchInfo(int indexA, int indexB, std::vector<Match>& matches)
	{
		this->indexA = indexA;
		this->indexB = indexB;
		this->matches = matches;
	}

	int indexA;
	int indexB;
	std::vector<Match> matches;
};

struct FeatureInfo
{
	FeatureInfo(int width, int height, SiftKeyPoints& points, SiftKeyDescriptors& descriptors)
	{
		this->width  = width;
		this->height = height;
		this->points = points;
		this->descriptors = descriptors;
	}

	int width;
	int height;
	SiftKeyPoints points;
	SiftKeyDescriptors descriptors;
};

class BundlerMatcher
{
	public:
		BundlerMatcher(float distanceThreshold, float ratioThreshold, int firstOctave = 1,
			bool binaryWritingEnabled = false, bool sequenceMatching = false, int sequenceMatchingLength = 5,
			bool tileMatching = false, int tileNum = 1, float tilePercent = 1.0, bool pairMatchingEnabled = false);
		~BundlerMatcher();
		 
		//load list.txt and output gpu.matches.txt + one key file per pictures
		void open(const std::string& inputPath, const std::string& inputFilename, const std::string& outMatchFilename, 
			const std::string& pairsFilename);

	protected:
		
		//Feature extraction
		int extractSiftFeature(int fileIndex);
		void saveAsciiKeyFile(int fileIndex);
		void saveBinaryKeyFile(int fileIndex);
		int readAsciiKeyFile(int fileIndex);

		//Feature matching
		void matchSiftFeature(int fileIndexA, int fileIndexB);	
		void saveMatches(const std::string& filename);

		//Helpers
		bool parseListFile(const std::string& filename);
		bool parsePairsFile(const std::string& filename);
		bool keyAsciiExists(const std::string& imagefile);
		bool keyBinaryExists(const std::string& imagefile);
		void clearScreen();
		void saveMatrix();
		void saveVector();
public:	
		bool                     mIsInitialized;
		bool                     mBinaryKeyFileWritingEnabled;
		bool                     mSequenceMatchingEnabled;
		int                      mSequenceMatchingLength;
		bool					 mTiledMatchingEnabled;
		int						 mTileNum;
		float					 mTilePercent;
		bool					 mPairedMatchingEnabled;
		Pairs					 mPairs;
		SiftGPU*                 mSift;
		SiftMatchGPU*            mMatcher;
		//int                      mMatchBuffer[4096][2];
		float                    mDistanceThreshold;
		float					 mRatioThreshold;
		std::string              mInputPath;

		std::vector<std::string> mFilenames;    //N images
		std::vector<FeatureInfo> mFeatureInfos; //N FeatureInfo
		std::vector<MatchInfo>   mMatchInfos;   //N(N-1)/2 MatchInfo
};