/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2021, Michael A. Cuevas <cuevas@u.northwestern.edu>
 * Copyright (c) 2021, Peter A. Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2021, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Michael A. Cuevas <cuevas@u.northwestern.edu>
 *          Peter A. Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include "nautilus/signal.h"


/* DEBUG, INFO, ERROR PRINTS */
#ifndef NAUT_CONFIG_DEBUG_SIGNALS
#undef  DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif
#define SIGNAL_INFO(fmt, args...) INFO_PRINT("signal: " fmt, ##args)
#define SIGNAL_ERROR(fmt, args...) ERROR_PRINT("signal: " fmt, ##args)
#define SIGNAL_DEBUG(fmt, args...) DEBUG_PRINT("signal: " fmt, ##args)
#define SIGNAL_WARN(fmt, args...)  WARN_PRINT("signal: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("signal: " fmt, ##args)


