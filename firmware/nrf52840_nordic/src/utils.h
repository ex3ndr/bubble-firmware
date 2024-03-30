#pragma once

#define ASSERT_OK(result)                                          \
    if ((result) < 0)                                              \
    {                                                              \
        printk("Error at %s:%d:%d\n", __FILE__, __LINE__, result); \
        return (result);                                           \
    }

#define ASSERT_TRUE(result)                                        \
    if (!result)                                                   \
    {                                                              \
        printk("Error at %s:%d:%d\n", __FILE__, __LINE__, result); \
        return -1;                                                 \
    }
