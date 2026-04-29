#include "pager.h"
#include <iostream>
#include <cstring>

Pager::Pager() : num_pages(0) {}

Pager::~Pager() {
    close();
}

void Pager::open(const std::string& path) {
    db_path = path;

    file.open(path, std::ios::in | std::ios::out | std::ios::binary);

    if (!file.is_open()) {
        // create file if not exists
        file.clear();
        file.open(path, std::ios::out | std::ios::binary);
        file.close();

        file.open(path, std::ios::in | std::ios::out | std::ios::binary);
    }

    // get file size
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();

    num_pages = (file_size + PAGE_SIZE - 1) / PAGE_SIZE;
}

std::vector<char>& Pager::getPage(size_t pageNum) {
    if (page_cache.find(pageNum) == page_cache.end()) {
        loadPage(pageNum);
    }
    return page_cache[pageNum];
}

void Pager::loadPage(size_t pageNum) {
    std::vector<char> page(PAGE_SIZE, 0);

    if (pageNum < num_pages) {
        file.seekg(pageNum * PAGE_SIZE, std::ios::beg);
        file.read(page.data(), PAGE_SIZE);
    }

    page_cache[pageNum] = page;
}

size_t Pager::allocatePage() {
    size_t new_page_num = num_pages;
    num_pages++;

    std::vector<char> page(PAGE_SIZE, 0);
    page_cache[new_page_num] = page;

    return new_page_num;
}

void Pager::flush(size_t pageNum) {
    if (page_cache.find(pageNum) == page_cache.end()) return;

    file.seekp(pageNum * PAGE_SIZE, std::ios::beg);
    file.write(page_cache[pageNum].data(), PAGE_SIZE);
    file.flush();
}

void Pager::close() {
    for (auto& [pageNum, _] : page_cache) {
        flush(pageNum);
    }

    if (file.is_open()) {
        file.close();
    }

    page_cache.clear();
}

size_t Pager::getPageCount() const {
    return num_pages;
}