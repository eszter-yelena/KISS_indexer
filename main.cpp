/*
	MAIN.CPP
	--------
	indexReference

	Created by Shlomo Geva on 16/7/2023.
*/

#include <map>
#include <thread>
#include <chrono>
#include <sstream>
#include <fstream>
#include <iostream>

#include "indexGenome.hpp"
#include "protected_vector.hpp"
#include "serialiseKmersMap.hpp"

std::vector<std::string> sampleSequences;
std::vector<std::string> fileNames;

// some global default values (overide with cmd line arguments)
std::string REFERENCE = ""; // file name for reference file to match against

/*
	WRITEMAPTOFILE()
	----------------
*/
void writeMapToFile(const std::string &filename, const std::map<uint32_t, std::string> &referenceIDMap)
	{
	std::ofstream outFile(filename);
	if (!outFile)
		{
		std::cerr << "Error opening the file: " << filename << std::endl;
		return;
		}

	for (const auto& entry : referenceIDMap)
		outFile << entry.first << " " << entry.second; // << std::endl;

	outFile.close();
	}

/*
	GETBASENAME()
	-------------
*/
std::string getBaseName(const std::string& filePath)
	{
	std::stringstream ss(filePath);
	std::string baseName;
	std::getline(ss, baseName, '.');
	return baseName;
	}

/*
	GETREFERENCE()
	--------------
*/
void getReference(std::string inputFile)
	{
	std::map<uint32_t, std::string> referenceIDMap;
	char *genome = nullptr;
	uint32_t MASK = 0;

    /*
		load the genome
	*/
    auto start = std::chrono::steady_clock::now();
    uint64_t genomeSize;
    genome = load_genome_file(REFERENCE, referenceIDMap, genomeSize);

	/*
		Calculate the number of elements to reserve in kmersIndex based on genome size
	*/
	int numBitsToKeep = ::ceil(::log2(genomeSize));
	MASK = (numBitsToKeep == 32) ? UINT32_MAX : (1 << numBitsToKeep) - 1;
	std::cout << "Keeping " << numBitsToKeep << " bits in kmerHash" << std::endl;
	std::vector<protected_vector<uint32_t>> kmersMap(pow(2, numBitsToKeep));

	/*
		Now index
	*/
    index_kmers(genome, genomeSize, kmersMap, MASK);

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    int minutes = (int) duration.count() / (1000 * 60);
    float seconds = ((duration.count() - minutes * 1000 * (float) 60))/1000;
    std::cout << "Building time: " << minutes << " min " << seconds << " sec" << std::endl;

	/*
		Compute global index statistics including the number of "words", number of unique "words" (including colisions), et.
	*/
	uint64_t kmerCount = genomeSize - 32;
	uint64_t kmersInMap = 0;
	for (int i = 0; i < kmersMap.size(); i++)
		if (!kmersMap[i].empty())
			kmersInMap++;
	std::cout  << "Map size " << kmersMap.size() << ", kmersCount " << kmerCount << ", kmers in Map " << kmersInMap << std::endl;

    /*
		Serialize the map
	*/
    start = std::chrono::steady_clock::now();
    std::string outerMapFilename = getBaseName(inputFile) + "_32_OuterBlob.idx";
    std::string innerMapFilename = getBaseName(inputFile) + "_32_InnerBlob.idx";
    std::string genomeFilename = getBaseName(inputFile) + "_genome.idx";
    std::string refIDFilename = getBaseName(inputFile) + "_refID.idx";

    std::cout << "Serialising genome to " << genomeFilename << " and " << innerMapFilename << std::endl;
    writeTextBlobToFile(genome, genomeSize, genomeFilename);
    end = std::chrono::steady_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    minutes = (int) duration.count() / (1000 * 60);
    seconds = ((duration.count() - minutes * 1000 * (float) 60))/1000;
    std::cout << "Serialising genome time: " << minutes << " min " << seconds << " sec" << std::endl;

    std::cout << "Serialising map to " << outerMapFilename << " and " << innerMapFilename << std::endl;
    serializeMap(kmersMap, innerMapFilename, outerMapFilename);
    end = std::chrono::steady_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    minutes = (int) duration.count() / (1000 * 60);
    seconds = ((duration.count() - minutes * 1000 * (float) 60))/1000;
    std::cout << "Serialising Maps time: " << minutes << " min " << seconds << " sec" << std::endl;
    
    std::cout << "Serialising ReferenceIDMap" << std::endl;
    writeMapToFile(refIDFilename, referenceIDMap);
        
    // DeSerialize the genome
    start = std::chrono::steady_clock::now();
    // Read the text blob from the file and directly assign to genome and textLength
    std::tie(genome, genomeSize) = readTextBlobFromFile(genomeFilename);
    end = std::chrono::steady_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    minutes = (int) duration.count() / (1000 * 60);
    seconds = ((duration.count() - minutes * 1000 * (float) 60))/1000;
    std::cout << "DeSerialising genome time: " << minutes << " min " << seconds << " sec" << std::endl;

    // DeSerialize the map
    start = std::chrono::steady_clock::now();
    std::vector<uint32_t> innerMapBlob;
    std::vector<uint32_t> outerMapBlob;
    deserializeMap(innerMapFilename, outerMapFilename, innerMapBlob, outerMapBlob);
    end = std::chrono::steady_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    minutes = (int) duration.count() / (1000 * 60);
    seconds = ((duration.count() - minutes * 1000 * (float) 60))/1000;
    std::cout << "DeSerialising Maps time: " << minutes << " min " << seconds << " sec" << std::endl;

	/*  SANITY TEST CODE, ignore
		// test the index
		// test the index
		// Use the helper function to access the map elements
		size_t index;
		for (index=1000000; index<10000000; index++) {
			if (!(kmersMap[index].empty())) {
				break;
			}
		}
		// Use the helper function to access the map elements
		std::vector<uint32_t> innerVector = getInnerVector(innerMapBlob, outerMapBlob, index);
		// Output the inner vector retrieved from the blob
		for (const auto& value : innerVector) {
			std::cout << value << " ";
		}
		std::cout << std::endl;
		for (const auto& value : kmersMap[index]) {
			std::cout << value << " ";
		}
		std::cout << std::endl;
	*/
	}

/*
	INITIALISE()
	------------
*/
void intialise(int argc, char *argv[])
	{
	if ((argc <= 1) || strcmp(argv[1], "-help") == 0)
		{
		std::cout << "Usage:  " << argv[0] << " -reference <reference_filename>\n";
		std::cout << "example:" << argv[0] << " -reference CutibacteriumGenome.fasta\n";
		exit(0);
		}

	for (int i = 1; i < argc; i++)
		{
		std::string arg = argv[i];
		if (i + 1 >= argc)
			{
			std::cout << "Error: Missing value for " << arg << " option." << std::endl;
			continue;
			}

		std::string value = argv[++i];

		// Process the command line option
		if (arg == "-reference")
			REFERENCE = value;
		else
			std::cerr << "Error: Unknown option: " << arg << std::endl;
		}

	std::cout << "indexReference run parameters\n";
	std::cout << "reference: " << REFERENCE << "\n";
	}

/*
	MAIN()
	------
*/
int main(int argc, char *argv[])
	{
	// Start the timer
	auto startAll = std::chrono::steady_clock::now();

	// set up KISS parameters
	intialise(argc, argv);
	getReference(REFERENCE); // load the reference collection index

	// report overall program duration
	auto endAll = std::chrono::steady_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endAll - startAll);
	int minutes = (int)duration.count() / (1000 * 60);
	float seconds = ((duration.count() - minutes * 1000 * (float) 60))/1000;
	std::cout << "\nTotal time: " << minutes << " min " << seconds << " sec" << std::endl;

	return 0;
	}