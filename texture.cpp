#include "texture.hpp"

#include <png++/png.hpp>

#include <cmath>

#include "utils.hpp"

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
    // TODO: Fix alignment (move by 0.5f * pixelsize, probably)

    float x = pos.x * xsize - 0.5f;
    float y = pos.y * ysize - 0.5f;
    float ix0f, iy0f;
    float fx = std::modf(x, &ix0f);
    float fy = std::modf(y, &iy0f);
    int ix0 = ix0f;
    int iy0 = iy0f;
    int ix1 = (ix0 != xsize - 1)? ix0 + 1 : ix0;
    int iy1 = (iy0 != ysize - 1)? iy0 + 1 : iy0;
    if(ix0 == -1) ix0 = 0;
    if(iy0 == -1) iy0 = 0;
    Color c00 = data[iy0 * xsize + ix0];
    Color c01 = data[iy0 * xsize + ix1];
    Color c10 = data[iy1 * xsize + ix0];
    Color c11 = data[iy1 * xsize + ix1];

    if(debug) std::cout << "x " << x << " y " << y << std::endl;
    if(debug) std::cout << "fx " << fx  << " fy " << fy << std::endl;
    if(debug) std::cout << "ix1 " << ix1  << " iy1 " << iy1 << std::endl;
    if(debug) std::cout << "xsize " << xsize  << " ysize " << ysize << std::endl;

    fy = 1.0f - fy;
    fx = 1.0f - fx;

    Color c0s = fx * c00 + (1.0f - fx) * c01;
    Color c1s = fx * c10 + (1.0f - fx) * c11;

    if(debug) std::cout << c00 << std::endl;
    if(debug) std::cout << c01 << std::endl;
    if(debug) std::cout << c10 << std::endl;
    if(debug) std::cout << c11 << std::endl;

    Color css = fy * c0s + (1.0f - fy) * c1s;

    return css;
}

inline float clamp( float f )
{
    return 0.5f * ( 1.0f + fabsf( f ) - fabsf( f - 1.0f ) );
}

void Texture::WriteToPNG(std::string path){

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

void Texture::WriteToBMP(std::string path){
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
