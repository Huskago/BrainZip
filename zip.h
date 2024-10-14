#ifndef ZIP_H
#define ZIP_H

int compressFiles(const char* output_filename, const char** input_files, int file_count);
int decompressFile(const char* input_filename);

#endif //ZIP_H
