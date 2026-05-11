#pragma once

#include<string>
#include<unordered_map>
#include<vector>
#include<fstream>

const size_t PAGE_SIZE = 4096;

class Pager{
    private:
        std::fstream file;
        std::string db_path;
        

        std::unordered_map<size_t, std::vector<char>> page_cache;

        size_t num_pages;

        void loadPage(size_t pageNum);
    
    public:
        Pager();
        ~Pager();

        void open(const std::string& path);

        std::vector<char>& getPage(size_t pageNum);

        size_t allocatePage();
        size_t getPageCount() const;

        void flush(size_t pageNum);
        void flushAll();

        void close();
};
