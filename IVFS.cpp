#include "IVFS.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <cassert>
#include <future>
#include <boost/filesystem.hpp>

using namespace TestTask;

std::ostream& operator<< (std::ostream& out, const FileState& state)
{
	if (state == FileState::readonly) {
		out << "readonly";
	}
	else {
		out << "writeonly";
	}

	return out;
}

bool IVFS::IsExist(const char* name)
{
	std::fstream stream(name);
	bool result = stream.good();
	stream.close();

	return result;	
}

size_t IVFS::GetFilesSize() {
	mtx.lock();
	size_t result = filesInUse.size();
	mtx.unlock();

	return result;
}

File* IVFS::Open(const char* name)
{
	// Check if file exists
	if (!IsExist(name))
	{
		return nullptr;
	}
	
	// Check if it is already exist in the used files vector
	mtx.lock();
	for (File* file : filesInUse)
	{
		if (file->name == name)
		{
			if (file->state == FileState::readonly)
			{
				mtx.unlock();
				return file;
			}
			else 
			{
				mtx.unlock();
				return nullptr;
			}
			
		}
	}

	// Create file

	File* file = new File();
	file->state = FileState::readonly;
	file->name = name;
	filesInUse.push_back(file);
	mtx.unlock();

	return file;
}

File* IVFS::Create(const char* name)
{
	// First check if such file already exists
	mtx.lock();
	for (File* file : filesInUse)
	{
		if (file->name == name)
		{
			if (file->state == FileState::writeonly)
			{
				mtx.unlock();
				return file;
			}
			else {
				mtx.unlock();
				return nullptr;
			}

		}
	}

	mtx.unlock();

	// Create directories and file if required
	std::istringstream stringReader(name);
	char delimiter = '\\';
	std::string newLine;
	std::string folderPath;
	while (std::getline(stringReader, newLine, delimiter)) 
	{
		if (newLine.find('.') == std::string::npos) 
		{
			/*std::cout << newLine << std::endl;*/
			folderPath += newLine + '\\';
		}
		
	}
	
	// Create forlders (can be createw with system mkdir command for Windows)
	if (!folderPath.empty()) 
	{
		boost::filesystem::create_directories(folderPath);
	}

	// Create file
	mtx.lock();
	File* file = new File();
	file->name = name;
	filesInUse.push_back(file);
	mtx.unlock();

	return file;
}

size_t IVFS::Read(File* f, char* buff, size_t len)
{
	size_t resultFileSize = 0;  

	// Check if we can read file as well as its existence
	mtx.lock();
	if (f->state == FileState::writeonly || !IsExist(f->name)) 
	{
		mtx.unlock();
		return 0;
	}
	mtx.unlock();

	
	// Read the data and fill the buffer with data line
	std::fstream fileReader(f->name);
	/*char* readData = new char[len];*/
	unsigned char newChar;
	unsigned int i = 0;
	while (fileReader >> std::noskipws >> newChar && i < len)
	{
		buff[i] = newChar;
		i++;

		// 1 char is 1 byte
		resultFileSize += sizeof(newChar);
	}
	/*buff[i] = '\0';*/

	// What should be done if read data is smaller than the allocated memory?

	fileReader.close();

	return resultFileSize;
}

size_t IVFS::Write(File* f, char* buff, size_t len)
{
	size_t resultFileSize = 0;

	// Check if we can write to the file as well as its existence
	mtx.lock();
	if (f->state == FileState::readonly || !IsExist(f->name))
	{
		mtx.unlock();
		return 0;
	}
	mtx.unlock();

	// Write data to the file
	std::ofstream fileWriter(f->name);
	/*fileWriter << buff;*/

	for (unsigned int i = 0; i < len; i++) {
		fileWriter << buff[i];
		resultFileSize += sizeof(buff[i]);
	}

	fileWriter.close();

	// Isn't it just length of the buffer?
	/*resultFileSize += len * sizeof(char);*/

	return resultFileSize;
}

void IVFS::Close(File* f)
{
	// Delete if from the vector and free allocated memory
	mtx.lock();
	std::vector<File*>::iterator it = filesInUse.begin();
	while (it != filesInUse.end()) 
	{
		if ((*it) == f) 
		{
			filesInUse.erase(it);
			free(f);

			break;
		}
		it++;
	}
	mtx.unlock();
}

int main()
{
	// --- More tests ---
	// IVFS ivfs;
	//const char* name("FileFilder\\File.txt");
	//const unsigned int length = 10;
	//char message[length] = "file data";

	//File* file = ivfs.Create(name);
	//ivfs.Write(file, message, length);

	//std::cout << ivfs.GetFilesSize() << std::endl;

	//// Try to create same file
	//File* file2 = ivfs.Create(name);

	//std::cout << ivfs.GetFilesSize() << std::endl;

	//// Now read data and close file
	//ivfs.Close(file); // file2 is null as well

	//file = ivfs.Open(name);

	//char givenMessage[length] {};
	//ivfs.Read(file, givenMessage, length);

	//for (const auto& c : givenMessage) {
	//	std::cout << c;
	//}
	//std::cout << std::endl;

	//ivfs.Close(file);

	//std::cout << ivfs.GetFilesSize() << std::endl;

	// --- Multithreading ---
	IVFS ivfs;
	unsigned int threadNum = 5;

	// Create files in multiple threads
	std::vector<const char*> paths = { "FileFolder\\File1.txt", "FileFolder\\File2.txt", "FileFolder\\File3.txt",
		"FileFolder\\File4.txt", "FileFolder\\File5.txt" };
	std::vector<std::future<File*>> futureFiles(threadNum);
	std::vector<File*> files(threadNum);

	for (unsigned int i = 0; i < threadNum; i++) {
		futureFiles[i] = std::async(std::launch::async, &IVFS::Create, std::ref(ivfs), std::ref(paths.at(i)));
	}

	// Get files
	for (unsigned int i = 0; i < threadNum; i++) {
		files[i] = futureFiles[i].get();
	}

	std::cout << "File number: " <<ivfs.GetFilesSize() << std::endl;

	// Fill files with data
	const size_t charSize = 11;
	char message[charSize] = "file data ";
	for (unsigned int i = 0; i < threadNum; i++) {
		message[charSize - 1] = std::to_string(i)[0];
		std::async(std::launch::async, &IVFS::Write, std::ref(ivfs), std::ref(files.at(i)), std::ref(message), std::ref(charSize));
	}

	std::cout << "File number: " << ivfs.GetFilesSize() << std::endl;

	for (unsigned int i = 0; i < threadNum; i++) {
		std::async(std::launch::async, &IVFS::Close, std::ref(ivfs), std::ref(files.at(i)));
	}

	std::cout << "File number: " << ivfs.GetFilesSize() << std::endl;

	return 0;
}