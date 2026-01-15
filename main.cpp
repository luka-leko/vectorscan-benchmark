#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <iostream>
#include "hs/hs.h"
#include "nlohmann/json.hpp"
#include <chrono>

int n_matches = 0;
static int eventHandler(unsigned int id, unsigned long long from, unsigned long long to,
                        unsigned int flags,
                        void* ctx) {  // cppcheck-suppress constParameterCallback
    n_matches += 1;
    printf("Match for pattern \"%s\" with id \"%d\" at offset %llu\n", ((char**)ctx)[id], id, to);
    return 0;
}

static char* readInputData(const char* inputFN, unsigned int* length) {
    FILE* f = fopen(inputFN, "rb");
    if (!f) {
        fprintf(stderr, "ERROR: unable to open file \"%s\": %s\n", inputFN, strerror(errno));
        return NULL;
    }

    // Get file size by:
    // 1. moving the file pointer to the end of the file
    if (fseek(f, 0, SEEK_END) != 0) {
        fprintf(stderr, "ERROR: unable to seek file \"%s\": %s\n", inputFN, strerror(errno));
        fclose(f);
        return NULL;
    }
    // 2. Capturing the relative position of the file pointer
    long dataLen = ftell(f);
    if (dataLen < 0) {
        fprintf(stderr, "ERROR: ftell() failed: %s\n", strerror(errno));
        fclose(f);
        return NULL;
    }
    // 3. Moving the file pointer back to the start of the file
    if (fseek(f, 0, SEEK_SET) != 0) {
        fprintf(stderr, "ERROR: unable to seek file \"%s\": %s\n", inputFN, strerror(errno));
        fclose(f);
        return NULL;
    }
    // Put a maximum on the file size (as the whole file is kept in memory at the same time)
    if ((unsigned long)dataLen > UINT_MAX) {
        dataLen = UINT_MAX;
        printf("WARNING: clipping data to %ld bytes\n", dataLen);
    } else if (dataLen == 0) {
        fprintf(stderr, "ERROR: input file \"%s\" is empty\n", inputFN);
        fclose(f);
        return NULL;
    }

    char* inputData = (char*)malloc(dataLen);
    if (!inputData) {
        fprintf(stderr, "ERROR: unable to malloc %ld bytes\n", dataLen);
        fclose(f);
        return NULL;
    }

    char* p = inputData;
    size_t bytesLeft = dataLen;
    while (bytesLeft) {
        size_t bytesRead = fread(p, 1, bytesLeft, f);
        bytesLeft -= bytesRead;
        p += bytesRead;
        if (ferror(f) != 0) {
            fprintf(stderr, "ERROR: fread() failed\n");
            free(inputData);
            fclose(f);
            return NULL;
        }
    }

    fclose(f);

    *length = (unsigned int)dataLen;
    return inputData;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <queries file> <input file>\n", argv[0]);
        return -1;
    }

    std::ifstream ifs("data/patterns.json");
    // if (!ifs.is_)
    // nlohmann::json jf = json::parse(ifs);

    // Compile database
    const char* queries_filename = argv[1];
    const unsigned int elements = 3;
    const char* const expressions[elements] = {"does not exist", "also does not exist", "test"};
    const unsigned int flags[elements] = {HS_FLAG_SINGLEMATCH, HS_FLAG_SINGLEMATCH,
                                          HS_FLAG_SINGLEMATCH};
    const unsigned int ids[elements] = {0, 1, 2};
    hs_database_t* database;
    hs_compile_error_t* compile_err;
    if (hs_compile_multi(expressions, flags, ids, elements, HS_MODE_BLOCK, NULL, &database,
                         &compile_err) != HS_SUCCESS) {
        fprintf(stderr, "ERROR: Unable to compile patterns \"%s\": %s\n", expressions[0],
                compile_err->message);
        hs_free_compile_error(compile_err);
        return -1;
    }

    // Allocate scratch space
    hs_scratch_t* scratch = NULL;
    if (hs_alloc_scratch(database, &scratch) != HS_SUCCESS) {
        fprintf(stderr, "ERROR: Unable to allocate scratch space. Exiting.\n");
        hs_free_database(database);
        return -1;
    }

    // open the input file
    const char* input_filename = argv[2];
    std::ifstream file(input_filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << input_filename << std::endl;
        return 1;
    }

    const size_t BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    size_t n_lines = 0;
    size_t scanned_bytes = 0;
    std::cout << "Start scanning..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    while (file.getline(buffer, BUFFER_SIZE)) {
        n_lines++;
        size_t input_len = strlen(buffer);
        scanned_bytes += input_len + 1;
        if (hs_scan(database, buffer, input_len, 0, scratch, eventHandler, (void*)expressions) !=
        HS_SUCCESS) {
            fprintf(stderr, "ERROR: Unable to scan input buffer. Exiting.\n");
            hs_free_database(database);
            hs_free_scratch(scratch);
            return -1;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    size_t scanned_MB = scanned_bytes / 1024 / 1024;
    float duration_seconds = duration.count() / 1000.0;
    std::cout << "Scanned a total of " << scanned_MB << " MB across " << n_lines << " line scans in " << duration_seconds << " seconds." << std::endl;
    std::cout << "=> " << scanned_MB / duration_seconds << "MB/s scans." << std::endl;

    file.close();

    hs_free_scratch(scratch);
    hs_free_database(database);
    return 0;
}
