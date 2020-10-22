/* C/Fortran interface is different on different machines. 
 * You may need to tweak this.
 */

#ifndef _WTIME_H_
#define _WTIME_H_

#if defined(IBM)
#define wtime wtime
#elif defined(CRAY)
#define wtime WTIME
#else
#define wtime wtime_
#endif


#include<nautilus/libccompat.h>




#endif
