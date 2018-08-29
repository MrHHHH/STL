//SGI STL空间配置器的模拟实现 

#pragma once
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <iostream>
using namespace std;
// Trace 跟踪

#define __DEBUG__

FILE* fOut = fopen("trace.log", "w");

static string GetFileName(const string& path)
{
	char ch = '/';

#ifdef _WIN32
	ch = '\\';
#endif

	size_t pos = path.rfind(ch);
	if (pos == string::npos)
		return path;
	else
		return path.substr(pos + 1);
}
// 用于调试追溯的trace log
inline static void __trace_debug(const char* function,
	const char * filename, int line, char* format, ...)
{
	// 读取配置文件
#ifdef __DEBUG__
	// 输出调用函数的信息
	fprintf(stdout, "【 %s:%d】%s", GetFileName(filename).c_str(), line, function);
	fprintf(fOut, "【 %s:%d】%s", GetFileName(filename).c_str(), line, function);

	// 输出用户打的trace信息
	va_list args;
	va_start(args, format);
	vfprintf(stdout, format, args);
	vfprintf(fOut, format, args);
	va_end(args);
#endif
}

#define __TRACE_DEBUG(...)  \
	__trace_debug(__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__);

/////////////////////////////////////////////////////////////////////////
//一级空间配置器

//参数inst为预留，暂时不用
template<int inst>
class __MallocAllocTemplate
{
private:
	//处理内存不足的情况的函数
	//oom: out of memory
	static void *OomMalloc(size_t);
	static void *OomRealloc(void *, size_t);
	
	//分配内存失败时处理函数的句柄函数指针 
	static void(*__mallocAllocOomHandler)();
public:

	//空间分配:一级空间配置器直接使用malloc
	static void *Allocate(size_t n)
	{
		__TRACE_DEBUG("(n:%u)\n", n);
		void *result = malloc(n);
		if (result == 0)
			result = OomMalloc(n);
		return result;
	}

	//空间释放：一级空间配置器直接使用free
	static void Deallocate(void *p, size_t /*n*/)
	{
		__TRACE_DEBUG("(p:%p)\n", p);
		free(p);
	}

	//重新分配空间：一级空间配置器直接调用realloc
	static void *Reallocate(void *p, size_t /*oldSize*/, size_t newSize)
	{
		void *result = realloc(p, newSize);
		if (result == 0)
			result = OomRealloc(p, newSize);
		return result;
	}

	
	static void(*SetMallocHandler(void *f()))()
	{
		void(*old)() = __mallocAllocOomHandler;
		__mallocAllocOomHandler = f;
		return old;
	}
};

template<int inst>
void(*__MallocAllocTemplate<inst>::__mallocAllocOomHandler)() = 0;

template<int inst>
void *__MallocAllocTemplate<inst>::OomMalloc(size_t n)
{
	void(*myMallocHandler)();
	void *result;

	while (1)  //一直尝试 释放，开辟，释放，再开辟....
	{
		myMallocHandler = __mallocAllocOomHandler;
		if (myMallocHandler == 0)
		{
			cerr << "out of memory" << endl;
			exit(1);
		}

		(*myMallocHandler)(); //企图释放内存
		result = malloc(n);   //开辟内存
		if (result)
			return result;
	}
}

template<int inst>
void *__MallocAllocTemplate<inst>::OomRealloc(void *p, size_t n)
{
	void(*myMallocHandler)();
	void * result;
	while (1)
	{
		myMallocHandler = __mallocAllocOomHandler;
		if (myMallocHandler == 0)
		{
			throw::bad_alloc;
		}

		(*myMallocHandler)();
		result = realloc(p, n);
		if (result)
			return result;
	}
}

typedef __MallocAllocTemplate<0> MallocAlloc;

/////////////////////////////////////////////////////////////////////////
//二级空间配置器

//匿名枚举，定义整数常量的另一种方式
enum {__ALIGN = 8}; //小型区块的上调边界（排列间隔）
enum {__MAX_BYTES = 128};   //区块的最大值
enum {__NFREELISTS = __MAX_BYTES/__ALIGN};  //free_list 的结点个数

//第一个参数用于多线程情况， 第二个参数预留，暂时不用
template<bool threads, int inst>
class __DefaultAllocTemplate
{
private:
	union Obj
	{
		union Obj *freeListLink;   //指向相同形式的另一个Obj
		char clientData[1];        //指向实际区块
	};

public:

	//空间分配
	static void *Allocate(size_t n) //n为字节数
	{
		__TRACE_DEBUG("(n: %u)\n", n);
		//1.分配区块大于__MAX_BYTES，调用一级空间配置器
		//2.否则，进入二级空间配置器中获取
		if (n > (size_t)__MAX_BYTES)
			return MallocAlloc::Allocate(n);
		
		//进入二级空间配置器
		//1.如果对应自由链表中的位置没有空间，用Refill填充
		//2.如果自由链表中有，则直接返回一个结点块内存
		//ps:如果为多线程，需要进行加锁
		size_t index = FreeListIndex(n);
		Obj* head = freeList[index];
		
		if (head == NULL)
		{
			//没有找到可用的freeList,重新填充freeList
			return Refill(ROUND_UP(n));
		}
		else
		{
			__TRACE_DEBUG("自由链表取内存:_freeList[%d]\n", index);

			freeList[index] = head->freeListLink;  //头删自由链表
			return head;
		}
	}

	//空间回收
	static void Deallocate(void *p, size_t n)  //n为字节数
	{
		__TRACE_DEBUG("(p:%p, n: %u)\n", p, n);
		//1.回收区块大于__MAX_BYTES,调用一级空间配置器
		//2.否则，调用二级空间配置器
		if (n > (size_t)__MAX_BYTES)
			MallocAlloc::Deallocate(p, n);
		
		//进入二级空间配置器
		//ps:多线程环境需要考虑加锁
		size_t index = FreeListIndex(n);
		Obj *head = freeList[index];

		Obj *tmp = (Obj*)p;
		tmp->freeListLink = head; //头插自由链表
		freeList[index] = tmp;
	}

	//重新分配空间
	static void *Reallocate(void *p, size_t oldSize, size_t newSize)
	{
		//1.回收区块大于__MAX_BYTES,调用一级空间配置器
		//2.否则，调用二级空间配置器
		if (oldSize > (size_t)__MAX_BYTES && newSize > (size_t)__MAX_BYTES)
			MallocAlloc::Reallocate(p, oldSize, newSize);

		//进入二级空间配置器
		//1.如果新旧的大小在同一区间，则不需要重新分配，直接返回
		//2.否则，重新分配并拷贝内容
		if (ROUND_UP(oldSize) == ROUND_UP(newSize))
			return p;

		void *result = Allocate(newSize);
		memcpy(result, p, (oldSize <= newSize ? oldSize : newSize));
		Deallocate(p, oldSize);
		return result;
	}
private:
	//根据区块大小，选用freeList的位置，返回对应下标
	static size_t FreeListIndex(size_t bytes)
	{
		return (bytes + __ALIGN - 1) / __ALIGN - 1;
	}

	//获得内存插入freeList中。返回一个大小为n的对象，并可能加入大小为n的其他区块到freeList中
	static void *Refill(size_t n)
	{
		__TRACE_DEBUG("(n:%u)\n", n);
		int nobjs = 20;
		char* chunk = ChunkAlloc(n, nobjs);

		if (nobjs == 1)
			return chunk;

		Obj *result, *cur;
		result = (Obj*)chunk;  //保存要传回的内容
		//把剩余的空间链接到自由链表中
		cur = (Obj*)(chunk + n);
		size_t index = FreeListIndex(n);
		freeList[index] = cur;

		for (int i = 2; i <= nobjs; ++i)
		{
			cur->freeListLink = (Obj*)(chunk + n*i);
			cur = cur->freeListLink;
		}
		cur->freeListLink = NULL;
		return result;
	}

	//获取大块内存，尝试取得nobjs个区块
	static char *ChunkAlloc(size_t size, int& nobjs)
	{
		__TRACE_DEBUG("(size: %u, nobjs: %d)\n", size, nobjs);

		size_t bytesNeed = size*nobjs;
		size_t bytesLeft = endFree - startFree;  //内存池剩余空间
		char *result;
		//1.内存池内存足够，直接从内存池中取(bytesLeft >= bytesNeed)
		//2.内存池内存不全够，但是至少够一个对象的大小(bytesLeft >= size)
		//3.内存池内存不足一个对象的大小，则从系统分配大块内存到内存池中(bytesLeft < size)

		if (bytesLeft >= bytesNeed)
		{
			__TRACE_DEBUG("内存池中内存足够分配%d个对象\n", nobjs);
			result = startFree;
			startFree += bytesNeed;
			return result;
		}
		else if (bytesLeft >= size)
		{
			__TRACE_DEBUG("内存池中内存不够分配%d个对象，只能分配%d个对象\n", nobjs, bytesLeft / size);
			result = startFree;
			nobjs = bytesLeft / size; //剩余空间可以分配对象的个数
			startFree += (nobjs*size);
			return result;
		}
		else
		{
			//如果内存池中还有小块内存，将它头插到合适的freeList
			if (bytesLeft > 0)
			{
				size_t index = FreeListIndex(bytesLeft);
				Obj *tmp = (Obj*)startFree;
				tmp->freeListLink = freeList[index];
				freeList[index] = tmp;
				startFree = NULL;
				__TRACE_DEBUG("将内存池中剩余的空间，分配给freeList[%d]\n", index);
			}

			//从系统分配大块内存
			size_t bytesToGet = bytesNeed * 2 + ROUND_UP(heapSize >> 4);
			startFree = (char*)malloc(bytesToGet);
			__TRACE_DEBUG("内存池空间不足，系统堆分配%u bytes内存\n", bytesToGet)
			//如果在系统堆中分配内存失败，则尝试到自由链表中更大的结点中分配
			if (startFree == NULL)
			{
				__TRACE_DEBUG("系统堆已无足够，无奈之下，只能到自由链表中看看\n");
				for (int i = size; i <= __MAX_BYTES; i += __ALIGN)
				{
					size_t index = FreeListIndex(i);
					Obj *head = freeList[index];

					if (head != NULL)
					{
						startFree = (char*)head;
						freeList[index] = head->freeListLink;
						endFree = startFree + i;
						return ChunkAlloc(size, nobjs);
					}
				}
				endFree = 0;
				//自由链表中也没有内存，则再到一级配置器中分配内存
				//因为一级配置器中可能设置了内存处理，有可能会分配到内存
				__TRACE_DEBUG("系统堆和自由链表都已无内存，一级配置器做最后一根稻草\n");
				startFree = (char*)MallocAlloc::Allocate(bytesToGet);
			}
			heapSize += bytesToGet;
			endFree = startFree + bytesToGet;
			return ChunkAlloc(size, nobjs);
		}
	}

	//将bytes上调到8的倍数
	static size_t ROUND_UP(size_t bytes)
	{
		return (bytes + __ALIGN - 1) & (~(__ALIGN - 1));
	}
private:
	static Obj *volatile freeList[__NFREELISTS];  //自由链表。volatile用来修饰不同线程访问和修改的变量
	static char *startFree;  //内存池的开始位置
	static char *endFree;    //内存池的结束位置
	static size_t heapSize;  //从系统堆分配的总大小
};

//初始化静态变量
template <bool threads, int inst>
char *__DefaultAllocTemplate<threads, inst>::startFree = 0;
template <bool threads, int inst>
char *__DefaultAllocTemplate<threads, inst>::endFree = 0;
template <bool threads, int inst>
size_t __DefaultAllocTemplate<threads, inst>::heapSize = 0;
template <bool threads, int inst>
typename __DefaultAllocTemplate<threads, inst>::Obj *volatile __DefaultAllocTemplate<threads, inst>::freeList[__NFREELISTS] = 
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

#   define __NODE_ALLOCATOR_THREADS true
#ifdef __USE_MALLOC
typedef MallocAlloc Alloc; //令alloc为一级空间配置器
#else
typedef __DefaultAllocTemplate<__NODE_ALLOCATOR_THREADS, 0> Alloc;//令Alloc为二级空间配置器
#endif // __USE_MALLOC

template<class T, class _Alloc>
class SimpleAlloc
{
public:
	static T *Allocate(size_t n)  //n为对象个数
	{
		return (n == 0) ? 0 : (T*)_Alloc::Allocate(n*sizeof(T));
	}

	static T *Allocate()
	{
		return (T*)_Alloc::Allocate(sizeof(T));
	}

	static void Deallocate(T *p, size_t n)
	{
		if (n != 0)
		{
			_Alloc::Deallocate(p, n*sizeof(T));
		}
	}

	static void Deallocate(T *p)
	{
		_Alloc::Deallocate(p, sizeof(T));
	}
};


//通过__TRACE_DEBUG做白盒测试

//测试特殊场景 
void TestAlloc1()
{
	cout<<"测试内存池空间不足分配"<<endl;   
	char *p1=SimpleAlloc<char,Alloc>::Allocate(8);
	char *p2 = SimpleAlloc<char, Alloc>::Allocate(8);
	cout << "测试内存池空间不足，系统堆进行分配" << endl;
	char *p3 = SimpleAlloc<char, Alloc>::Allocate(12); 
}

//测试系统堆内存耗尽的场景 
void TestAlloc2()
{
	cout<<"测试系统堆内存耗尽"<<endl;
	SimpleAlloc<char, Alloc>::Allocate(1024 * 1024 * 1024);    //SimpleAlloc<char, Alloc>::Allocate(1024*1024*1024);
	SimpleAlloc<char, Alloc>::Allocate(1024 * 1024);
//不好测试，说明系统管理小块内存的能力还是很强的。   
	for(int i= 0;i< 100000; ++i)    
	{
		char *p1=SimpleAlloc<char,Alloc>::Allocate(128);
	}
}
