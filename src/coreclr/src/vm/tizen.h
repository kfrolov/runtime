// Copyright (C) 2019 Samsung Electronics Co., Ltd.
// See the LICENSE file in the project root for more information.

#ifndef __TIZEN_H__
#define __TIZEN_H__

#include "method.hpp"

namespace Tizen
{
    // This function is called from src/vm/prestub.cpp, after some particular
    // class method is JIT-compiled.  The task of this function is to call
    // `inst_class' and `inst_func' from `struct Import' in right order, to make
    // possible to record which classes is loaded, how generic classes and functions
    // should be specialized, and which function is compiled now.
    void MethodPrepared(MethodDesc *);
};

#endif
