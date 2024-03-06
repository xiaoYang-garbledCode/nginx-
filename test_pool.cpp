#include"ngx_palloc.h"
#include<iostream>

struct Data
{
	char* ptr;
	FILE* pfile;
};

void func1(void* p)
{
	char* p1 = (char*)p;
	std::cout << "free ptr mem!" << std::endl;
	free(p1);
}

void func2(void* pf)
{
	FILE* pf1 = (FILE*)pf;
	std::cout << "close file!" << std::endl;
	fclose(pf1);
}

int main()
{
	ngx_mem_pool pool;
	if (pool.ngx_create_pool(512) == nullptr)
	{
		std::cout << "create pool failed!" << std::endl;
	}
	void* p1 = pool.ngx_palloc(128);
	Data* data2 = (Data*)pool.ngx_palloc(512);

	if(p1==nullptr)
	{
		std::cout << "ngx_palloc 128  failed!" << std::endl;
	}
	if (data2 == nullptr)
	{
		std::cout << "ngx_palloc 512 failed!" << std::endl;
	}
	// 在开辟的内存空间中有指向外部资源的指针
	data2->ptr = (char*)malloc(12);
	strcpy(data2->ptr, "hello world");
	data2->pfile = fopen("data.txt", "w");

	// 将自定义的资源回收函数传入
	ngx_pool_cleanup_s* c1 = pool.ngx_pool_cleanup_add(sizeof(char*));
	c1->handler = func1;
	c1->data = data2->ptr;
	ngx_pool_cleanup_s* c2 = pool.ngx_pool_cleanup_add(sizeof(FILE*));
	c2->handler = func2;
	c2->data = data2->pfile;
	pool.ngx_destroy_pool();
}