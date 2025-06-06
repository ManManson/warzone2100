/*
	This file is part of Warzone 2100.
	Copyright (C) 2022  Warzone 2100 Project

	Warzone 2100 is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Warzone 2100 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Warzone 2100; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

// A WzMap::IOProvider implementation that uses libzip (https://libzip.org/) to support loading from zip archives

#pragma once

#include <wzmaplib/map_io.h>
#include <wzmaplib/map_debug.h>
#include <memory>
#include <vector>
#include <functional>
#include <cstdint>

class WrappedZipArchive;

class WzZipIOSourceReadProvider
{
public:
	WzZipIOSourceReadProvider();
	virtual ~WzZipIOSourceReadProvider();

	virtual optional<uint64_t> tell() = 0;
	virtual bool seek(uint64_t pos) = 0;
	virtual optional<uint64_t> readBytes(void *buffer, uint64_t len) = 0;
	virtual optional<uint64_t> fileSize() = 0;
	virtual optional<uint64_t> modTime() = 0;

	void inform_source_keep();
	void inform_source_free();
	void* error();
private:
	void *error_ = nullptr;
	size_t retainCount = 0;
};

class WzMapZipIO : public WzMap::IOProvider
{
private:
	WzMapZipIO() { }
public:
	// Initialize a new WzMapZipIO provider with a fileSystemPath to the .zip/.wz archive
	// Options:
	//	- extraConsistencyChecks: Enable extra consistency checks in libzip (see: ZIP_CHECKCONS)
	//	- readOnly: Open the zip archive in read-only mode
	static std::shared_ptr<WzMapZipIO> openZipArchiveFS(const char* fileSystemPath, bool extraConsistencyChecks = false, bool readOnly = false);

	// Initialize a new WzMapZipIO provider with a uint8_t buffer of the .zip/.wz archive data
	// Options:
	//	- extraConsistencyChecks: Enable extra consistency checks in libzip (see: ZIP_CHECKCONS)
	static std::shared_ptr<WzMapZipIO> openZipArchiveMemory(std::unique_ptr<std::vector<uint8_t>> zipFileContents, bool extraConsistencyChecks = false);

	// Initialize a new WzMapZipIO provider with a WzZipIOSourceReadProvider implementation of the .zip/.wz archive data
	// Options:
	//	- extraConsistencyChecks: Enable extra consistency checks in libzip (see: ZIP_CHECKCONS)
	static std::shared_ptr<WzMapZipIO> openZipArchiveReadIOProvider(std::shared_ptr<WzZipIOSourceReadProvider> zipSourceProvider, WzMap::LoggingProtocol* pCustomLogger = nullptr, bool extraConsistencyChecks = false);

	// Initialize a new WzMapZipIO provider with a fileSystemPath to a new .zip/.wz archive (to be written)
	static std::shared_ptr<WzMapZipIO> createZipArchiveFS(const char* fileSystemPath, bool fixedLastMod = false);

	// Initialize a new WzMapZipIO provider for writing, which will output the zip contents to a closure when closed
	typedef std::function<void (std::unique_ptr<std::vector<uint8_t>> zipFileContents)> CreatedMemoryZipOnCloseFunc;
	static std::shared_ptr<WzMapZipIO> createZipArchiveMemory(CreatedMemoryZipOnCloseFunc onCloseFunc, bool fixedLastMod = false);

	~WzMapZipIO();
public:
	virtual std::unique_ptr<WzMap::BinaryIOStream> openBinaryStream(const std::string& filename, WzMap::BinaryIOStream::OpenMode mode) override;
	virtual WzMap::IOProvider::LoadFullFileResult loadFullFile(const std::string& filename, std::vector<char>& fileData, uint32_t maxFileSize = 0, bool appendNullCharacter = false) override;
	virtual bool writeFullFile(const std::string& filename, const char *ppFileData, uint32_t fileSize) override;
	virtual bool makeDirectory(const std::string& directoryPath) override;
	virtual const char* pathSeparator() const override;
	virtual bool fileExists(const std::string& filePath) override;

	virtual bool enumerateFiles(const std::string& basePath, const std::function<bool (const char* file)>& enumFunc) override;
	virtual bool enumerateFolders(const std::string& basePath, const std::function<bool (const char* file)>& enumFunc) override;
	virtual bool enumerateFilesRecursive(const std::string& basePath, const std::function<bool (const char* file)>& enumFunc) override;
	virtual bool enumerateFoldersRecursive(const std::string& basePath, const std::function<bool (const char* file)>& enumFunc) override;

public:
	static std::string getZipLibraryVersionString();

private:
	bool determineIfMalformedWindowsPathSeparatorWorkaround();
	bool enumerateFilesInternal(const std::string& basePath, bool recurse, const std::function<bool (const char* file)>& enumFunc);
	bool enumerateFoldersInternal(const std::string& basePath, bool recurse, const std::function<bool (const char* file)>& enumFunc);

private:
	std::shared_ptr<WrappedZipArchive> m_zipArchive;
	std::vector<std::string> m_cachedDirectoriesList;
	bool m_fixedLastMod = false;
	// for best-effort handling of malformed / non-standard zip archives
	optional<bool> m_foundMalformedWindowsPathSeparators;
};
