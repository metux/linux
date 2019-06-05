// SPDX-License-Identifier: GPL-2.0
//
// find unncessary cases of unlikely(IS_ERR(foo))
// IS_ERR() already calls unlikely() call
//
// Copyright (C) 2019 Enrico Weigelt, metux IT consult <info@metux.net>
//
virtual patch
virtual context
virtual org
virtual report

@@
expression E;
@@
- unlikely(IS_ERR(E))
+ IS_ERR(E)

@@
expression E;
@@
- unlikely(IS_ERR_OR_NULL(E))
+ IS_ERR_OR_NULL(E)

@@
expression E;
@@
- likely(!IS_ERR(E))
+ !IS_ERR(E)

@@
expression E;
@@
- likely(!IS_ERR_OR_NULL(E))
+ !IS_ERR_OR_NULL(E)
