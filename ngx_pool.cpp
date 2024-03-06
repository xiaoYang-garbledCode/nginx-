#include"ngx_palloc.h"
#include <iostream>
// ������СΪsize���ڴ��,����С���ڴ�ز�����һ��ҳ���С
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

// �����ֽڶ��룬���ڴ������size��С���ڴ�
void* ngx_mem_pool::ngx_palloc(size_t size)
{
	
	if (size <= pool_->max)
	{
		// �����sizeС��max
		return ngx_palloc_small(size, 1);
	}
	return ngx_palloc_large(size);
}

// �������ֽڶ��룬���ڴ������size��С���ڴ�
void* ngx_mem_pool::ngx_pnalloc(size_t size)
{
	if (size <= pool_->max)
	{
		return ngx_palloc_small(size, 0);
	}
	return ngx_palloc_large(size);
}

// ����ngx_palloc������ʼ��Ϊ0
void* ngx_mem_pool::ngx_pcalloc(size_t size)
{
	void* p = ngx_palloc(size);
	if (p) {
		memset(p, 0, size);
	}
	return p;
}

// ���Դ��ڴ������size��С���ڴ棬�ڴ治������Ҫ��ϵͳ���뿪���ڴ档
void* ngx_mem_pool::ngx_palloc_small(size_t size, ngx_uint_t align)
{
	u_char* m;
	ngx_pool_s* p;
	p = pool_->current; // ��һ���ɷ���С���ڴ��С���ڴ��ַ
	//ʹ��do while��ԭ��ʱ������տ�ʼp��Ϊ�գ�ҲҪȷ������һ�����ѭ��������û���
	do {
		m = (u_char*)p->d.last; // ����С���ڴ����ʼ��ַ
		if (align)
		{
			m = ngx_align_ptr(m, NGX_POOL_ALIGNMENT); // ��m�ϵ���NGX_POOL_ALIGNMENT�ı���
		}
		if ((size_t)(p->d.end - m) >= size)
			//��ǰ�ڴ�ɹ�����size
		{
			p->d.last = m + size;
			return m;
		}
		//��ǰ�ڴ治�����䣬������Ѱ��
		p = p->d.next;
	} while (p);//һֱ���㹻��С���ڴ棬ֱ��pΪ��ʱ��˵��û���ҵ����ʴ�С���ڴ�

	// ��ʱ��ϵͳ���봴��size��С���ڴ�
	return ngx_palloc_block(size);
}

// �Ӳ���ϵͳ�����µ�С���ڴ�ء� ngx_palloc_small����ngx_palloc_block
void* ngx_mem_pool::ngx_palloc_block(size_t size)
{
	u_char* m;  // ���ڽ��տ��ٵ��ڴ��ַ
	ngx_pool_s* p,*newblock; // p�����ж�failed������ newtempָ���¿��ٵ��ڴ��ͷ����Ϣ

	// �����ȥС�ڴ��ͷ��Ϣ���С���ڴ��Сpsize
	size_t psize = size_t(pool_->d.end - sizeof(ngx_pool_s));
	
	//��psize���а�NGX_POOL_ALIGNMENT�����ڴ濪��
	m = (u_char*)malloc(psize);
	if (m == nullptr)
	{
		return nullptr;
	}
	// ����С���ڴ�أ���end,next,failed,last���и�ֵ
	newblock = (ngx_pool_s*)m;
	newblock->d.end = m + psize;
	newblock->d.failed = 0;
	newblock->d.next = nullptr;
	m = m + sizeof(ngx_pool_data_s); // ��ʱ��m��ʾ��ȥС�ڴ�ͷ��Ϣ�󣬿ɷ���С�ڵ����last
	m = ngx_align_ptr(m, NGX_POOL_ALIGNMENT); // ��m����
	newblock->d.last = m + size; // �����ȥsize

	//֮ǰ���еĴ�������4������ʹ������ڴ���п��٣��޸�����currentָ�룬С��4ҲҪ��fail����1
	for (p = pool_; p->d.next; p = p->d.next)
	{
		if (p->d.failed++ > 4)
		{
			p->current = p->d.next;
		}
	}
	p->d.failed++;
	// ������´������ڴ��ҵ�С�ڴ����Ϣ��������һ�����
	p->d.next = newblock;

	return m; // �����ȥsize����ʼ��ַ
}

// size��С����max����Ҫ���ٴ���ڴ�
void* ngx_mem_pool::ngx_palloc_large(size_t size)
{
	u_char* m;
	int count = 3;
	ngx_pool_large_s* p = pool_->large;
	// �����СΪsize���ڴ�
	m = (u_char*)malloc(size);
	// ����ʧ�ܷ���nullptr
	if (m == nullptr)
	{
		return nullptr;
	}
	// �ҵ����ʵ�λ�ù��ϣ���pool_��ʼ������������㣬
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
	// �������large����Ϊ�գ����С���ڴ���з���ngx_pool_large_s��С���ڴ����洢����ڴ��ͷ��Ϣ
	ngx_pool_large_s* newlarge = (ngx_pool_large_s*)ngx_palloc_small(sizeof(ngx_pool_large_s), 1);
	// �ڴ濪��ʧ�ܷ��ؿ�
	if (newlarge == nullptr)
	{
		free(m); // ���տ��ٵĴ��ڴ�飬��Ϊ����ͷ����Ϣû�еط��档
		return nullptr;
	}
	newlarge->alloc = m;
	// ������ͷ�巨�����һ��ngx_pool_large_s��next����
	newlarge->next = pool_->large;
	pool_->large = newlarge;
	return m;
}

// �ͷŴ���ڴ�
void ngx_mem_pool::ngx_pfree(void* p)
{
	// ����large������ͷ�alloc
	for (ngx_pool_large_s* p = pool_->large; p; p = p->next)
	{
		if (p->alloc)
		{
			free(p->alloc);
			p->alloc = nullptr;
		}
	}
}


// �����ڴ��
void ngx_mem_pool::ngx_destroy_pool()
{
	// �ͷ��ⲿ��Դ
	ngx_pool_cleanup_s* c = pool_->cleanup;
	for (; c; c = c->next)
	{
		if (c->handler)
		{
			c->handler(c->data); // �����Զ����ⲿ��Դ���պ���
		}
	}

	// �ͷŴ��ڴ��
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
	// �ͷ�С�ڴ��
	for (p1 = pool_, n = pool_->d.next; ;p1 = n , p1 = p1->d.next)
	{
		free(p1);
		if (n == nullptr)
		{
			break;
		}
	}
}

// �����ڴ��
void ngx_mem_pool::ngx_reset_pool()
{
	// !������Դ����Ҫ�ͷ��ⲿ��Դ
	
	// �ͷŴ���ڴ�
	ngx_pool_large_s* p = pool_->large;
	for (; p; p = p->next)
	{
		if (p->alloc)
		{
			free(p->alloc);
			p->alloc = nullptr;
		}
	}

	// ����С���ڴ�  ����ÿ���ڴ�أ���last�ûؿɷ���С���ڴ����ʼλ��
	// ��һ����������Ŀ��lastλ��������
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