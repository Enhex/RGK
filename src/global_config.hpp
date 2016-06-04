
#ifdef NDEBUG
#define ENABLE_DEBUG 0
#else
#define ENABLE_DEBUG 1
#endif

#define TILE_SIZE 50

#define PREVIEW_DIMENTIONS_RATIO 3
#define PREVIEW_RAYS_RATIO 2
#define PREVIEW_SPEED_RATIO (PREVIEW_DIMENTIONS_RATIO*PREVIEW_DIMENTIONS_RATIO*PREVIEW_RAYS_RATIO)

#define BARSIZE 40

// =======================================

#ifdef NDEBUG
  #undef ENABLE_DEBUG
  #define ENABLE_DEBUG 0
#endif

#if ENABLE_DEBUG
  #include <cassert>
  #define IFDEBUG if(debug)
#else
  #define IFDEBUG if(0)
  #undef assert
#define assert(x) (void)0
#endif

#if ENABLE_DEBUG
  #ifndef NO_EXTERN
    extern bool debug_trace;
    extern unsigned int debug_x, debug_y;
  #endif // NO_EXTERN
#else
  #ifndef NDEBUG
    #define NDEBUG
  #endif
#endif

#define NEAR(x,y) (x < y + 0.001f && x > y - 0.001f)
