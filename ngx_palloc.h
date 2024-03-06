#pragma once

using u_char = unsigned char;
using ngx_uint_t = unsigned int;

using ngx_pool_cleanup_t = void(*)(void* data);



// 外部资源头信息
struct ngx_pool_cleanup_s 
{
    ngx_pool_cleanup_t   handler; // 自定义外部资源回收函数
    void* data; // 需要回收的数据
    ngx_pool_cleanup_s* next; // 外部资源信息被串在一条链表上
};


// 大块内存头信息
struct ngx_pool_large_s
{
    ngx_pool_large_s* next; // 大块内存信息被串在一条链表上
    void* alloc; // 大块内存地址入口
};
struct ngx_pool_s;

// 小块内存头信息
 struct ngx_pool_data_s 
 {
    u_char* last;  // 小块内存起始地址
    u_char* end;   // 小块内存末地址
    ngx_pool_s* next; // 小块内存被串在一条链表上
    ngx_uint_t  failed; // 小块内存分配失败次数
} ;
 
 // ngx头信息与管理成员信息
 struct ngx_pool_s {
     ngx_pool_data_s  d;  //存储当前小块内存的使用情况
     size_t  max; // 小块内存和大块内存的分界线
     ngx_pool_s* current; // 第一个可分配的小块内存的小块内存地址
     ngx_pool_large_s*  large;   // 大块内存的入口信息
     ngx_pool_cleanup_s* cleanup; // 所有清理操作的入口地址
 };
 // 一个页面的大小
 const int ngx_pagesize = 4096;
 // 小块内存的最大值不能超过一个页面
 const int NGX_MAX_ALLOC_FROM_POOL = ngx_pagesize - 1;
 //默认创建的内存池大小为16K
 const int  NGX_DEFAULT_POOL_SIZE = 16 * 1024;

 // 对齐为16的整数倍
 const int  NGX_POOL_ALIGNMENT = 16;

 // 将d上调至a的倍数
#define ngx_align(d, a)   (((d) + (a - 1)) & ~(a - 1))

 // 内存池创建允许的最小size
 const int   NGX_MIN_POOL_SIZE =
     ngx_align((sizeof(ngx_pool_s) + 2 * sizeof(ngx_pool_large_s)),NGX_POOL_ALIGNMENT);
 // 将指针ptr上调至a的倍数
#define ngx_align_ptr(ptr, a) \
    (u_char*)(((uintptr_t)(ptr) + ((uintptr_t)a - 1)) & ~((uintptr_t)a - 1))
class ngx_mem_pool
{
public:
    // 创建大小为size的内存池,但是小块内存池不超过一个页面大小
    void* ngx_create_pool(size_t size);

    // 销毁内存池
    void ngx_destroy_pool();

    // 内存重置内存池
    void ngx_reset_pool();

    // 考虑字节对齐，从内存池申请size大小的内存
    void* ngx_palloc(size_t size);
    // 不考虑字节对齐，从内存池申请size大小的内存
    void* ngx_pnalloc(size_t size);
    // 调用ngx_palloc，并初始化为0
    void* ngx_pcalloc(size_t size);

    //尝试从内存池中拿size大小的内存，内存不够则需要向系统申请开辟内存。
    void* ngx_palloc_small(size_t size, ngx_uint_t align);

    // 从操作系统开辟新的小块内存池。 ngx_palloc_small调用ngx_palloc_block
    void* ngx_palloc_block(size_t size);

    //size大小大于max，需要开辟大块内存
    void* ngx_palloc_large(size_t size);

    // 释放大块内存
    void ngx_pfree(void* p);

    ngx_pool_cleanup_s* ngx_pool_cleanup_add(size_t size);

private:
    ngx_pool_s* pool_;
};