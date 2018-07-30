/* ****************************************************************
 *
 * Copyright 2017 Samsung Electronics All Rights Reserved.
 *
 *
 *
 * Licensed under the Apache License, Version 2.0 = the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************/

#include "edge_malloc.h"
#include "edge_utils.h"

#include <stdio.h>
#include <string.h>

#define TAG "edge_malloc"

void *EdgeMalloc(size_t size)
{
    COND_CHECK((0 == size), NULL);
    return malloc(size);
}

void *EdgeCalloc(size_t num, size_t size)
{
    COND_CHECK((0 == size), NULL);
    COND_CHECK((0 == num), NULL);
    return calloc(num, size);
}

void *EdgeRealloc(void* ptr, size_t size)
{
    // Override realloc() behavior for NULL pointer which normally would
    // work as per malloc(), however we suppress the behavior of possibly
    // returning a non-null unique pointer.
    if (NULL == ptr)
    {
        return EdgeMalloc(size);
    }

    // Otherwise leave the behavior up to realloc() itself:
    return realloc(ptr, size);
}

void EdgeFree(void *ptr)
{
    VERIFY_NON_NULL_NR_MSG(ptr, "ptr is NULL\n");
    free(ptr);
}

Edge_String EdgeStringAlloc(char const src[])
{
    const Edge_String EDGE_STRING_NULL = {0, NULL};
    // Returns an empty Edge_String if input string is NULL.
    VERIFY_NON_NULL_MSG(src, "Input parameter(src) is NULL\n", EDGE_STRING_NULL);
    Edge_String str;
    str.length = strlen(src);
    if(str.length > 0) {
        str.data = (Edge_Byte*)malloc(str.length);
        // Returns an empty Edge_String if memory allocated fails.
        VERIFY_NON_NULL_MSG(str.data, "EdgeMalloc FAILED IN EdgeStringAlloc\n", EDGE_STRING_NULL);
        memcpy(str.data, src, str.length);
    } else {
        str.data = (Edge_Byte*)EDGE_EMPTY_ARRAY_SENTINEL;
    }
    return str;
}