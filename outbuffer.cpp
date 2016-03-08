#include "outbuffer.hpp"

#include <png++/png.hpp>

OutBuffer::OutBuffer(int xsize, int ysize):
    xsize(xsize), ysize(ysize)
{
    data.resize(xsize*ysize);
}

void OutBuffer::SetPixel(int x, int y, Color c)
{
    data[y*xsize + x] = c;
}

void OutBuffer::WriteToPNG(std::string path){

    png::image<png::rgb_pixel> image(xsize, ysize);

    for (png::uint_32 y = 0; y < image.get_height(); ++y){
        for (png::uint_32 x = 0; x < image.get_width(); ++x){
            auto c = data[y*xsize + x];
            auto px = png::rgb_pixel(255.0*c.r,255.0*c.g,255.0*c.b);
            image[y][x] = px;
        }
    }
    image.write(path);
}

inline float clamp( float f )
{
    return 0.5f * ( 1.0f + fabsf( f ) - fabsf( f - 1.0f ) );
}

void OutBuffer::WriteToBMP(std::string path){
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
