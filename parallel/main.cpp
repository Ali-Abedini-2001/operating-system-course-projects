#include <iostream>
#include <fstream>
#include <vector>
#include <pthread.h>
#include <thread>

using namespace std;

#pragma pack(1)

typedef int LONG;
typedef unsigned short WORD;
typedef unsigned int DWORD;

typedef struct tagBITMAPFILEHEADER
{
    WORD bfType;
    DWORD bfSize;
    WORD bfReserved1;
    WORD bfReserved2;
    DWORD bfOffBits;
} BITMAPFILEHEADER, *PBITMAPFILEHEADER;


typedef struct tagBITMAPINFOHEADER
{
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    WORD biPlanes;
    WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER;

typedef struct Pixel{
    int red;
    int green;
    int blue;
    Pixel(int r, int g, int b){
        red = max(0, min(255,r));
        green = max(0, min(255,g));
        blue = max(0, min(255,b));
    }
    Pixel(){
        red = 0;
        green = 0;
        blue = 0;
    }
} Pixel;

int Kernel[3][3] = {
        {0, -1, 0},
        {-1, 5, -1},
        {0, -1, 0}
};

struct Args{
    int col;
    int cols;
    int rows;
    int row;
    int row2;
    int extra;
    int xtr2;
    vector<vector<Pixel>>* image;
    vector<vector<Pixel>>* image2;
    char* buffer;
};

bool fillAndAllocate(char *&buffer, const char *fileName, int &rows, int &cols, int &bufferSize) {
    std::ifstream file(fileName);

    if (!file) {
        cout << "File" << fileName << " doesn't exist!" << endl;
        return false;
    }
    file.seekg(0, std::ios::end);
    std::streampos length = file.tellg();
    file.seekg(0, std::ios::beg);

    buffer = new char[length];
    file.read(&buffer[0], length);

    PBITMAPFILEHEADER file_header;
    PBITMAPINFOHEADER info_header;

    file_header = (PBITMAPFILEHEADER) (&buffer[0]);
    info_header = (PBITMAPINFOHEADER) (&buffer[0] + sizeof(BITMAPFILEHEADER));
    rows = info_header->biHeight;
    cols = info_header->biWidth;
    bufferSize = file_header->bfSize;
    return true;
}

inline Pixel get_pixel(const char* buffer, int red_index){
    return {
            int((unsigned char)buffer[red_index]),
            int((unsigned char)buffer[red_index-1]),
            int((unsigned char)buffer[red_index-2]),
    };
}

void* bounded_getPixelsFromBMP24(void* arguments){
    struct Args *args;
    args = (struct Args *) arguments;
    int extra = args->xtr2; // col offset
    int idx = args->extra; // index
    int cols = args->cols;
    int row = args->row;
    int row2 = args->row2;
    vector<vector<Pixel>>* image = args->image;
    for (int i = row; i < row2; i++) {
        idx -= extra;
        for (int j = cols - 1; j >= 0; j--) {
            for (int k = 0; k < 3; k++) {
                idx--;
                auto c = (unsigned char) args->buffer[idx];
                switch (k) {
                    case 0:
                        (*image)[i][j].red = c;
                        break;
                    case 1:
                        (*image)[i][j].green = c;
                        break;
                    case 2:
                        (*image)[i][j].blue = c;
                        break;
                }

            }
        }
    }
    return nullptr;
}

vector<vector<Pixel>> getPixelsFromBMP24(int end, int& rows, int& cols, char *fileReadBuffer, int threadCount) {
    int index = end;
    int extra = cols % 4;
    vector<vector<Pixel>> picture(rows, vector<Pixel>(cols));
    int dx = (3*cols + extra) * rows;
    while(dx % threadCount != 0)threadCount--;
    int offset = end - dx, portion = rows / threadCount;
    pthread_t threads[threadCount];
    Args args[threadCount];
    int st = 0;

    for (int i = 0; i < threadCount; i++) {
        args[i].col = 0;
        args[i].cols = cols;
        args[i].rows = rows;
        args[i].row = st;
        st += portion;
        args[i].row2 = st;
        args[i].extra = index;
        args[i].xtr2 = extra;
        args[i].image = &picture;
        args[i].buffer = fileReadBuffer;
        pthread_create(&threads[i], nullptr, bounded_getPixelsFromBMP24, &args[i]);
        index -= (3*cols + extra) * portion;
    }

    for (int i = 0; i < threadCount; i++) {
        pthread_join(threads[i], nullptr);
    }

    return picture;
}

void* bounded_writeOutBmp24(void* arguments) {
    struct Args *args;
    args = (struct Args *) arguments;
    int extra = args->xtr2; // col offset
    int idx = args->extra; // index
    int cols = args->cols;
    int row = args->row;
    int row2 = args->row2;
    vector<vector<Pixel>> *image = args->image;
    for (int i = row; i < row2; i++) {
        idx -= extra;
        for (int j = cols - 1; j >= 0; j--) {
            for (int k = 0; k < 3; k++) {
                idx--;
                switch (k) {
                    case 0:
                        args->buffer[idx] = char(args->image->at(i)[j].red);
                        break;
                    case 1:
                        args->buffer[idx] = char(args->image->at(i)[j].green);
                        break;
                    case 2:
                        args->buffer[idx] = char(args->image->at(i)[j].blue);
                        break;
                }
            }
        }
    }
    return nullptr;
}

void writeOutBmp24(char *fileBuffer, const char *nameOfFileToCreate, int bufferSize, int& rows, int& cols, vector<vector<Pixel>>& image, int threadCount) {
    std::ofstream write(nameOfFileToCreate);
    if (!write) {
        cout << "Failed to write " << nameOfFileToCreate << endl;
        return;
    }
    int index = bufferSize;
    int extra = cols % 4;
    int dx = (3*cols + extra) * rows;
    while(dx % threadCount != 0)threadCount--;
    int offset = bufferSize - dx, portion = rows / threadCount;
    pthread_t threads[threadCount];
    Args args[threadCount];
    int st = 0;

    for (int i = 0; i < threadCount; i++) {
        args[i].col = 0;
        args[i].cols = cols;
        args[i].rows = rows;
        args[i].row = st;
        st += portion;
        args[i].row2 = st;
        args[i].extra = index;
        args[i].xtr2 = extra;
        args[i].image = &image;
        args[i].buffer = fileBuffer;
        pthread_create(&threads[i], nullptr, bounded_writeOutBmp24, &args[i]);
        index -= (3*cols + extra) * portion;
    }

    for (int i = 0; i < threadCount; i++) {
        pthread_join(threads[i], nullptr);
    }
    write.write(fileBuffer, bufferSize);
}

void* horizontal_mirror_column(void* arguments) {
    struct Args *args;
    args = (struct Args *) arguments;
    for (int row = args->row; row < args->row2; row++)
        for (int j = 0; j < args->col/2; j++) {
            Pixel temp = (*args->image)[row][j];
            (*args->image)[row][j] = (*args->image)[row][args->col - j - 1];
            (*args->image)[row][args->col - j - 1] = temp;
        }
    return nullptr;
}

void horizontal_mirror(vector<vector<Pixel>>& image, int& rows, int& cols, int thread_count) {
    auto *threads = new pthread_t[thread_count];
    int ratio = rows / thread_count;
    auto *args = new Args[thread_count];
    for (int i = 0; i < thread_count; i++) {
        args[i].image = &image;
        args[i].col = cols;
        int start = i * ratio;
        int end = (i + 1) * ratio;
        if (i == thread_count - 1) end = rows;
        args[i].row = start, args[i].row2 = end;
        pthread_create(&threads[i], nullptr, reinterpret_cast<void *(*)(void *)>(horizontal_mirror_column),
                       (void *) &args[i]);
    }
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], nullptr);
    }
    delete[] threads;
    delete[] args;
}

void vertical_mirror_thread(std::vector<std::vector<Pixel>>& image, int start_row, int end_row) {
    for (int i = start_row; i < end_row; i++) {
        int width = image[i].size();
        for (int j = 0; j < width / 2; j++) {
            std::swap(image[i][j], image[i][width - j - 1]);
        }
    }
}

void vertical_mirror(std::vector<std::vector<Pixel>>& image, int& rows, int& cols, int thread_count) {
    int rows_per_thread = rows / thread_count;
    std::vector<std::thread> threads;

    for (int i = 0; i < thread_count; i++) {
        int start_row = i * rows_per_thread;
        int end_row = (i == thread_count - 1) ? rows : (i + 1) * rows_per_thread;
        threads.emplace_back(vertical_mirror_thread, std::ref(image), start_row, end_row);
    }

    for (auto& t : threads) {
        t.join();
    }

    // Reverse the order of the rows in the image
    for (int i = 0; i < rows / 2; i++) {
        std::swap(image[i], image[rows - i - 1]);
    }
}


Pixel kernel_convolution(vector<vector<Pixel>>& image, int row, int col){
    int r=0, g=0, b=0;
    for(int i=-1; i<=1; i++){
        for(int j=-1; j<=1; j++){
            r += image[row+i][col+j].red * Kernel[i+1][j+1];
            g += image[row+i][col+j].green * Kernel[i+1][j+1];
            b += image[row+i][col+j].blue * Kernel[i+1][j+1];
        }
    }
    return {r, g, b};
}

void* bounded_convolution(void* arguments) {
    struct Args *args;
    args = (struct Args *) arguments;
    for (int row = args->row; row < args->row2; row++) {
        if(row == 0)
            continue;
        for (int j = 1; j < args->col - 1; j++) {
            (*args->image2)[row][j] = kernel_convolution(*args->image, row, j);
        }
    }
    return nullptr;
}

void edge_copy(vector<vector<Pixel>>& s_image, vector<vector<Pixel>>& r_image, int& rows, int& cols) {
    for(int t=0; t<rows; t++) {
        r_image[t][0] = s_image[t][0];
        r_image[t][cols-1] = s_image[t][cols-1];
    }
    for(int t=0; t<cols; t++) {
        r_image[0][t] = s_image[0][t];
        r_image[rows-1][t] = s_image[rows-1][t];
    }
}

void convolute_with_kernel(vector<vector<Pixel>>& image, int& rows, int& cols, int threads_count)
{
    vector<vector<Pixel>> temp(rows, vector<Pixel>(cols));
    auto *threads = new pthread_t[threads_count];
    int ratio = rows / threads_count;
    auto *args = new Args[threads_count];

    for (int i = 0; i < threads_count; i++) {
        args[i].image = &image;
        args[i].image2 = &temp;
        args[i].col = cols;
        int start = i * ratio;
        if (start == 0) start = 1;
        int end = (i + 1) * ratio;
        if (i == threads_count - 1) end = rows-1;
        args[i].row = start, args[i].row2 = end;
        pthread_create(&threads[i], nullptr, reinterpret_cast<void *(*)(void *)>(bounded_convolution),
                       (void *) &args[i]);
    }

    edge_copy(image, temp, rows, cols);
    for (int i = 0; i < threads_count; i++) {
        pthread_join(threads[i], nullptr);
    }
    image = temp;

    delete[] threads;
    delete[] args;
}

void* sepiaFilter_bounded(void* arguments) {
    struct Args *args;
    args = (struct Args *) arguments;
    for (int i = args->row; i < args->row2; i++) {
        for (int j = 0; j < args->cols; j++) {
            int b = (*args->image)[i][j].blue;
            int g = (*args->image)[i][j].green;
            int r = (*args->image)[i][j].red;

            int sepiaBlue = (int)min((0.272 * r) + (0.534 * g) + (0.131 * b), 255.0);
            int sepiaGreen = (int)min((0.349 * r) + (0.686 * g) + (0.168 * b), 255.0);
            int sepiaRed = (int)min((0.393 * r) + (0.769 * g) + (0.189 * b), 255.0);

            (*args->image)[i][j].blue = sepiaBlue;
            (*args->image)[i][j].green = sepiaGreen;
            (*args->image)[i][j].red = sepiaRed;
        }
    }
    return nullptr;
}

void sepiaFilter(vector<vector<Pixel>>& image, int& rows, int& cols, int thread_count) {
    auto *threads = new pthread_t[thread_count];
    int ratio = rows / thread_count;
    auto *args = new Args[thread_count];
    for (int i = 0; i < thread_count; i++) {
        args[i].image = &image;
        args[i].cols = cols;
        int start = i * ratio;
        int end = (i + 1) * ratio;
        if (i == thread_count - 1) end = rows;
        args[i].row = start, args[i].row2 = end;
        pthread_create(&threads[i], nullptr, reinterpret_cast<void *(*)(void *)>(sepiaFilter_bounded),
                       (void *) &args[i]);
    }
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], nullptr);
    }
    delete[] threads;
    delete[] args;
}

void* x_mark_bounded(void* arguments){
    struct Args *args;
    args = (struct Args *) arguments;
    for(int r=args->row; r<args->row2; r++)
    {
        for(int c=0; c<args->cols; c++)
        {
            if(r == c || r + c == args->rows - 1)
            {
                (*args->image)[r][c].red = 255;
                (*args->image)[r][c].green = 255;
                (*args->image)[r][c].blue = 255;
            }
        }
    }
    return nullptr;
}

void x_mark(vector<vector<Pixel>>& image, int& rows, int& cols, int threads_count)
{
    auto *threads = new pthread_t[threads_count];
    int ratio = rows / threads_count;
    auto *args = new Args[threads_count];
    for (int i = 0; i < threads_count; i++) {
        args[i].image = &image;
        args[i].cols = cols;
        args[i].rows = rows;
        int start = i * ratio;
        int end = (i + 1) * ratio;
        if (i == threads_count - 1) end = rows;
        args[i].row = start, args[i].row2 = end;
        pthread_create(&threads[i], nullptr, reinterpret_cast<void *(*)(void *)>(x_mark_bounded),
                       (void *) &args[i]);
    }
    for (int i = 0; i < threads_count; i++) {
        pthread_join(threads[i], nullptr);
    }
    delete[] threads;
    delete[] args;
}

int main(int argc, char *argv[])
{
    char *fileBuffer;
    int bufferSize;
    int rows, cols;
    if (argc < 2)
    {
        cout << "Please provide an input file name" << endl;
        return 0;
    }
    char *fileName = argv[1];
    auto start_time = std::chrono::high_resolution_clock::now();
    if (!fillAndAllocate(fileBuffer, fileName, rows, cols, bufferSize))
    {
        cout << "File read error" << endl;
        return 1;
    }

    auto image = getPixelsFromBMP24(bufferSize, rows, cols, fileBuffer, 8);

    horizontal_mirror(image, rows, cols, 6);
    vertical_mirror(image, rows, cols, 6);
    convolute_with_kernel(image, rows, cols, 8);
    sepiaFilter(image, rows, cols, 4);
    x_mark(image, rows, cols, 4);

    writeOutBmp24(fileBuffer, "output.bmp", bufferSize, rows, cols, image, 8);

    auto end_time = std::chrono::high_resolution_clock::now();
    cout << "Execution Time: " << std::chrono::duration_cast<std::chrono::microseconds>(end_time-start_time).count() << endl;
    pthread_exit(nullptr);
}