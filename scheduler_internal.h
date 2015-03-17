#pragma once

//
// Compatibility macros
//

// Missing from Arduino toolchain
#ifndef UINT16_MAX
#define UINT16_MAX 0xFFFF
#endif

// Missing from Arduino toolchain
#ifndef UINT32_MAX
#define UINT32_MAX 0xFFFFFFFF
#endif

// Just in case
#ifndef __has_builtin
  #define __has_builtin(x) 0
#endif
