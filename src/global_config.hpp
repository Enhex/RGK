
#define ENABLE_DEBUG 0

#define TILE_SIZE 50

#define PREVIEW_DIMENTIONS_RATIO 3
#define PREVIEW_RAYS_RATIO 2
#define PREVIEW_SPEED_RATIO (PREVIEW_DIMENTIONS_RATIO*PREVIEW_DIMENTIONS_RATIO*PREVIEW_RAYS_RATIO)

#define BARSIZE 40

// =======================================

#ifdef NDEBUG
  #define ENABLE_DEBUG 0
#endif

#if ENABLE_DEBUG
  #define IFDEBUG if(debug)
#else
  #define IFDEBUG if(0)
#endif

#if ENABLE_DEBUG
  #ifndef NO_EXTERN
    extern bool debug_trace;
    extern unsigned int debug_x, debug_y;
  #endif // NO_EXTERN
#else
  #define NDEBUG
#endif
