#include "localsearch.h"

LocalSearch::LocalSearch() {
	std::locale::global(std::locale(""));
	std::wcout.imbue(std::locale());
}

LocalSearch::~LocalSearch() {
	vecFileFrn.clear();
	vecSearchResult.clear();
	vecSortedIndexOfFileFrn.clear();

	for (auto &x : umFrnMapIdx) {
		x.clear();
	}
}

void LocalSearch::init() {
	clock_t startTime = clock();
	scanAllVol();
	clock_t finishTime = clock();

	std::cout << "scan all volume used time: " << (finishTime-startTime) << " ms" << std::endl;
	std::cout << "files total: " << vecFileFrn.size() << std::endl;

	// 按文件名排序
	vecSortedIndexOfFileFrn.reserve(vecFileFrn.size());
	for (unsigned int i = 0; i < vecFileFrn.size(); ++i) {
		vecSortedIndexOfFileFrn.push_back(i);
	}
	std::sort(vecSortedIndexOfFileFrn.begin(), vecSortedIndexOfFileFrn.end(), 
		[&](unsigned int i, unsigned int j){ return vecFileFrn[i]->wsFileName < vecFileFrn[j]->wsFileName; });
}

unsigned int LocalSearch::search(const std::wstring& wsPattern) {
	vecSearchResult.clear();
	std::wregex regExp(L"");

	try {
		regExp.assign(wsPattern);
	} catch(...) {
		regExp.assign(L"");
	}

	for (auto x : vecSortedIndexOfFileFrn) {
		if (std::regex_match(vecFileFrn[x]->wsFileName, regExp)) {
			vecSearchResult.push_back(x);
		}
	}

	return vecSearchResult.size();
}

std::vector<std::shared_ptr<FileInfo>> LocalSearch::getResult(unsigned int beginPos, unsigned int endPos) {
	std::vector<std::shared_ptr<FileInfo>> result;
	endPos = std::min(endPos, (unsigned int)vecSearchResult.size()); 
	for (unsigned int i = beginPos; i < endPos; ++i) {
		result.push_back(getFileInfoFromFileFrn(*(vecFileFrn[vecSearchResult[i]])));
	}

	std::cout << "result total: " << result.size() << std::endl;

	return result;
}

bool LocalSearch::isNtfs(const std::string& volName) {
	char sysName[MAX_PATH + 1] = { 0 };
	if (GetVolumeInformation(volName.c_str(), nullptr, 0, nullptr, nullptr, nullptr, sysName, MAX_PATH + 1)) {
		return strcmp(sysName, "NTFS") == 0;
	}
	return false;
}

HANDLE LocalSearch::initUsn(const std::string& volName) {
	// 只有NTFS文件系统才有USN日志 
	if (!isNtfs(volName)) {
		return INVALID_HANDLE_VALUE;
	}

	// 传入的文件名必须为 \\.\C: 的格式
	std::string fileName("\\\\.\\");
	fileName.append(volName);
	fileName.erase(fileName.find_last_of(":")+1);

	// 调用该函数需要管理员权限，返回驱动盘句柄 
	HANDLE hVol = CreateFile(
		fileName.c_str(),
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		0,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_READONLY,
		0);

	// 初始化USN日志文件
	if (hVol != INVALID_HANDLE_VALUE) {
		DWORD br;
		CREATE_USN_JOURNAL_DATA cujd;
		cujd.MaximumSize = 0;
		cujd.AllocationDelta = 0;

		if (DeviceIoControl(hVol, FSCTL_CREATE_USN_JOURNAL, 
			&cujd, sizeof(cujd), nullptr, 0, &br, nullptr)) {
			return hVol;
		}
	}

	return INVALID_HANDLE_VALUE;
}

bool LocalSearch::getUsn(const std::string& volName) {
	int volID = volName[0] - 'A';
	HANDLE hVol;
	DWORD br;
	USN_JOURNAL_DATA usnInfo;

	hVol = initUsn(volName);
	if (INVALID_HANDLE_VALUE == hVol) {
		return false;
	}

	// 获取 USN日志基本信息 
	if (DeviceIoControl(hVol, FSCTL_QUERY_USN_JOURNAL, 
		nullptr, 0, &usnInfo, sizeof(usnInfo), &br, nullptr)) {
		// 枚举USN日志文件中的所有记录
		MFT_ENUM_DATA med;
		med.StartFileReferenceNumber = 0;
		med.LowUsn = 0;
		med.HighUsn = usnInfo.NextUsn;

		char buf[USN_PAGE_SIZE];
		DWORD usnDataSize = 0;
		PUSN_RECORD usnRecord = 0;
		memset(buf, 0, sizeof(buf));

		while (0 != DeviceIoControl(hVol, FSCTL_ENUM_USN_DATA, 
			&med, sizeof(med), buf, USN_PAGE_SIZE, &usnDataSize, nullptr)) {

			DWORD dwRetBytes = usnDataSize - sizeof(USN);
			// 找到第一个USN记录
			usnRecord = (PUSN_RECORD)(((PCHAR)buf) + sizeof(USN));
			
			while (dwRetBytes > 0) {
				// 保存记录
				std::wstring wsFileName(usnRecord->FileName, usnRecord->FileNameLength / 2);
				vecFileFrn.push_back(
					std::unique_ptr<FileFrn>(
						new FileFrn(wsFileName, usnRecord->ParentFileReferenceNumber, volName[0])));
				umFrnMapIdx[volID][usnRecord->FileReferenceNumber] = vecFileFrn.size() - 1;

				// 获取下一个记录
				dwRetBytes -= usnRecord->RecordLength;
				usnRecord = (PUSN_RECORD)(((PCHAR)usnRecord) + usnRecord->RecordLength);
			} 
			
			med.StartFileReferenceNumber = *(USN*)&buf;
		}
	}

	// 删除USN日志文件（也可以保留）
	if (hVol != INVALID_HANDLE_VALUE) {
		DELETE_USN_JOURNAL_DATA dujd;
		dujd.UsnJournalID = usnInfo.UsnJournalID;
		dujd.DeleteFlags = USN_DELETE_FLAG_DELETE; 
		
		DeviceIoControl(hVol, FSCTL_DELETE_USN_JOURNAL, 
			&dujd, sizeof(dujd), nullptr, 0, &br, nullptr);
		
		CloseHandle(hVol);
	}

	return true;
}

void LocalSearch::scanAllVol() {
	DWORD dwDrives = GetLogicalDrives();
	if (dwDrives <= 0) {
		return;
	}

	char volName[] = "C:\\";
	for (int i = 0; i < MAX_VOLUME; ++i) {
		if (dwDrives & (1 << i)) {
			volName[0] = 'A' + i;
			getUsn(volName);
		}
	}
}

std::wstring LocalSearch::getPathFromFileFrn(const FileFrn& fileFrn) {
	std::wstring path(L"C:");
	path[0] = fileFrn.volName;

	if (ROOT_FRN == fileFrn.pFrn) {
		path.append(L"\\");
		path.append(fileFrn.wsFileName);
		return path;
	}
 
	std::vector<int> temp;
	uint64_t pFrn = fileFrn.pFrn;
	int curIdx = 0, preIdx = 0, volID = fileFrn.volName - 'A';

	while (pFrn != ROOT_FRN) {
		preIdx = curIdx;
		curIdx = umFrnMapIdx[volID][pFrn];

		// $ 开头的文件的frn和pfrn 可能相同
		if (curIdx == preIdx) {
			break;
		}

		temp.push_back(curIdx);
		pFrn = vecFileFrn[curIdx]->pFrn;
	}

	for (int i = temp.size() - 1; i >= 0; --i) {
		path.append(L"\\");
		path.append(vecFileFrn[temp[i]]->wsFileName);
	}

	path.append(L"\\");
	path.append(fileFrn.wsFileName);

	return path;
}

std::shared_ptr<FileInfo> LocalSearch::getFileInfoFromFileFrn(const FileFrn& fileFrn) {
	std::shared_ptr<FileInfo> fileInfo(new FileInfo);
	fileInfo->wsFileName = fileFrn.wsFileName;
	fileInfo->wsFilePath = getPathFromFileFrn(fileFrn);
	return fileInfo;
}