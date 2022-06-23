#pragma once

#include <string>;
#include <vector>;
#include <mutex>;

namespace TestTask
{
	enum class FileState { readonly, writeonly };
	
	struct File 
	{
		FileState state { FileState::writeonly };
		const char* name {};
	};

	struct IVFS
	{
		std::mutex mtx {}; // Замок для потока		
		std::vector<File*> filesInUse {}; // Используемые на данный момент файлы

		bool IsExist(const char* name); // Checks is file exists
		size_t GetFilesSize(); // Check the size of the file
		File* Open(const char* name); // Open a file and saves the pointer to the structure
		File* Create(const char* name); // Creates virtual file in the required directory
		size_t Read(File* f, char* buff, size_t len); // Reads the file and return its size
		size_t Write(File* f, char* buff, size_t len); // Writes data into the file and return size of the written data
		void Close(File* f); // Close the file and remove the pointer
	};
	
}
