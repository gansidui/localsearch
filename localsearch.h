#ifndef __LOCAL_SEARCH_H__
#define __LOCAL_SEARCH_H__

#include <iostream>
#include <string>
#include <memory> 
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <regex>
#include <ctime>
#include <windows.h>

struct FileFrn {
	FileFrn(const std::wstring& _wsFileName, uint64_t _pFrn, char _volName) {
		wsFileName = _wsFileName;
		pFrn = _pFrn;
		volName = _volName;
	}
	std::wstring          wsFileName;  // 文件名 
	uint64_t              pFrn;        // 父目录的引用号 
	char                  volName;     // 盘符
};

struct FileInfo {
	std::wstring wsFileName;      // 文件名
	std::wstring wsFilePath;      // 文件路径
};

class LocalSearch {
public:
	LocalSearch();

	virtual ~LocalSearch();

	// 初始化
	void init();

	// 根据模式串pattern搜索
	unsigned int search(const std::wstring& wsPattern);

	// 返回 [beginPos,endPos)之间的搜索结果(分段展示)
	std::vector<std::shared_ptr<FileInfo>> getResult(unsigned int beginPos, unsigned int endPos);

protected:
	// 判断磁盘分区volName是不是NTFS文件格式，例如："C:\\"
	bool isNtfs(const std::string& volName);

	// 初始化磁盘分区volName的USN日志文件, 并返回其句柄
	HANDLE initUsn(const std::string& volName);

	// 获取磁盘分区volName的USN记录并保存
	bool getUsn(const std::string& volName);

	// 扫描所有磁盘分区
	void scanAllVol();

	// 根据FileFrn获得它的路径
	std::wstring getPathFromFileFrn(const FileFrn& fileFrn);

	// 根据FileFrn获得它的FileInfo
	std::shared_ptr<FileInfo> getFileInfoFromFileFrn(const FileFrn& fileFrn);

private:
	// 所有NTFS文件系统的分区根目录的引用号为常量.
	static const uint64_t ROOT_FRN         = 1407374883553285;
	static const unsigned int MAX_VOLUME   = 26;

	// 保存文件的引用号信息
	std::vector<std::unique_ptr<FileFrn>>       vecFileFrn;
	std::vector<unsigned int>                   vecSortedIndexOfFileFrn;
	std::vector<unsigned int>                   vecSearchResult;

	// 每个分区的文件引用号映射到vecFileFrn的index，用于计算文件路径，同一个分区上的文件引用号是唯一的($开头的文件除外)
	std::unordered_map<uint64_t, unsigned int>  umFrnMapIdx[MAX_VOLUME];
};

#endif // __LOCAL_SEARCH_H__