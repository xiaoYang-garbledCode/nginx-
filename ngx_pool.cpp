#include"ngx_palloc.h"
#include <iostream>
// 创建大小为size的内存池,但是小块内存池不超过一个页面大小
void* ngx_mem_pool::ngx_create_pool(size_t size)
{
	ngx_pool_s* p;
	p = (ngx_pool_s*)malloc(size);
	if (p == nullptr)
	{
		return nullptr;
	}
	p->d.last = (u_char*)p + sizeof(ngx_pool_s);
	p->d.end = (u_char*)p + size;
	p->d.next = nullptr;
	p->d.failed = 0;

	size = size - sizeof(ngx_pool_s);
	p->max = (size < NGX_MAX_ALLOC_FROM_POOL) ? size : NGX_MAX_ALLOC_FROM_POOL;
	p->current = p;
	p->large = nullptr;
	p->cleanup = nullptr;
	pool_ = p;
	return p;
}

// 考虑字节对齐，从内存池申请size大小的内存
void* ngx_mem_pool::ngx_palloc(size_t size)
{
	
	if (size <= pool_->max)
	{
		// 申请的size小于max
		return ngx_palloc_small(size, 1);
	}
	return ngx_palloc_large(size);
}

// 不考虑字节对齐，从内存池申请size大小的内存
void* ngx_mem_pool::ngx_pnalloc(size_t size)
{
	if (size <= pool_->max)
	{
		return ngx_palloc_small(size, 0);
	}
	return ngx_palloc_large(size);
}

// 调用ngx_palloc，并初始化为0
void* ngx_mem_pool::ngx_pcalloc(size_t size)
{
	void* p = ngx_palloc(size);
	if (p) {
		memset(p, 0, size);
	}
	return p;
}

// 尝试从内存池中拿size大小的内存，内存不够则需要向系统申请开辟内存。
void* ngx_mem_pool::ngx_palloc_small(size_t size, ngx_uint_t align)
{
	u_char* m;
	ngx_pool_s* p;
	p = pool_->current; // 第一个可分配小块内存的小块内存地址
	//使用do while的原因时，如果刚开始p就为空，也要确保进入一次这个循环？？？没理解
	do {
		m = (u_char*)p->d.last; // 可用小块内存的起始地址
		if (align)
		{
			m = ngx_align_ptr(m, NGX_POOL_ALIGNMENT); // 将m上调至NGX_POOL_ALIGNMENT的倍数
		}
		if ((size_t)(p->d.end - m) >= size)
			//当前内存可供分配size
		{
			p->d.last = m + size;
			return m;
		}
		//当前内存不够分配，向后继续寻找
		p = p->d.next;
	} while (p);//一直找足够大小的内存，直到p为空时，说明没有找到合适大小的内存

	// 此时向系统申请创建size大小的内存
	return ngx_palloc_block(size);
}

// 从操作系统开辟新的小块内存池。 ngx_palloc_small调用ngx_palloc_block
void* ngx_mem_pool::ngx_palloc_block(size_t size)
{
	u_char* m;  // 用于接收开辟的内存地址
	ngx_pool_s* p,*newblock; // p用于判断failed遍历， newtemp指向新开辟的内存池头部信息

	// 计算除去小内存池头信息后的小块内存大小psize
	size_t psize = size_t(pool_->d.end - sizeof(ngx_pool_s));
	
	//将psize进行按NGX_POOL_ALIGNMENT进行内存开辟
	m = (u_char*)malloc(psize);
	if (m == nullptr)
	{
		return nullptr;
	}
	// 创建小块内存池，对end,next,failed,last进行赋值
	newblock = (ngx_pool_s*)m;
	newblock->d.end = m + psize;
	newblock->d.failed = 0;
	newblock->d.next = nullptr;
	m = m + sizeof(ngx_pool_data_s); // 此时的m表示除去小内存头信息后，可分配小内的起点last
	m = ngx_align_ptr(m, NGX_POOL_ALIGNMENT); // 将m对齐
	newblock->d.last = m + size; // 分配出去size

	//之前所有的次数大于4，则不再使用这块内存进行开辟，修改它的current指针，小于4也要把fail加上1
	for (p = pool_; p->d.next; p = p->d.next)
	{
		if (p->d.failed++ > 4)
		{
			p->current = p->d.next;
		}
	}
	p->d.failed++;
	// 把这个新创建的内存块挂到小内存块信息链表的最后一个结点
	p->d.next = newblock;

	return m; // 分配出去size的起始地址
}

// size大小大于max，需要开辟大块内存
void* ngx_mem_pool::ngx_palloc_large(size_t size)
{
	u_char* m;
	int count = 3;
	ngx_pool_large_s* p = pool_->large;
	// 分配大小为size的内存
	m = (u_char*)malloc(size);
	// 分配失败返回nullptr
	if (m == nullptr)
	{
		return nullptr;
	}
	// 找到合适的位置挂上，从pool_开始最多遍历三个结点，
	while (p && count > 0)
	{
		if (p->alloc == nullptr)
		{
			p->alloc = m;
			return m;
		}
		p = p->next;
		count--;
	}
	// 三个结点large都不为空，则从小块内存池中分配ngx_pool_large_s大小的内存来存储大块内存的头信息
	ngx_pool_large_s* newlarge = (ngx_pool_large_s*)ngx_palloc_small(sizeof(ngx_pool_large_s), 1);
	// 内存开辟失败返回空
	if (newlarge == nullptr)
	{
		free(m); // 回收开辟的大内存块，因为它的头部信息没有地方存。
		return nullptr;
	}
	newlarge->alloc = m;
	// 并利用头插法插入第一个ngx_pool_large_s的next域中
	newlarge->next = pool_->large;
	pool_->large = newlarge;
	return m;
}

// 释放大块内存
void ngx_mem_pool::ngx_pfree(void* p)
{
	// 遍历large，逐个释放alloc
	for (ngx_pool_large_s* p = pool_->large; p; p = p->next)
	{
		if (p->alloc)
		{
			free(p->alloc);
			p->alloc = nullptr;
		}
	}
}


// 销毁内存池
void ngx_mem_pool::ngx_destroy_pool()
{
	// 释放外部资源
	ngx_pool_cleanup_s* c = pool_->cleanup;
	for (; c; c = c->next)
	{
		if (c->handler)
		{
			c->handler(c->data); // 调用自定义外部资源回收函数
		}
	}

	// 释放大内存块
	ngx_pool_large_s* p = pool_->large;
	ngx_pool_s* p1,*n;

	for (; p; p = p->next)
	{
		if (p->alloc)
		{
			free(p->alloc);
			p->alloc = nullptr;
		}
	}
	// 释放小内存块
	for (p1 = pool_, n = pool_->d.next; ;p1 = n , p1 = p1->d.next)
	{
		free(p1);
		if (n == nullptr)
		{
			break;
		}
	}
}

// 重置内存池
void ngx_mem_pool::ngx_reset_pool()
{
	// !重置资源不需要释放外部资源
	
	// 释放大块内存
	ngx_pool_large_s* p = pool_->large;
	for (; p; p = p->next)
	{
		if (p->alloc)
		{
			free(p->alloc);
			p->alloc = nullptr;
		}
	}

	// 重置小块内存  遍历每个内存池，将last置回可分配小块内存的起始位置
	// 第一个块与后续的块的last位置有区别
	pool_->d.last = (u_char*)pool_ + sizeof(ngx_pool_s);
	pool_->d.failed = 0;
	for (ngx_pool_s* p1 = pool_->d.next; p1; p1 = p1->d.next)
	{
		p1->d.last = (u_char*)p1 + sizeof(ngx_pool_data_s);
		p1->d.failed = 0;
	}
}

ngx_pool_cleanup_s* ngx_mem_pool::ngx_pool_cleanup_add(size_t size)
{
	ngx_pool_cleanup_s* c;

	c = (ngx_pool_cleanup_s*)ngx_palloc(sizeof(ngx_pool_cleanup_s));
	if (c == nullptr)
	{
		return nullptr;
	}

	if (size)
	{
		c->data = ngx_palloc(size);
		if (c->data == nullptr)
		{
			return nullptr;
		}
	}
	else
	{
		c->data = nullptr;
	}
	c->handler = nullptr;
	c->next = pool_->cleanup;
	pool_->cleanup = c;
	return c;
}