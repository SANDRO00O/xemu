#pragma once

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>

#ifndef close
#define close _close
#endif

#ifndef open
#define open _open
#endif

#ifndef read
#define read _read
#endif

#ifndef lseek
#define lseek _lseeki64
#endif
#endif
