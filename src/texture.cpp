#include "texture.hpp"

#include <png++/png.hpp>
#include <jpeglib.h>
#include <jerror.h>

#include <cmath>

#include "utils.hpp"

#include <glm/gtx/wrap.hpp>

Texture::Texture(int xsize, int ysize):
    xsize(xsize), ysize(ysize)
{
    data.resize(xsize*ysize);
}

void Texture::SetPixel(int x, int y, Color c)
{
    data[y*xsize + x] = c;
}


Color Texture::GetPixelInterpolated(glm::vec2 pos, bool debug) const{
    (void)debug;

    float x = glm::repeat(pos.x) * xsize - 0.5f;
    float y = glm::repeat(pos.y) * ysize - 0.5f;
    float ix0f, iy0f;
    float fx = std::modf(x, &ix0f);
    float fy = std::modf(y, &iy0f);
    int ix0 = ix0f;
    int iy0 = iy0f;
    int ix1 = (ix0 != int(xsize) - 1)? ix0 + 1 : ix0;
    int iy1 = (iy0 != int(ysize) - 1)? iy0 + 1 : iy0;
    if(ix0 == -1) ix0 = 0;
    if(iy0 == -1) iy0 = 0;
    Color c00 = data[iy0 * xsize + ix0];
    Color c01 = data[iy0 * xsize + ix1];
    Color c10 = data[iy1 * xsize + ix0];
    Color c11 = data[iy1 * xsize + ix1];

    /*
    if(debug) std::cout << "x " << x << " y " << y << std::endl;
    if(debug) std::cout << "fx " << fx  << " fy " << fy << std::endl;
    if(debug) std::cout << "ix1 " << ix1  << " iy1 " << iy1 << std::endl;
    if(debug) std::cout << "xsize " << xsize  << " ysize " << ysize << std::endl;
    */

    fy = 1.0f - fy;
    fx = 1.0f - fx;

    Color c0s = fx * c00 + (1.0f - fx) * c01;
    Color c1s = fx * c10 + (1.0f - fx) * c11;

    /*
    if(debug) std::cout << c00 << std::endl;
    if(debug) std::cout << c01 << std::endl;
    if(debug) std::cout << c10 << std::endl;
    if(debug) std::cout << c11 << std::endl;
    */

    Color css = fy * c0s + (1.0f - fy) * c1s;

    return css;
}

float Texture::GetSlopeRight(glm::vec2 pos){
    int x = glm::repeat(pos.x) * xsize - 0.5f;
    int y = glm::repeat(pos.y) * ysize - 0.5f;
    int x2 = (x != int(xsize) - 1)? x + 1 : x;
    if(x == -1) x = 0;
    if(y == -1) y = 0;
    Color  here = data[y * xsize + x];
    Color there = data[y * xsize + x2];
    float a = ( here.r +  here.g +  here.b)/3;
    float b = (there.r + there.g + there.b)/3;
    return a-b;
};
float Texture::GetSlopeBottom(glm::vec2 pos){
    int x = glm::repeat(pos.x) * xsize - 0.5f;
    int y = glm::repeat(pos.y) * ysize - 0.5f;
    int y2 = (y != int(ysize) - 1)? y + 1 : y;
    if(x == -1) x = 0;
    if(y == -1) y = 0;
    Color  here = data[y * xsize + x];
    Color there = data[y2 * xsize + x];
    float a = ( here.r +  here.g +  here.b)/3;
    float b = (there.r + there.g + there.b)/3;
    return a-b;
}

inline float clamp( float f )
{
    return 0.5f * ( 1.0f + fabsf( f ) - fabsf( f - 1.0f ) );
}

bool Texture::Write(std::string path) const{
    std::string out_dir = Utils::GetDir(path);
    std::string out_file = Utils::GetFilename(path);

    auto fname = Utils::GetFileExtension(out_file);
    if(fname.second == "BMP" || fname.second == "bmp"){
        WriteToBMP(path);
    }else if(fname.second == "PNG" || fname.second == "png"){
        WriteToPNG(path);
    }else{
        std::cerr << "Sorry, output file format '" << fname.second << "' is not supported." << std::endl;
        return false;
    }
    return true;
}

void Texture::WriteToPNG(std::string path) const{

    png::image<png::rgb_pixel> image(xsize, ysize);

    for (png::uint_32 y = 0; y < image.get_height(); ++y){
        for (png::uint_32 x = 0; x < image.get_width(); ++x){
            auto c = data[y*xsize + x];
            auto px = png::rgb_pixel(255.0*clamp(c.r),
                                     255.0*clamp(c.g),
                                     255.0*clamp(c.b));
            image[y][x] = px;
        }
    }
    image.write(path);
}

void Texture::WriteToBMP(std::string path) const{
    // This procedure was contributed by Adam Malinowski and donated
    // to the public domain.

    unsigned int H = ysize;
    unsigned int W = xsize;

    std::ofstream ofbStream;
    ofbStream.open(path, std::ios::binary );
    if( !ofbStream.is_open() )
    {
        // TODO: report failure
        return;
    }

    char Header[54] = {
        'B', 'M',
        'x', 'x', 'x', 'x',
        0, 0, 0, 0, 54, 0, 0, 0, 40, 0, 0, 0,
        'x', 'x', 'x', 'x',
        'x', 'x', 'x', 'x',
        1, 0, 24, 0, 0, 0, 0, 0,
        'x', 'x', 'x', 'x',
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    unsigned int S = H * ( 3 * W + W % 4 );
    *((unsigned int*) ( Header +  2 ) ) = 54 + S;
    *((unsigned int*) ( Header + 18 ) ) = W;
    *((unsigned int*) ( Header + 22 ) ) = H;
    *((unsigned int*) ( Header + 34 ) ) = S;

    ofbStream.write( Header, 54 );

    for( int i = H-1; i >= 0; --i )
        {
            for( unsigned int j = 0; j < W; ++j )
                {
                    ofbStream << (char) ( 255 * clamp( data[ i * W + j ].b ) );
                    ofbStream << (char) ( 255 * clamp( data[ i * W + j ].g ) );
                    ofbStream << (char) ( 255 * clamp( data[ i * W + j ].r ) );
                }
            for( unsigned int j = 0; j < W % 4; ++j ) ofbStream << (char) 0;
        }

    ofbStream.close();
}

Texture* Texture::CreateNewFromPNG(std::string path){
    if(!Utils::GetFileExists(path)){
        std::cerr << "Failed to load texture '" << path << ", file does not exist." << std::endl;
        return nullptr;
    }
    png::image< png::rgb_pixel > image(path);
    unsigned int w = image.get_width(), h = image.get_height();

    std::cerr << "Opened image '" << path << "', " << w << "x" << h << std::endl;

    Texture* t = new Texture(w,h);
    for(unsigned int y = 0; y < h; y++){
        for(unsigned int x = 0; x < w; x++){
            auto pixel = image.get_pixel(x, y);
            Color c = Color(pixel.red/255.0f, pixel.green/255.0f, pixel.blue/255.0f);
            t->SetPixel(x, y, c);
        }
    }
    return t;
}

Texture* Texture::CreateNewFromJPEG(std::string path){
    if(!Utils::GetFileExists(path)){
        std::cerr << "Failed to load texture '" << path << ", file does not exist." << std::endl;
        return nullptr;
    }

    struct jpeg_decompress_struct info;
    struct jpeg_error_mgr err;

    FILE* file = fopen(path.c_str(), "rb");  //open the file

    info.err = jpeg_std_error(& err);
    jpeg_create_decompress(& info);
    if(!file) {
        std::cout << "Failed to read file `" << path << "`, ignoring this texture." << std::endl;
        return nullptr;
    }


    jpeg_stdio_src(&info, file);
    jpeg_read_header(&info, TRUE);
    jpeg_start_decompress(&info);

    int w = info.output_width;
    int h = info.output_height;
    int channels = info.num_components;
    if(channels == 3){
        unsigned char* buf = new unsigned char[w * h * 3];
        unsigned char * rowptr[1];
        while (info.output_scanline < info.output_height){
            // Enable jpeg_read_scanlines() to fill our jdata array
            rowptr[0] = (unsigned char *)buf +
                3* info.output_width * info.output_scanline;
            jpeg_read_scanlines(&info, rowptr, 1);
        }
        jpeg_finish_decompress(&info);
        fclose(file);

        Texture* t = new Texture(w,h);
        for(int y = 0; y < h; y++){
            for(int x = 0; x < w; x++){
                int p = (y * w + x) * 3;
                t->SetPixel(x, h-y-1, Color(buf[p+0]/255.0f,
                                            buf[p+1]/255.0f,
                                            buf[p+2]/255.0f));
            }
        }

        delete[] buf;
        return t;

    }else if(channels == 1){
        unsigned char* buf = new unsigned char[w * h];
        unsigned char * rowptr[1];
        while (info.output_scanline < info.output_height){
            // Enable jpeg_read_scanlines() to fill our jdata array
            rowptr[0] = (unsigned char *)buf +
                info.output_width * info.output_scanline;
            jpeg_read_scanlines(&info, rowptr, 1);
        }
        jpeg_finish_decompress(&info);
        fclose(file);

        Texture* t = new Texture(w,h);
        for(int y = 0; y < h; y++){
            for(int x = 0; x < w; x++){
                int p = (y * w + x);
                t->SetPixel(x, h-y-1, Color(buf[p+0]/255.0f,
                                            buf[p+0]/255.0f,
                                            buf[p+0]/255.0f));
            }
        }

        delete[] buf;
        return t;

    }else{
        std::cout << "Unsupported number of channels in JPEG file `" << path << "`, only 3-channel and 1-channel files are supported" << std::endl;
        return nullptr;
    }

}


void Texture::FillStripes(unsigned int size, Color a, Color b){
    for(unsigned int y = 0; y < ysize; y++){
        for(unsigned int x = 0; x < xsize; x++){
            unsigned int d = (x+y)%(size*2);
            if(d < size) SetPixel(x, y, a);
            else         SetPixel(x, y, b);
        }
    }
}
