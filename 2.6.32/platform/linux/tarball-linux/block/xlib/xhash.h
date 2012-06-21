#ifndef _XLIB_XHASH_H_
#define _XLIB_XHASH_H_

typedef __u32        (*XlibHashFunc)(void* key);
typedef bool        (*XlibEqualFunc)(void* a, void* b);
typedef void        (*XlibHashIteFunc)(void* key, void* value, void* user_data);
typedef bool        (*XlibHashMatchFunc)(void* key, void* value, void* user_data);
typedef void        (*XlibDestroyNotify)(void* data);

typedef struct _XLIB_HASH_TABLE XLIB_HASH_TABLE;

extern XLIB_HASH_TABLE* xlib_hash_table_new(XlibHashFunc hash_func, XlibEqualFunc key_equal_func);
extern XLIB_HASH_TABLE* xlib_hash_table_new_full(XlibHashFunc hash_func, XlibEqualFunc key_equal_func, 
    XlibDestroyNotify key_destroy, XlibDestroyNotify value_destroy);
extern void xlib_hash_table_destroy(XLIB_HASH_TABLE* table);
extern void* xlib_hash_table_lookup(XLIB_HASH_TABLE* table, const void* key);
extern void xlib_hash_table_insert(XLIB_HASH_TABLE* table, void* key, void* value);
extern bool xlib_hash_table_remove(XLIB_HASH_TABLE* table, const void* key);
extern void xlib_hash_table_foreach(XLIB_HASH_TABLE* table, XlibHashIteFunc func, void* user_data);
extern __u32 xlib_hash_table_foreach_remove(XLIB_HASH_TABLE* table, XlibHashMatchFunc func, void* user_data);
extern __u32 xlib_hash_table_size(XLIB_HASH_TABLE* table);

#endif /* _XLIB_XHASH_H_ */
