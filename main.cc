#include "localsearch.h"

int main() {
	std::locale::global(std::locale(""));
	std::wcout.imbue(std::locale());

	LocalSearch ls;
	ls.init();

	wchar_t temp[MAX_PATH+1] = { 0 };
	std::wstring wsPattern;

	while (true) {
		std::cout << "input:" << std::endl;
		std::wcin.getline(temp, MAX_PATH);
		wsPattern.assign(temp);

		ls.search(wsPattern);
		std::vector<std::shared_ptr<FileInfo>> result = ls.getResult(0, 10);

		std::cout << "--------------------------------------------------------" << std::endl;
		for (auto x : result) {
			std::wcout << x->wsFileName << L" ---> " << x->wsFilePath << std::endl;
		}
		std::cout << "--------------------------------------------------------" << std::endl;
	}

	return 0;
}