#include <iostream>
#include <unistd.h>
#include <fstream>
#include <vector>
#include <ctime>
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

bool fillAndAllocate(char *&buffer, const char *fileName, int &rows, int &cols, int &bufferSize) {
    std::ifstream file(fileName);

  if (file)
  {
    file.seekg(0, std::ios::end);
    std::streampos length = file.tellg();
    file.seekg(0, std::ios::beg);

    buffer = new char[length];
    file.read(&buffer[0], length);

    PBITMAPFILEHEADER file_header;
    PBITMAPINFOHEADER info_header;

    file_header = (PBITMAPFILEHEADER)(&buffer[0]);
    info_header = (PBITMAPINFOHEADER)(&buffer[0] + sizeof(BITMAPFILEHEADER));
    rows = info_header->biHeight;
    cols = info_header->biWidth;
    bufferSize = file_header->bfSize;
    return 1;
  }
  else
  {
    cout << "File" << fileName << " doesn't exist!" << endl;
    return 0;
  }
}

inline Pixel get_pixel(const char* buffer, int red_index){
    return {
        int((unsigned char)buffer[red_index]),
        int((unsigned char)buffer[red_index-1]),
        int((unsigned char)buffer[red_index-2]),
    };
}

vector<vector<Pixel>> getPixelsFromBMP24(int end, int& rows, int& cols, const char *fileReadBuffer) {
    int count = 1;
    int extra = cols % 4;
    vector<vector<Pixel>> image(rows, vector<Pixel>(cols));
    for (int i = 0; i < rows; i++) {
        count += extra;
        for (int j = cols - 1; j >= 0; j--) {
            for (int k = 0; k < 3; k++)
            {
                unsigned char channel = fileReadBuffer[end-count];
                count++;
                switch (k)
                {
                    case 0:
                        // fileReadBuffer[end - count] is the red value
                        image[i][j].red = int(channel);
                        break;
                    case 1:
                        // fileReadBuffer[end - count] is the green value
                        image[i][j].green = int(channel);
                        break;
                    case 2:
                        // fileReadBuffer[end - count] is the blue value
                        image[i][j].blue = int(channel);
                        break;
                        // go to the next position in the buffer
                }
            }
//            image[i][j] = get_pixel(fileReadBuffer, end - count);
//            count += 3;
        }
    }
    return image;
}

void writeOutBmp24(char *fileBuffer, const char *nameOfFileToCreate, int bufferSize, int& rows, int& cols, vector<vector<Pixel>>& image) {
    std::ofstream write(nameOfFileToCreate);
    if (!write) {
        cout << "Failed to write " << nameOfFileToCreate << endl;
        return;
    }
    int count = 0;
    int extra = cols % 4;
    for (int i = 0; i < rows; i++) {
        count += extra;
        for (int j = cols - 1; j >= 0; j--) {
            for (int k = 0; k < 3; k++) {
                count++;
                switch (k) {
                    case 0:
                        // write red value in fileBuffer[bufferSize - count]
                        fileBuffer[bufferSize - count] = char(image[i][j].red);
                        break;
                    case 1:
                        // write green value in fileBuffer[bufferSize - count]
                        fileBuffer[bufferSize - count] = char(image[i][j].green);
                        break;
                    case 2:
                        // write blue value in fileBuffer[bufferSize - count]
                        fileBuffer[bufferSize - count] = char(image[i][j].blue);
                        break;
                }
                // go to the next position in the buffer
            }
        }
    }
    write.write(fileBuffer, bufferSize);
}

void horizontal_mirror(vector<vector<Pixel>>& image, int& rows, int& cols)
{
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols / 2; j++)
        {
            Pixel temp = image[i][j];
            image[i][j] = image[i][cols - j - 1];
            image[i][cols - j - 1] = temp;
        }
    }
}

void vertical_mirror(vector<vector<Pixel>>& image, int& rows, int& cols)
{
    for (int i = 0; i < rows / 2; i++)
    {
        for (int j = 0; j < cols; j++)
        {
            Pixel temp = image[i][j];
            image[i][j] = image[rows - i - 1][j];
            image[rows - i - 1][j] = temp;
        }
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

void convolute_with_kernel(vector<vector<Pixel>>& image, int& rows, int& cols)
{
    vector<vector<Pixel>> temp(rows, vector<Pixel>(cols));
    for(int t=0; t<rows; t++) {
        temp[t][0] = image[t][0];
        temp[t][cols-1] = image[t][cols-1];
    }
    for(int t=0; t<cols; t++) {
        temp[0][t] = image[0][t];
        temp[rows-1][t] = image[rows-1][t];
    }

    for (int i = 1; i < rows - 1; i++)
        for (int j = 1; j < cols - 1; j++)
            temp[i][j] = kernel_convolution(image, i, j);
    image = temp;
}

void sepiaFilter(vector<vector<Pixel>>& image, int& rows, int& cols)
{
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            
            int b = image[i][j].blue;
            int g = image[i][j].green;
            int r = image[i][j].red;

            
            int sepiaRed = (int)min((0.393 * r) + (0.769 * g) + (0.189 * b), 255.0);
            int sepiaGreen = (int)min((0.349 * r) + (0.686 * g) + (0.168 * b), 255.0);
            int sepiaBlue = (int)min((0.272 * r) + (0.534 * g) + (0.131 * b), 255.0);
            

            image[i][j].blue = sepiaBlue;
            image[i][j].green = sepiaGreen;
            image[i][j].red = sepiaRed;
        }
    }
}

void drawX(vector<vector<Pixel>>& image, int& rows, int& cols)
{
    for (int i = 0; i < min(rows, cols); i++) {
        image[i][i].red = 255;
        image[i][i].green = 255;
        image[i][i].blue = 255;
    }

    for (int i = 0; i < min(rows, cols); i++) {
        image[i][cols - i - 1].red = 225;
        image[i][cols - i - 1].green = 255;
        image[i][cols - i - 1].blue = 255;
    }
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

    auto image = getPixelsFromBMP24(bufferSize, rows, cols, fileBuffer);

    horizontal_mirror(image, rows, cols);
    vertical_mirror(image, rows, cols);
    convolute_with_kernel(image, rows, cols);
    sepiaFilter(image, rows, cols);
    drawX(image, rows, cols);

    writeOutBmp24(fileBuffer, "output.bmp", bufferSize, rows, cols, image);

    auto end_time = std::chrono::high_resolution_clock::now();
    cout << "Execution Time: " << std::chrono::duration_cast<std::chrono::microseconds>(end_time-start_time).count() << endl;
    
    return 0;
}