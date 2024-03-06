#pragma once

using u_char = unsigned char;
using ngx_uint_t = unsigned int;

using ngx_pool_cleanup_t = void(*)(void* data);



// �ⲿ��Դͷ��Ϣ
struct ngx_pool_cleanup_s 
{
    ngx_pool_cleanup_t   handler; // �Զ����ⲿ��Դ���պ���
    void* data; // ��Ҫ���յ�����
    ngx_pool_cleanup_s* next; // �ⲿ��Դ��Ϣ������һ��������
};


// ����ڴ�ͷ��Ϣ
struct ngx_pool_large_s
{
    ngx_pool_large_s* next; // ����ڴ���Ϣ������һ��������
    void* alloc; // ����ڴ��ַ���
};
struct ngx_pool_s;

// С���ڴ�ͷ��Ϣ
 struct ngx_pool_data_s 
 {
    u_char* last;  // С���ڴ���ʼ��ַ
    u_char* end;   // С���ڴ�ĩ��ַ
    ngx_pool_s* next; // С���ڴ汻����һ��������
    ngx_uint_t  failed; // С���ڴ����ʧ�ܴ���
} ;
 
 // ngxͷ��Ϣ������Ա��Ϣ
 struct ngx_pool_s {
     ngx_pool_data_s  d;  //�洢��ǰС���ڴ��ʹ�����
     size_t  max; // С���ڴ�ʹ���ڴ�ķֽ���
     ngx_pool_s* current; // ��һ���ɷ����С���ڴ��С���ڴ��ַ
     ngx_pool_large_s*  large;   // ����ڴ�������Ϣ
     ngx_pool_cleanup_s* cleanup; // ���������������ڵ�ַ
 };
 // һ��ҳ��Ĵ�С
 const int ngx_pagesize = 4096;
 // С���ڴ�����ֵ���ܳ���һ��ҳ��
 const int NGX_MAX_ALLOC_FROM_POOL = ngx_pagesize - 1;
 //Ĭ�ϴ������ڴ�ش�СΪ16K
 const int  NGX_DEFAULT_POOL_SIZE = 16 * 1024;

 // ����Ϊ16��������
 const int  NGX_POOL_ALIGNMENT = 16;

 // ��d�ϵ���a�ı���
#define ngx_align(d, a)   (((d) + (a - 1)) & ~(a - 1))

 // �ڴ�ش����������Сsize
 const int   NGX_MIN_POOL_SIZE =
     ngx_align((sizeof(ngx_pool_s) + 2 * sizeof(ngx_pool_large_s)),NGX_POOL_ALIGNMENT);
 // ��ָ��ptr�ϵ���a�ı���
#define ngx_align_ptr(ptr, a) \
    (u_char*)(((uintptr_t)(ptr) + ((uintptr_t)a - 1)) & ~((uintptr_t)a - 1))
class ngx_mem_pool
{
public:
    // ������СΪsize���ڴ��,����С���ڴ�ز�����һ��ҳ���С
    void* ngx_create_pool(size_t size);

    // �����ڴ��
    void ngx_destroy_pool();

    // �ڴ������ڴ��
    void ngx_reset_pool();

    // �����ֽڶ��룬���ڴ������size��С���ڴ�
    void* ngx_palloc(size_t size);
    // �������ֽڶ��룬���ڴ������size��С���ڴ�
    void* ngx_pnalloc(size_t size);
    // ����ngx_palloc������ʼ��Ϊ0
    void* ngx_pcalloc(size_t size);

    //���Դ��ڴ������size��С���ڴ棬�ڴ治������Ҫ��ϵͳ���뿪���ڴ档
    void* ngx_palloc_small(size_t size, ngx_uint_t align);

    // �Ӳ���ϵͳ�����µ�С���ڴ�ء� ngx_palloc_small����ngx_palloc_block
    void* ngx_palloc_block(size_t size);

    //size��С����max����Ҫ���ٴ���ڴ�
    void* ngx_palloc_large(size_t size);

    // �ͷŴ���ڴ�
    void ngx_pfree(void* p);

    ngx_pool_cleanup_s* ngx_pool_cleanup_add(size_t size);

private:
    ngx_pool_s* pool_;
};